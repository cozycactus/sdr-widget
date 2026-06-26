import os
import socket
import tempfile
import threading
import unittest

import make_bootloader_image
import uc3jtag


def ihex_record(addr, record_type, data):
    data = bytes(data)
    body = bytes([len(data), (addr >> 8) & 0xFF, addr & 0xFF, record_type]) + data
    return ":%s%02X" % (body.hex().upper(), (-sum(body)) & 0xFF)


def write_ihex(records):
    fd, path = tempfile.mkstemp(suffix=".hex")
    with os.fdopen(fd, "w") as f:
        f.write("\n".join(records + [ihex_record(0, 1, b"")]) + "\n")
    return path


class IntelHexTests(unittest.TestCase):
    def test_parse_ihex_extended_linear_address(self):
        path = write_ihex([
            ihex_record(0, 4, bytes([0x80, 0x00])),
            ihex_record(0x2000, 0, bytes([1, 2, 3, 4])),
        ])
        self.addCleanup(os.unlink, path)

        self.assertEqual(
            uc3jtag.parse_ihex(path),
            [(uc3jtag.FLASH_BASE + 0x2000, bytes([1, 2, 3, 4]))],
        )

    def test_parse_ihex_rejects_bad_checksum(self):
        fd, path = tempfile.mkstemp(suffix=".hex")
        with os.fdopen(fd, "w") as f:
            f.write(":00000001FE\n")
        self.addCleanup(os.unlink, path)

        with self.assertRaises(ValueError):
            uc3jtag.parse_ihex(path)


class ImageLayoutTests(unittest.TestCase):
    def test_build_image_splits_flash_and_user_page(self):
        main, user = uc3jtag.build_image([
            (uc3jtag.FLASH_BASE + 0x10, bytes([0x11, 0x22])),
            (uc3jtag.USER_PAGE_ADDR, bytes([0x33])),
        ], flash_size=256 * 1024)

        self.assertEqual(main[0][0x10:0x12], bytes([0x11, 0x22]))
        self.assertEqual(user[0], 0x33)

    def test_build_image_rejects_out_of_range_address(self):
        with self.assertRaises(uc3jtag.OpenOCDError):
            uc3jtag.build_image([(0x12345678, b"\x00")], flash_size=256 * 1024)

    def test_bootloader_merge_drops_app_trampoline(self):
        app = write_ihex([
            ihex_record(0, 4, bytes([0x80, 0x00])),
            ihex_record(0x0000, 0, bytes([0xAA])),
            ihex_record(0x2000, 0, bytes([0xBB])),
        ])
        boot = write_ihex([
            ihex_record(0, 4, bytes([0x80, 0x00])),
            ihex_record(0x0000, 0, bytes([0xCC])),
        ])
        self.addCleanup(os.unlink, app)
        self.addCleanup(os.unlink, boot)

        mem, dropped = make_bootloader_image.build_merged_image(app, boot)

        self.assertEqual(dropped, 1)
        self.assertEqual(mem[uc3jtag.FLASH_BASE], 0xCC)
        self.assertEqual(mem[uc3jtag.FLASH_BASE + 0x2000], 0xBB)


class BootRegionPolicyTests(unittest.TestCase):
    def test_boot_region_pages_covers_bottom_8kb(self):
        pages = uc3jtag.boot_region_pages(256 * 1024)
        # 0x2000 / 512 == 16 pages (0..15).
        self.assertEqual(pages, set(range(16)))

    def test_normal_app_hex_touches_boot_region(self):
        # A reset trampoline at FLASH_BASE lands in the bootloader window, so a
        # naive program would overwrite an installed bootloader.
        main, user = uc3jtag.build_image(
            [(uc3jtag.FLASH_BASE, bytes([0x01, 0x02, 0x03, 0x04])),
             (uc3jtag.FLASH_BASE + 0x4000, bytes([0xAA]))],
            flash_size=256 * 1024)
        boot_pages = uc3jtag.boot_region_pages(256 * 1024)
        touched = [p for p in main if p in boot_pages]
        self.assertIn(0, touched)

    def test_app_only_filter_preserves_bootloader_pages(self):
        # Simulate the --app-only filter cmd_program applies: drop boot pages.
        main, _ = uc3jtag.build_image(
            [(uc3jtag.FLASH_BASE, bytes([0x11])),            # page 0  (boot)
             (uc3jtag.FLASH_BASE + 0x4000, bytes([0x22]))],  # page 32 (app)
            flash_size=256 * 1024)
        boot_pages = uc3jtag.boot_region_pages(256 * 1024)
        for p in [p for p in main if p in boot_pages]:
            del main[p]
        self.assertNotIn(0, main)
        self.assertIn(0x4000 // uc3jtag.PAGE_BYTES, main)


class _FakeRPCServer:
    """A minimal TCP server that replies with fixed bytes, for _raw() framing."""

    def __init__(self, reply: bytes):
        self.reply = reply
        self.srv = socket.socket()
        self.srv.bind(("127.0.0.1", 0))
        self.srv.listen(1)
        self.port = self.srv.getsockname()[1]
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()

    def _serve(self):
        conn, _ = self.srv.accept()
        try:
            conn.recv(64)        # consume the command
            conn.sendall(self.reply)
            # keep the connection open briefly so the client can read
            import time
            time.sleep(0.2)
        finally:
            conn.close()
            self.srv.close()


def _client_ocd(port):
    ocd = object.__new__(uc3jtag.OpenOCD)
    sock = socket.create_connection(("127.0.0.1", port), timeout=1.0)
    ocd.sock = sock
    ocd._rxbuf = b""
    ocd.verbose = False
    return ocd, sock


class TclRpcFramingTests(unittest.TestCase):
    def test_two_replies_coalesced_in_one_packet(self):
        # OpenOCD result + terminator + a second reply can arrive together;
        # _raw must return only the first reply and keep the rest buffered.
        srv = _FakeRPCServer(b"ONE\x1aTWO\x1a")
        ocd, sock = _client_ocd(srv.port)
        self.addCleanup(sock.close)
        self.assertEqual(ocd._raw("a"), "ONE")
        # second call must NOT re-read the socket for the buffered reply
        self.assertEqual(ocd._raw("b"), "TWO")

    def test_reply_followed_by_extra_bytes(self):
        # Sentinel not at end of the recv chunk: old code blocked until close.
        srv = _FakeRPCServer(b"OK\x1aEXTRA\x1a")
        ocd, sock = _client_ocd(srv.port)
        self.addCleanup(sock.close)
        self.assertEqual(ocd._raw("a"), "OK")
        self.assertEqual(ocd._raw("b"), "EXTRA")

    def test_reply_split_across_recv_calls(self):
        # Reply delivered in fragments without a trailing sentinel in the first
        # chunk; _raw must keep reading until the sentinel arrives.
        class _Frag(_FakeRPCServer):
            def _serve(self):
                conn, _ = self.srv.accept()
                try:
                    conn.recv(64)
                    conn.sendall(b"HEL")
                    import time
                    time.sleep(0.05)
                    conn.sendall(b"LO\x1a")
                    time.sleep(0.1)
                finally:
                    conn.close()
                    self.srv.close()

        srv = _Frag(b"")
        ocd, sock = _client_ocd(srv.port)
        self.addCleanup(sock.close)
        self.assertEqual(ocd._raw("a"), "HELLO")


if __name__ == "__main__":
    unittest.main()
