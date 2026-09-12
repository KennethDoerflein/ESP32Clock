#!/usr/bin/env python3
"""
Unit tests for OTA scripts and build helpers.
"""

from datetime import datetime
import hashlib
import os
import re
import sys
import tempfile
import unittest

# Add project root and scripts dir to path
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "scripts")
sys.path.insert(0, SCRIPTS_DIR)
sys.path.insert(0, PROJECT_ROOT)

from generate_ota_keys import (
    format_public_key_c_array,
    update_public_key_header,
    nacl as ota_nacl,
    generate_keypair,
)
from sign_firmware import (
    compute_firmware_sha256,
    get_artifact_paths,
    parse_private_key,
    sign_firmware_data,
    nacl as sign_nacl,
)
from get_version import (
    generate_dev_version,
    format_version_header,
)


class TestOtaKeyGeneration(unittest.TestCase):
    def test_format_public_key_c_array_length_and_format(self):
        dummy_key = bytes(range(32))
        formatted = format_public_key_c_array(dummy_key)

        lines = formatted.split("\n")
        self.assertEqual(len(lines), 4, "Should be 4 lines of 8 bytes each")

        # Each line should have 8 hex elements indented by 4 spaces
        for line in lines:
            self.assertTrue(line.startswith("    0x"))
            elements = [elem.strip() for elem in line.rstrip(",").split(",")]
            self.assertEqual(len(elements), 8)
            for elem in elements:
                self.assertTrue(re.match(r"^0x[0-9a-f]{2}$", elem))

    def test_format_public_key_c_array_values(self):
        test_bytes = bytes([0x00, 0xFF, 0x12, 0x34, 0xAB, 0xCD, 0xEF, 0x42] * 4)
        formatted = format_public_key_c_array(test_bytes)
        self.assertIn("0x00, 0xff, 0x12, 0x34, 0xab, 0xcd, 0xef, 0x42", formatted)

    def test_update_public_key_header_file_creation(self):
        test_bytes = bytes(range(32))
        with tempfile.TemporaryDirectory() as tmp_dir:
            target_header = os.path.join(tmp_dir, "ota_public_key.h")
            update_public_key_header(test_bytes, target_header)

            self.assertTrue(os.path.exists(target_header))
            with open(target_header, "r") as f:
                content = f.read()

            self.assertIn("#pragma once", content)
            self.assertIn("static const uint8_t OTA_PUBLIC_KEY[32] = {", content)
            self.assertIn("static const bool OTA_KEY_CONFIGURED = true;", content)
            self.assertIn("0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07", content)

    def test_generate_keypair_if_nacl_installed(self):
        if ota_nacl is None:
            self.skipTest("PyNaCl not installed in host environment")
        sk, vk = generate_keypair()
        self.assertEqual(len(bytes(sk)), 32)
        self.assertEqual(len(bytes(vk)), 32)


class TestFirmwareSigningScript(unittest.TestCase):
    def test_compute_firmware_sha256_known_vectors(self):
        # Empty input NIST SHA-256
        empty_hash = compute_firmware_sha256(b"")
        self.assertEqual(
            empty_hash,
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        )

        # "abc" NIST SHA-256
        abc_hash = compute_firmware_sha256(b"abc")
        self.assertEqual(
            abc_hash,
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        )

    def test_get_artifact_paths(self):
        sig, sha = get_artifact_paths("firmware.bin")
        self.assertEqual(sig, "firmware.sig")
        self.assertEqual(sha, "firmware.sha256")

        sig_sub, sha_sub = get_artifact_paths(".pio/build/esp32s3/firmware.bin")
        self.assertEqual(sig_sub, ".pio/build/esp32s3/firmware.sig")
        self.assertEqual(sha_sub, ".pio/build/esp32s3/firmware.sha256")

    def test_parse_private_key_validation(self):
        if sign_nacl is None:
            self.skipTest("PyNaCl not installed in host environment")

        # Invalid length (not 32 bytes / 64 hex chars)
        with self.assertRaises(ValueError):
            parse_private_key("deadbeef")

        # Invalid hex characters
        with self.assertRaises(ValueError):
            parse_private_key("z" * 64)

        # Valid 32-byte key
        valid_hex = "01" * 32
        key = parse_private_key(valid_hex)
        self.assertIsNotNone(key)

    def test_sign_firmware_data_flow(self):
        if sign_nacl is None:
            self.skipTest("PyNaCl not installed in host environment")

        dummy_data = b"ESP32Clock sample firmware content for signing test"
        dummy_privkey = "42" * 32
        hash_hex, sig_hex = sign_firmware_data(dummy_data, dummy_privkey)

        expected_hash = hashlib.sha256(dummy_data).hexdigest()
        self.assertEqual(hash_hex, expected_hash)
        self.assertEqual(len(sig_hex), 128)  # 64 bytes = 128 hex chars


class TestVersionScript(unittest.TestCase):
    def test_generate_dev_version_pattern(self):
        fixed_time = datetime(2026, 9, 11, 14, 30, 45)
        dev_version = generate_dev_version(fixed_time)
        self.assertEqual(dev_version, "dev-20260911-143045")

        live_version = generate_dev_version()
        self.assertTrue(re.match(r"^dev-\d{8}-\d{6}$", live_version))

    def test_format_version_header_content(self):
        header = format_version_header("v2.1.0")
        self.assertIn('#define FIRMWARE_VERSION "v2.1.0"', header)
        self.assertIn("#pragma once", header)


if __name__ == "__main__":
    unittest.main()
