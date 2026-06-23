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
JTAG reaches the UC3 through the Atmel-ICE SAM port. The rest is protocol work
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
                    "(check power, SAM-port wiring, VTG).\n\n--- openocd ---\n"
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
