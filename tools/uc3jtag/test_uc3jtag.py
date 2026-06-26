import os
import tempfile
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


if __name__ == "__main__":
    unittest.main()
