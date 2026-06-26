#!/usr/bin/env python3
"""uc3jtag - flash/debug an AT32UC3A3256 over JTAG with an Atmel-ICE, on macOS.

Why this exists
---------------
macOS has no tool that can drive an Atmel-ICE to program an AVR32 UC3 part:
OpenOCD only supports the AVR32 *AP7000* (not UC3) and ships no AVR32 flash
driver; avrdude/avarice are 8-bit AVR only; atprogram is Windows-only. This is
the gap that bricks you when the DFU bootloader is gone and JTAG is the only
way back in.

Approach
--------
We do NOT reimplement CMSIS-DAP or the JTAG state machine. OpenOCD already
drives this exact Atmel-ICE reliably in CMSIS-DAP + JTAG mode, so we run it as
a dumb transport and speak its Tcl-RPC protocol over a local TCP socket,
issuing raw `irscan`/`drscan`/`runtest`. On top of that we implement only the
AVR32-UC3-specific bits OpenOCD lacks.

Implemented milestones cover IDCODE, OCD register access, memory read/write,
FLASHC erase/program/verify, and CPU halt. Fuse/BOOTPROT handling is still not
implemented.

No third-party Python packages required (stdlib only).
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CFG = os.path.join(HERE, "openocd", "uc3a3.cfg")
TAP = "uc3.cpu"

# OpenOCD Tcl-RPC framing: every request and reply is terminated by this byte.
RPC_SENTINEL = b"\x1a"


class OpenOCDError(RuntimeError):
    pass


class OpenOCD:
    """Launches OpenOCD with the UC3 transport config and talks to its Tcl-RPC.

    Use as a context manager:
        with OpenOCD() as ocd:
            print(ocd.cmd("scan_chain"))
    """

    def __init__(self, cfg: str = DEFAULT_CFG, port: int = 6666,
                 openocd: str | None = None, verbose: bool = False):
        self.cfg = cfg
        self.port = port
        self.verbose = verbose
        self.openocd = openocd or shutil.which("openocd")
        if not self.openocd:
            raise OpenOCDError("openocd not found on PATH (brew install open-ocd)")
        # An explicit --openocd path that doesn't exist (or isn't a file) should
        # fail with a clear message, not a raw FileNotFoundError from Popen.
        if openocd and not (os.path.isfile(self.openocd)
                            and os.access(self.openocd, os.X_OK)):
            raise OpenOCDError(f"openocd not executable: {self.openocd}")
        if not os.path.exists(self.cfg):
            raise OpenOCDError(f"transport config not found: {self.cfg}")
        self.proc: subprocess.Popen | None = None
        self.sock: socket.socket | None = None
        # Receive buffer for Tcl-RPC: a single recv() may return more than one
        # sentinel-terminated reply (TCP coalescing) or split one across reads,
        # so we accumulate here and hand back exactly one reply per _raw() call.
        self._rxbuf = b""
        # OpenOCD output goes to a temp file (not a PIPE): an undrained PIPE
        # fills its ~64KB OS buffer and deadlocks openocd on write.
        self._logfile = None

    # -- process lifecycle --------------------------------------------------
    def __enter__(self) -> "OpenOCD":
        self.start()
        return self

    def __exit__(self, *exc) -> None:
        self.stop()

    def start(self) -> None:
        # tcl_port must be set in the config phase, i.e. before `init`. The cfg
        # deliberately does not call init, so we do it here after the port is set.
        cmd = [self.openocd, "-f", self.cfg,
               "-c", f"tcl_port {self.port}",
               "-c", "init"]
        if self.verbose:
            print("+ " + " ".join(cmd), file=sys.stderr)
        self._logfile = tempfile.TemporaryFile(mode="w+")
        self.proc = subprocess.Popen(
            cmd, stdout=self._logfile, stderr=subprocess.STDOUT, text=True)
        try:
            self._connect_or_die()
        except Exception:
            self.stop()
            raise

    def _connect_or_die(self, timeout: float = 15.0) -> None:
        deadline = time.time() + timeout
        last_err = None
        while time.time() < deadline:
            # If OpenOCD died (e.g. JTAG chain interrogation failed), surface its log.
            if self.proc.poll() is not None:
                self._logfile.seek(0)
                out = self._logfile.read()
                raise OpenOCDError(
                    "OpenOCD exited before the RPC port was ready.\n"
                    "This usually means it could not reach the target "
                    "(check power, JTAG wiring, VTG).\n\n--- openocd ---\n"
                    + out.strip())
            try:
                s = socket.create_connection(("127.0.0.1", self.port), timeout=1.0)
                s.settimeout(60.0)
                self._rxbuf = b""
                self.sock = s
                return
            except OSError as e:
                last_err = e
                time.sleep(0.2)
        raise OpenOCDError(f"timed out connecting to OpenOCD Tcl-RPC: {last_err}")

    def stop(self) -> None:
        try:
            if self.sock:
                try:
                    self._raw("shutdown")
                except (OSError, OpenOCDError):
                    # shutdown often drops the socket before replying; ignore.
                    pass
                self.sock.close()
        finally:
            self.sock = None
            if self.proc and self.proc.poll() is None:
                try:
                    self.proc.terminate()
                    self.proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.proc.kill()
            if self._logfile:
                self._logfile.close()
                self._logfile = None

    # -- Tcl-RPC ------------------------------------------------------------
    def _raw(self, command: str) -> str:
        if not self.sock:
            raise OpenOCDError("not connected")
        self.sock.sendall(command.encode() + RPC_SENTINEL)
        # One reply == bytes up to the next sentinel. recv() boundaries do not
        # align with sentinels (TCP may coalesce replies or split one), so we
        # read into a buffer and keep whatever follows the sentinel for the
        # next call instead of assuming the chunk ends exactly on it.
        while RPC_SENTINEL not in self._rxbuf:
            data = self.sock.recv(4096)
            if not data:
                raise OpenOCDError("RPC connection closed by OpenOCD")
            self._rxbuf += data
        reply, self._rxbuf = self._rxbuf.split(RPC_SENTINEL, 1)
        return reply.decode(errors="replace")

    def cmd(self, command: str) -> str:
        if self.verbose:
            print(f"  rpc> {command}", file=sys.stderr)
        reply = self._raw(command)
        if self.verbose and reply:
            print("  rpc< " + reply.replace("\n", "\n       "), file=sys.stderr)
        return reply


# -- high-level operations --------------------------------------------------
_IDCODE_RE = re.compile(r"0x([0-9a-fA-F]{8})")


def decode_idcode(idcode: int) -> str:
    """Break an IEEE 1149.1 IDCODE into version / part / manufacturer."""
    version = (idcode >> 28) & 0xF
    part = (idcode >> 12) & 0xFFFF
    manuf = (idcode >> 1) & 0x7FF  # 11-bit JEDEC manufacturer id
    lsb = idcode & 1               # must be 1 for a valid IDCODE
    atmel = " (Atmel/Microchip)" if manuf == 0x01F else ""
    return (f"version=0x{version:X} part=0x{part:04X} "
            f"manuf=0x{manuf:03X}{atmel} lsb={lsb}")


def read_idcode(ocd: OpenOCD) -> int:
    """Read the UC3 IDCODE via scan_chain (DR holds IDCODE right after TAP reset)."""
    out = ocd.cmd("scan_chain")
    # scan_chain prints a table; the TAP row contains the captured IdCode.
    for line in out.splitlines():
        if TAP in line:
            m = _IDCODE_RE.search(line)
            if m:
                return int(m.group(1), 16)
    # Fall back: any 8-hex token in the table.
    m = _IDCODE_RE.search(out)
    if m:
        return int(m.group(1), 16)
    raise OpenOCDError(f"could not parse IDCODE from scan_chain output:\n{out}")


# -- AVR32 UC3 JTAG access (ported from OpenOCD src/target/avr32_jtag.c) -----
#
# We replicate OpenOCD's AVR32 NEXUS/Memory-Word-Access scans using raw
# irscan/drscan over Tcl-RPC. The bit field layouts below are taken verbatim
# from avr32_jtag.c so the DR scans are byte-for-byte identical.

INST_NEXUS_ACCESS = 0x10
INST_MW_ACCESS = 0x11
SLAVE_OCD = 0x01
SLAVE_HSB_CACHED = 0x04
SLAVE_HSB_UNCACHED = 0x05
MODE_WRITE = 0x00
MODE_READ = 0x01

# OCD (NEXUS) register addresses.
OCDREG_DID = 0x00   # Device ID
OCDREG_DC = 0x02    # Development Control
OCDREG_DS = 0x04    # Development Status
OCDREG_DC_DBE = 1 << 13   # Debug Enable
OCDREG_DC_DBR = 1 << 12   # Debug Request (halt the CPU)

# Busy-wait budget for a single SAB access. A chip-erase holds the Service
# Access Bus busy until it completes, so this must comfortably exceed the
# worst-case flash operation time.
_BUSY_TIMEOUT = 8.0


def _ir(ocd: OpenOCD, instr: int) -> None:
    ocd.cmd(f"irscan {TAP} 0x{instr:02x} -endstate IDLE")


def _dr(ocd: OpenOCD, *pairs: tuple[int, int]) -> list[int]:
    """drscan with (num_bits, value) fields; returns captured value per field."""
    args = " ".join(f"{n} 0x{v:x}" for n, v in pairs)
    out = ocd.cmd(f"drscan {TAP} {args} -endstate IDLE")
    return [int(tok, 16) for tok in out.split()]


def _scan_until_ready(ocd, fields, busy_fn, label):
    """Repeat a DR scan until its busy bit clears or _BUSY_TIMEOUT elapses.

    The SAB asserts busy while a long flash op (e.g. chip-erase) is in flight,
    so this doubles as the wait for that op to finish.
    """
    deadline = time.time() + _BUSY_TIMEOUT
    while True:
        ret = _dr(ocd, *fields)
        if not busy_fn(ret):
            return ret
        if time.time() > deadline:
            raise OpenOCDError(f"{label} stuck busy")


def nexus_read(ocd: OpenOCD, addr: int) -> int:
    """Read a 32-bit OCD register over JTAG NEXUS_ACCESS.

    Address phase: 26 unused bits + 8 bits {mode:1, addr:7}; busy = field1 bit 6.
    Data phase:    32 data bits + 2 status bits;             busy = field1 bit 0.
    """
    _ir(ocd, INST_NEXUS_ACCESS)
    a1 = (MODE_READ & 1) | ((addr & 0x7F) << 1)
    _scan_until_ready(ocd, [(26, 0), (8, a1)],
                      lambda r: (r[1] >> 6) & 1, "NEXUS address phase")
    ret = _scan_until_ready(ocd, [(32, 0), (2, 0)],
                            lambda r: r[1] & 1, "NEXUS data phase")
    return ret[0] & 0xFFFFFFFF


def mwa_read(ocd: OpenOCD, addr: int, slave: int = SLAVE_HSB_UNCACHED) -> int:
    """Read a 32-bit word from the system bus over JTAG MEMORY_WORD_ACCESS.

    Address phase: 31 bits {mode:1, (addr>>2):30} + 4 bits slave; busy=field1 bit1.
    Data phase:    32 data bits + 3 status bits;                   busy=field1 bit0.
    """
    if addr & 3:
        raise ValueError("address must be word-aligned")
    _ir(ocd, INST_MW_ACCESS)
    a0 = (MODE_READ & 1) | ((addr >> 2) << 1)   # 31-bit field
    _scan_until_ready(ocd, [(31, a0), (4, slave)],
                      lambda r: (r[1] >> 1) & 1, "MWA address phase")
    ret = _scan_until_ready(ocd, [(32, 0), (3, 0)],
                            lambda r: r[1] & 1, "MWA data phase")
    return ret[0] & 0xFFFFFFFF


def nexus_write(ocd: OpenOCD, addr: int, value: int) -> None:
    """Write a 32-bit OCD register over JTAG NEXUS_ACCESS.

    Data phase per avr32_jtag.c: field0 = 2 dummy bits, field1 = 32 data bits.
    (OpenOCD's nexus-write busy check is a no-op, so a single pass suffices.)
    """
    _ir(ocd, INST_NEXUS_ACCESS)
    a1 = (MODE_WRITE & 1) | ((addr & 0x7F) << 1)
    _scan_until_ready(ocd, [(26, 0), (8, a1)],
                      lambda r: (r[1] >> 6) & 1, "NEXUS address phase")
    _dr(ocd, (2, 0), (32, value & 0xFFFFFFFF))


def mwa_write(ocd: OpenOCD, addr: int, value: int,
              slave: int = SLAVE_HSB_UNCACHED) -> None:
    """Write a 32-bit word to the system bus over JTAG MEMORY_WORD_ACCESS.

    Data phase per avr32_jtag.c: field0 = 3 status bits (busy=bit0),
    field1 = 32 data bits.
    """
    if addr & 3:
        raise ValueError("address must be word-aligned")
    _ir(ocd, INST_MW_ACCESS)
    a0 = (MODE_WRITE & 1) | ((addr >> 2) << 1)
    _scan_until_ready(ocd, [(31, a0), (4, slave)],
                      lambda r: (r[1] >> 1) & 1, "MWA address phase")
    _scan_until_ready(ocd, [(3, 0), (32, value & 0xFFFFFFFF)],
                      lambda r: r[0] & 1, "MWA write data phase")


# Batched MWA helpers: pack a whole page of scans into ONE Tcl-RPC round trip.
# Page-buffer writes and flash reads never assert busy on this part (verified),
# so a single pass per word is safe; verify re-reads any mismatch individually.

def _mwa_fill(ocd, addr: int, words: list[int]) -> None:
    """Fill consecutive words via MWA writes in a single RPC (no result needed)."""
    out = [f"irscan {TAP} 0x{INST_MW_ACCESS:02x} -endstate IDLE"]
    a = addr
    for w in words:
        a0 = (a >> 2) << 1                       # mode = WRITE (0)
        out.append(f"drscan {TAP} 31 0x{a0:x} 4 {SLAVE_HSB_UNCACHED} -endstate IDLE")
        out.append(f"drscan {TAP} 3 0 32 0x{w & 0xFFFFFFFF:x} -endstate IDLE")
        a += 4
    ocd._raw("\n".join(out))


def mwa_read_words(ocd, addr: int, nwords: int, chunk: int = 32) -> list[int]:
    """Read consecutive words via MWA, batching `chunk` words per RPC.

    Large single-RPC scripts (128 words) occasionally stall OpenOCD, so we keep
    each round trip small and concatenate.
    """
    vals: list[int] = []
    done = 0
    while done < nwords:
        n = min(chunk, nwords - done)
        out = ["set r {}", f"irscan {TAP} 0x{INST_MW_ACCESS:02x} -endstate IDLE"]
        a = addr + done * 4
        for _ in range(n):
            a0 = ((a >> 2) << 1) | MODE_READ
            out.append(f"drscan {TAP} 31 0x{a0:x} 4 {SLAVE_HSB_UNCACHED} -endstate IDLE")
            out.append(f"lappend r [lindex [drscan {TAP} 32 0 3 0 -endstate IDLE] 0]")
            a += 4
        out.append("return $r")
        vals += [int(t, 16) for t in ocd._raw("\n".join(out)).split()]
        done += n
    return vals


# -- AVR32 UC3A3 FLASHC flash programming -----------------------------------
# Sequences from app note AVR32708 (doc32070). FLASHC base found empirically
# on this board: FSR readback 0x6001 => FRDY=1, FSZ=3 (256 KB).

FLASH_BASE = 0x80000000                     # flash array (page data r/w here)
USER_PAGE_ADDR = FLASH_BASE + 0x00800000    # 0x80800000, single 512B user page
# The DFU/ISP bootloader occupies the bottom 8 KB (pages 0-15). A normal
# application HEX still contains a reset trampoline in this window, so blindly
# programming such a HEX overwrites an installed bootloader. `program --app-only`
# drops records in this range so an existing bootloader is preserved.
BOOT_REGION_BYTES = 0x2000                  # 0x80000000-0x80001FFF
FLASHC_BASE = 0xFFFE1400                     # FLASHC registers
FLASHC_FCMD = FLASHC_BASE + 0x04
FLASHC_FSR = FLASHC_BASE + 0x08
FLASHC_KEY = 0xA5000000

FCMD_WRITE_PAGE = 1
FCMD_ERASE_PAGE = 2
FCMD_CLEAR_PAGE_BUFFER = 3
FCMD_ERASE_ALL = 6
FCMD_WRITE_USER_PAGE = 13
FCMD_ERASE_USER_PAGE = 14

FSR_FRDY = 1 << 0
FSR_LOCKE = 1 << 2
FSR_PROGE = 1 << 3
_FSZ_KB = {0: 32, 1: 64, 2: 128, 3: 256, 4: 384, 5: 512, 6: 768, 7: 1024}

PAGE_BYTES = 512
PAGE_WORDS = 128


def flashc_flash_size(ocd) -> int:
    fsz = (mwa_read(ocd, FLASHC_FSR) >> 13) & 0x7
    return _FSZ_KB[fsz] * 1024


def flashc_wait_ready(ocd, timeout: float = 12.0) -> int:
    deadline = time.time() + timeout
    while time.time() < deadline:
        fsr = mwa_read(ocd, FLASHC_FSR)
        if fsr & FSR_LOCKE:
            raise OpenOCDError("FLASHC LOCKE: write to a locked region")
        if fsr & FSR_PROGE:
            raise OpenOCDError("FLASHC PROGE: invalid command/key")
        if fsr & FSR_FRDY:
            return fsr
    raise OpenOCDError("FLASHC FRDY timeout")


def flashc_command(ocd, cmd: int, pagen: int = 0) -> None:
    flashc_wait_ready(ocd)
    mwa_write(ocd, FLASHC_FCMD, FLASHC_KEY | ((pagen & 0xFFFF) << 8) | (cmd & 0x1F))
    flashc_wait_ready(ocd)


def ocd_setbits(ocd, reg: int, bits: int) -> None:
    nexus_write(ocd, reg, nexus_read(ocd, reg) | bits)


def cpu_halt(ocd) -> None:
    """Stop the CPU via the OCD (DC.DBE then DC.DBR).

    Mandatory before erasing/programming: if the core keeps running from flash
    while it is erased it will fault and drag the flash controller into reset,
    wedging the SAB (requires a power cycle to recover).
    """
    ocd_setbits(ocd, OCDREG_DC, OCDREG_DC_DBE)
    ocd_setbits(ocd, OCDREG_DC, OCDREG_DC_DBR)


def cpu_resume(ocd) -> None:
    nexus_write(ocd, OCDREG_DC, nexus_read(ocd, OCDREG_DC) & ~OCDREG_DC_DBR)


def flash_erase_all(ocd) -> None:
    flashc_command(ocd, FCMD_ERASE_ALL)


def _words_be(buf: bytes) -> list[int]:
    return [int.from_bytes(buf[i:i + 4], "big") for i in range(0, len(buf), 4)]


def flash_program_page(ocd, page_index: int, buf512: bytes, erase: bool = True) -> None:
    # NOR flash only clears bits (1->0), so the page must be blank before
    # WRITE_PAGE. Erase it first unless the caller already did a full chip erase.
    if erase:
        flashc_command(ocd, FCMD_ERASE_PAGE, page_index)
    flashc_command(ocd, FCMD_CLEAR_PAGE_BUFFER)
    _mwa_fill(ocd, FLASH_BASE + page_index * PAGE_BYTES, _words_be(buf512))
    flashc_command(ocd, FCMD_WRITE_PAGE, page_index)


def flash_program_user_page(ocd, buf512: bytes) -> None:
    flashc_command(ocd, FCMD_ERASE_USER_PAGE)
    flashc_command(ocd, FCMD_CLEAR_PAGE_BUFFER)
    _mwa_fill(ocd, USER_PAGE_ADDR, _words_be(buf512))
    flashc_command(ocd, FCMD_WRITE_USER_PAGE)


def parse_ihex(path: str) -> list[tuple[int, bytes]]:
    """Minimal Intel HEX parser.

    Handles record types 00 (data), 01 (EOF), 04 (extended linear address).
    Types 03/05 (start address) carry no flash data and are ignored. Type 02
    (extended segment) is rejected. Malformed records (bad checksum, truncated,
    or a byte-count that disagrees with the payload) raise ValueError rather
    than slicing past the end of the line.
    """
    segs: list[tuple[int, bytes]] = []
    ext = 0
    with open(path) as f:
        for ln in f:
            ln = ln.strip()
            if not ln.startswith(":"):
                continue
            try:
                b = bytes.fromhex(ln[1:])
            except ValueError:
                raise ValueError(f"ihex non-hex record: {ln}")
            if len(b) < 5:                       # count + addr(2) + type + checksum
                raise ValueError(f"ihex record too short: {ln}")
            if (sum(b) & 0xFF) != 0:
                raise ValueError(f"ihex checksum error: {ln}")
            n, addr, rt = b[0], (b[1] << 8) | b[2], b[3]
            if len(b) != n + 5:                  # n data bytes between type and checksum
                raise ValueError(f"ihex length mismatch (count={n}): {ln}")
            data = b[4:4 + n]
            if rt == 0:
                segs.append(((ext << 16) + addr, data))
            elif rt == 4:
                if n != 2:
                    raise ValueError(f"ihex type-04 must carry 2 bytes: {ln}")
                ext = (data[0] << 8) | data[1]
            elif rt == 1:
                break
            elif rt in (3, 5):
                continue                         # start-address records: no flash data
            elif rt == 2:
                raise ValueError("ihex type-02 (segment) not supported")
            else:
                raise ValueError(f"ihex unsupported record type 0x{rt:02X}: {ln}")
    return segs


def build_image(segs, flash_size: int):
    """Split hex into main-flash pages (page_index -> 512B) and the user page."""
    main: dict[int, bytearray] = {}
    user = None
    for addr, data in segs:
        for i, byte in enumerate(data):
            a = addr + i
            if FLASH_BASE <= a < FLASH_BASE + flash_size:
                off = a - FLASH_BASE
                pg = main.setdefault(off // PAGE_BYTES, bytearray(b"\xff" * PAGE_BYTES))
                pg[off % PAGE_BYTES] = byte
            elif USER_PAGE_ADDR <= a < USER_PAGE_ADDR + PAGE_BYTES:
                if user is None:
                    user = bytearray(b"\xff" * PAGE_BYTES)
                user[a - USER_PAGE_ADDR] = byte
            else:
                raise OpenOCDError(f"hex address 0x{a:08X} outside flash/user page")
    return main, user


def boot_region_pages(flash_size: int) -> set[int]:
    """Page indices covered by the bootloader window [FLASH_BASE, +BOOT_REGION_BYTES)."""
    n = min(BOOT_REGION_BYTES, flash_size) // PAGE_BYTES
    return set(range(n))


def verify_grouped(make_ocd, main, user, group: int = 16, progress: bool = True) -> int:
    """Read back flash and compare to the expected image; returns mismatch count.

    OpenOCD's RPC server wedges after a few thousand result-returning reads in a
    single session, so we restart the session every `group` pages to stay well
    under that threshold.
    """
    pages = sorted(main)
    n = len(pages)
    bad = 0
    for start in range(0, n, group):
        with make_ocd() as ocd:
            for pi in pages[start:start + group]:
                expect = _words_be(bytes(main[pi]))
                got = mwa_read_words(ocd, FLASH_BASE + pi * PAGE_BYTES, PAGE_WORDS)
                for j, (e, g) in enumerate(zip(expect, got)):
                    if e != g and mwa_read(ocd, FLASH_BASE + pi * PAGE_BYTES + j * 4) != e:
                        bad += 1
                        if bad <= 8:
                            print(f"  MISMATCH @0x{FLASH_BASE + pi * PAGE_BYTES + j * 4:08X}"
                                  f": exp 0x{e:08X} got 0x{g:08X}")
        if progress:
            print(f"\r  {min(start + group, n)}/{n} pages", end="", flush=True)
    if progress:
        print()
    if user is not None:
        with make_ocd() as ocd:
            exp = _words_be(bytes(user))
            got = mwa_read_words(ocd, USER_PAGE_ADDR, PAGE_WORDS)
            bad += sum(1 for e, g in zip(exp, got) if e != g)
    return bad


# -- CLI --------------------------------------------------------------------
def cmd_idcode(args) -> int:
    with OpenOCD(cfg=args.config, port=args.port, openocd=args.openocd,
                 verbose=args.verbose) as ocd:
        idcode = read_idcode(ocd)
        if idcode in (0x00000000, 0xFFFFFFFF):
            print(f"IDCODE = 0x{idcode:08X}  -- INVALID (open/unpowered chain). "
                  "Check power, wiring, VTG.", file=sys.stderr)
            return 2
        print(f"IDCODE = 0x{idcode:08X}")
        print(f"         {decode_idcode(idcode)}")
        return 0


def cmd_did(args) -> int:
    with OpenOCD(cfg=args.config, port=args.port, openocd=args.openocd,
                 verbose=args.verbose) as ocd:
        did = nexus_read(ocd, OCDREG_DID)
        dc = nexus_read(ocd, OCDREG_DC)
        ds = nexus_read(ocd, OCDREG_DS)
        print(f"OCD DID (NEXUS 0x00) = 0x{did:08X}")
        print(f"OCD DC  (NEXUS 0x02) = 0x{dc:08X}")
        print(f"OCD DS  (NEXUS 0x04) = 0x{ds:08X}")
        if did in (0, 0xFFFFFFFF):
            print("DID looks invalid -- NEXUS access not working.", file=sys.stderr)
            return 2
        return 0


def cmd_read(args) -> int:
    addr = int(args.addr, 0)
    count = args.count
    with OpenOCD(cfg=args.config, port=args.port, openocd=args.openocd,
                 verbose=args.verbose) as ocd:
        for i in range(count):
            a = addr + 4 * i
            val = mwa_read(ocd, a, slave=args.slave)
            print(f"0x{a:08X}: 0x{val:08X}")
    return 0


def cmd_write(args) -> int:
    """Raw system-bus word write via MWA (for SRAM/peripherals; flash needs
    the page-buffer + FLASHC sequence, not raw writes)."""
    addr = int(args.addr, 0)
    value = int(args.value, 0)
    with OpenOCD(cfg=args.config, port=args.port, openocd=args.openocd,
                 verbose=args.verbose) as ocd:
        mwa_write(ocd, addr, value, slave=args.slave)
        if args.verify:
            back = mwa_read(ocd, addr, slave=args.slave)
            ok = back == (value & 0xFFFFFFFF)
            print(f"0x{addr:08X}: wrote 0x{value:08X}, read 0x{back:08X} "
                  f"-> {'OK' if ok else 'MISMATCH'}")
            return 0 if ok else 2
        print(f"0x{addr:08X} <- 0x{value:08X}")
    return 0


def cmd_flashinfo(args) -> int:
    with OpenOCD(cfg=args.config, port=args.port, openocd=args.openocd,
                 verbose=args.verbose) as ocd:
        fsr = mwa_read(ocd, FLASHC_FSR)
        size = flashc_flash_size(ocd)
        print(f"FLASHC FSR = 0x{fsr:08X}  FRDY={fsr & 1}  "
              f"LOCKE={(fsr >> 2) & 1}  PROGE={(fsr >> 3) & 1}")
        print(f"flash size = {size // 1024} KB ({size // PAGE_BYTES} pages)")
        return 0


def cmd_erase(args) -> int:
    with OpenOCD(cfg=args.config, port=args.port, openocd=args.openocd,
                 verbose=args.verbose) as ocd:
        if not args.no_halt:
            print("Halting CPU...", flush=True)
            cpu_halt(ocd)
        print("Chip erase (ERASE_ALL)...", flush=True)
        flash_erase_all(ocd)
        w0 = mwa_read(ocd, FLASH_BASE)
        wn = mwa_read(ocd, FLASH_BASE + flashc_flash_size(ocd) - 4)
        ok = w0 == 0xFFFFFFFF and wn == 0xFFFFFFFF
        print(f"after erase: [0x{FLASH_BASE:08X}]=0x{w0:08X}  "
              f"[top]=0x{wn:08X} -> {'ERASED' if ok else 'NOT BLANK'}")
        return 0 if ok else 2


def _make_ocd(args):
    return lambda: OpenOCD(cfg=args.config, port=args.port, openocd=args.openocd,
                           verbose=args.verbose)


def cmd_program(args) -> int:
    segs = parse_ihex(args.hexfile)
    mk = _make_ocd(args)
    with mk() as ocd:
        size = flashc_flash_size(ocd)
        main, user = build_image(segs, size)
        if user is not None and not args.program_user_page:
            print("Refusing to program UC3 user page records from this HEX without "
                  "--program-user-page.", file=sys.stderr)
            return 2

        # Bootloader-region policy. A normal application HEX has a reset
        # trampoline in pages 0-15, which would overwrite an installed
        # bootloader. --app-only drops those pages to preserve it; otherwise
        # warn loudly so the overwrite is never silent.
        boot_pages = boot_region_pages(size)
        touched_boot = sorted(p for p in main if p in boot_pages)
        if args.app_only:
            for p in touched_boot:
                del main[p]
            if touched_boot:
                print(f"--app-only: dropped {len(touched_boot)} bootloader-region "
                      f"page(s) (0x{FLASH_BASE:08X}-0x{FLASH_BASE + BOOT_REGION_BYTES - 1:08X}); "
                      "existing bootloader preserved.")
        elif touched_boot and not args.erase_all:
            print(f"WARNING: this HEX writes {len(touched_boot)} page(s) in the "
                  f"bootloader region (0x{FLASH_BASE:08X}-0x{FLASH_BASE + BOOT_REGION_BYTES - 1:08X}); "
                  "any installed bootloader there will be overwritten. Pass "
                  "--app-only to preserve it.", file=sys.stderr)

        pages = sorted(main)
        if not pages and user is None:
            print("Nothing to program after applying policy filters.", file=sys.stderr)
            return 2
        print(f"{args.hexfile}: {len(pages)} flash pages"
              f"{' + user page' if user else ''} (flash {size // 1024} KB)")

        if not args.no_halt:
            print("Halting CPU...", flush=True)
            cpu_halt(ocd)
        if args.erase_all:
            print("Chip erase...", flush=True)
            flash_erase_all(ocd)
        else:
            print("Skipping chip erase; pass --erase-all for a full destructive erase.",
                  flush=True)

        # If we just chip-erased, pages are blank; otherwise erase each page
        # before writing so re-programming a non-blank chip is correct.
        erase_each = not args.erase_all
        print("Programming...", flush=True)
        for i, pi in enumerate(pages):
            flash_program_page(ocd, pi, bytes(main[pi]), erase=erase_each)
            if i % 16 == 0 or i == len(pages) - 1:
                print(f"\r  page {i + 1}/{len(pages)}", end="", flush=True)
        print()
        if user is not None:
            print("Programming user page...", flush=True)
            flash_program_user_page(ocd, bytes(user))

    if args.no_verify:
        return 0
    print("Verifying...", flush=True)
    bad = verify_grouped(mk, main, user)
    if bad:
        print(f"VERIFY FAILED: {bad} word(s) differ", file=sys.stderr)
        return 2
    print("VERIFY OK")
    return 0


def cmd_verify(args) -> int:
    segs = parse_ihex(args.hexfile)
    mk = _make_ocd(args)
    with mk() as ocd:
        size = flashc_flash_size(ocd)
    main, user = build_image(segs, size)
    print(f"Verifying {args.hexfile} ({len(main)} pages"
          f"{' + user page' if user else ''})...", flush=True)
    bad = verify_grouped(mk, main, user)
    if bad:
        print(f"VERIFY FAILED: {bad} word(s) differ", file=sys.stderr)
        return 2
    print("VERIFY OK")
    return 0


def cmd_halt(args) -> int:
    with OpenOCD(cfg=args.config, port=args.port, openocd=args.openocd,
                 verbose=args.verbose) as ocd:
        cpu_halt(ocd)
        dc = nexus_read(ocd, OCDREG_DC)
        ds = nexus_read(ocd, OCDREG_DS)
        print(f"halt requested: DC=0x{dc:08X}  DS=0x{ds:08X}")
        return 0


def cmd_notimpl(args) -> int:
    print(f"'{args.cmd}' is not implemented yet. Run 'idcode' first to confirm "
          "the JTAG link, then we build this milestone. See the roadmap in the "
          "module docstring / README.", file=sys.stderr)
    return 3


def main(argv=None) -> int:
    # Shared options usable either before OR after the subcommand.
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--config", default=DEFAULT_CFG, help="OpenOCD transport .cfg")
    common.add_argument("--port", type=int, default=6666, help="OpenOCD Tcl-RPC port")
    common.add_argument("--openocd", default=None, help="path to openocd binary")
    common.add_argument("-v", "--verbose", action="store_true", help="trace RPC traffic")

    p = argparse.ArgumentParser(
        prog="uc3jtag", parents=[common],
        description="Flash/debug an AT32UC3A3256 over JTAG via Atmel-ICE on macOS.")
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("idcode", parents=[common],
                   help="connect and read the device IDCODE").set_defaults(
        func=cmd_idcode)

    sub.add_parser("did", parents=[common],
                   help="read OCD DID/DC/DS registers (NEXUS access)").set_defaults(
        func=cmd_did)

    pr = sub.add_parser("read", parents=[common],
                        help="read words from the system bus (MEMORY_WORD_ACCESS)")
    pr.add_argument("addr", help="word-aligned address, e.g. 0x80000000 (flash base)")
    pr.add_argument("count", nargs="?", type=int, default=1, help="word count")
    pr.add_argument("--slave", type=lambda s: int(s, 0), default=SLAVE_HSB_UNCACHED,
                    help="SAB slave (5=HSB uncached, 4=HSB cached, 1=OCD)")
    pr.set_defaults(func=cmd_read)

    pw = sub.add_parser("write", parents=[common],
                        help="raw bus word write via MWA (SRAM/peripherals)")
    pw.add_argument("addr", help="word-aligned address")
    pw.add_argument("value", help="32-bit value, e.g. 0xDEADBEEF")
    pw.add_argument("--verify", action="store_true", help="read back after write")
    pw.add_argument("--slave", type=lambda s: int(s, 0), default=SLAVE_HSB_UNCACHED)
    pw.set_defaults(func=cmd_write)

    sub.add_parser("flashinfo", parents=[common],
                   help="read FLASHC status and flash size").set_defaults(
        func=cmd_flashinfo)

    sub.add_parser("halt", parents=[common],
                   help="stop the CPU via the OCD (DC.DBE|DBR)").set_defaults(
        func=cmd_halt)

    pe = sub.add_parser("erase", parents=[common],
                        help="halt + chip erase (ERASE_ALL) + blank check")
    pe.add_argument("--no-halt", action="store_true", help="skip CPU halt (unsafe)")
    pe.set_defaults(func=cmd_erase)

    pp = sub.add_parser("program", parents=[common],
                        help="halt + flash an Intel .hex, then verify")
    pp.add_argument("hexfile", help="Intel HEX file (e.g. ../../Release/widget.hex)")
    pp.add_argument("--no-halt", action="store_true", help="skip CPU halt (unsafe)")
    pp.add_argument("--erase-all", action="store_true",
                    help="chip erase before programming; destroys bootloader/config "
                         "unless the image restores them")
    pp.add_argument("--app-only", action="store_true",
                    help="drop records in the bootloader region "
                         "(0x80000000-0x80001FFF) so an installed bootloader is "
                         "preserved; otherwise a normal app HEX overwrites it")
    pp.add_argument("--program-user-page", action="store_true",
                    help="allow programming records in the UC3 user page at 0x80800000")
    pp.add_argument("--no-verify", action="store_true", help="skip verify")
    pp.set_defaults(func=cmd_program)

    pv = sub.add_parser("verify", parents=[common],
                        help="read back flash and compare to an Intel .hex")
    pv.add_argument("hexfile", help="Intel HEX file")
    pv.set_defaults(func=cmd_verify)

    sub.add_parser("fuses", parents=[common],
                   help="GP/BOOTPROT fuses, restore bootloader (TODO)").set_defaults(
        func=cmd_notimpl)

    args = p.parse_args(argv)
    try:
        return args.func(args)
    except (OpenOCDError, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
