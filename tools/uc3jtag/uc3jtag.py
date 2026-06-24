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

Milestones
----------
  [1] idcode  - connect and read the device IDCODE          <-- viability gate
  [2] halt    - AVR_RESET + enter debug (needs JTAG opcodes)
  [3] memory  - HSB read/write via NEXUS/MEMORY access
  [4] erase   - JTAG CHIP_ERASE
  [5] program - FLASHC page-buffer programming of a .hex/.bin
  [6] fuses   - GP/BOOTPROT fuses + restore DFU bootloader

Only milestone 1 is implemented so far; it is the gate that proves generic
JTAG reaches the UC3 through the Atmel-ICE (either port). The rest is protocol work
built on the same transport.

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
        if not os.path.exists(self.cfg):
            raise OpenOCDError(f"transport config not found: {self.cfg}")
        self.proc: subprocess.Popen | None = None
        self.sock: socket.socket | None = None
        self._log: list[str] = []

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
        self.proc = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        self._connect_or_die()

    def _connect_or_die(self, timeout: float = 15.0) -> None:
        deadline = time.time() + timeout
        last_err = None
        while time.time() < deadline:
            # If OpenOCD died (e.g. JTAG chain interrogation failed), surface its log.
            if self.proc.poll() is not None:
                out = self.proc.stdout.read() if self.proc.stdout else ""
                raise OpenOCDError(
                    "OpenOCD exited before the RPC port was ready.\n"
                    "This usually means it could not reach the target "
                    "(check power, JTAG wiring, VTG).\n\n--- openocd ---\n"
                    + out.strip())
            try:
                s = socket.create_connection(("127.0.0.1", self.port), timeout=1.0)
                s.settimeout(10.0)
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
                except OSError:
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

    # -- Tcl-RPC ------------------------------------------------------------
    def _raw(self, command: str) -> str:
        if not self.sock:
            raise OpenOCDError("not connected")
        self.sock.sendall(command.encode() + RPC_SENTINEL)
        chunks = []
        while True:
            data = self.sock.recv(4096)
            if not data:
                raise OpenOCDError("RPC connection closed by OpenOCD")
            chunks.append(data)
            if data.endswith(RPC_SENTINEL):
                break
        return b"".join(chunks)[:-1].decode(errors="replace")

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

_MAX_BUSY = 64


def _ir(ocd: OpenOCD, instr: int) -> None:
    ocd.cmd(f"irscan {TAP} 0x{instr:02x} -endstate IDLE")


def _dr(ocd: OpenOCD, *pairs: tuple[int, int]) -> list[int]:
    """drscan with (num_bits, value) fields; returns captured value per field."""
    args = " ".join(f"{n} 0x{v:x}" for n, v in pairs)
    out = ocd.cmd(f"drscan {TAP} {args} -endstate IDLE")
    return [int(tok, 16) for tok in out.split()]


def nexus_read(ocd: OpenOCD, addr: int) -> int:
    """Read a 32-bit OCD register over JTAG NEXUS_ACCESS.

    Address phase: 26 unused bits + 8 bits {mode:1, addr:7}; busy = field1 bit 6.
    Data phase:    32 data bits + 2 status bits;             busy = field1 bit 0.
    """
    _ir(ocd, INST_NEXUS_ACCESS)
    a1 = (MODE_READ & 1) | ((addr & 0x7F) << 1)
    for _ in range(_MAX_BUSY):
        ret = _dr(ocd, (26, 0), (8, a1))
        if not ((ret[1] >> 6) & 1):
            break
    else:
        raise OpenOCDError("NEXUS address phase stuck busy")
    for _ in range(_MAX_BUSY):
        ret = _dr(ocd, (32, 0), (2, 0))
        if not (ret[1] & 1):
            return ret[0] & 0xFFFFFFFF
    raise OpenOCDError("NEXUS data phase stuck busy")


def mwa_read(ocd: OpenOCD, addr: int, slave: int = SLAVE_HSB_UNCACHED) -> int:
    """Read a 32-bit word from the system bus over JTAG MEMORY_WORD_ACCESS.

    Address phase: 31 bits {mode:1, (addr>>2):30} + 4 bits slave; busy=field1 bit1.
    Data phase:    32 data bits + 3 status bits;                   busy=field1 bit0.
    """
    if addr & 3:
        raise ValueError("address must be word-aligned")
    _ir(ocd, INST_MW_ACCESS)
    a0 = (MODE_READ & 1) | ((addr >> 2) << 1)   # 31-bit field
    for _ in range(_MAX_BUSY):
        ret = _dr(ocd, (31, a0), (4, slave))
        if not ((ret[1] >> 1) & 1):
            break
    else:
        raise OpenOCDError("MWA address phase stuck busy")
    for _ in range(_MAX_BUSY):
        ret = _dr(ocd, (32, 0), (3, 0))
        if not (ret[1] & 1):
            return ret[0] & 0xFFFFFFFF
    raise OpenOCDError("MWA data phase stuck busy")


def nexus_write(ocd: OpenOCD, addr: int, value: int) -> None:
    """Write a 32-bit OCD register over JTAG NEXUS_ACCESS.

    Data phase per avr32_jtag.c: field0 = 2 dummy bits, field1 = 32 data bits.
    (OpenOCD's nexus-write busy check is a no-op, so a single pass suffices.)
    """
    _ir(ocd, INST_NEXUS_ACCESS)
    a1 = (MODE_WRITE & 1) | ((addr & 0x7F) << 1)
    for _ in range(_MAX_BUSY):
        ret = _dr(ocd, (26, 0), (8, a1))
        if not ((ret[1] >> 6) & 1):
            break
    else:
        raise OpenOCDError("NEXUS address phase stuck busy")
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
    for _ in range(_MAX_BUSY):
        ret = _dr(ocd, (31, a0), (4, slave))
        if not ((ret[1] >> 1) & 1):
            break
    else:
        raise OpenOCDError("MWA address phase stuck busy")
    for _ in range(_MAX_BUSY):
        ret = _dr(ocd, (3, 0), (32, value & 0xFFFFFFFF))
        if not (ret[0] & 1):
            return
    raise OpenOCDError("MWA write data phase stuck busy")


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

    for name, help_ in [("halt", "enter debug (TODO)"),
                        ("erase", "chip erase (TODO)"),
                        ("program", "flash a .hex/.bin (TODO)"),
                        ("fuses", "read/write fuses, restore bootloader (TODO)")]:
        sub.add_parser(name, parents=[common], help=help_).set_defaults(
            func=cmd_notimpl)

    args = p.parse_args(argv)
    try:
        return args.func(args)
    except OpenOCDError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
