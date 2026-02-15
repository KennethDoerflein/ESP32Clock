#!/usr/bin/env python3
"""
sign_firmware.py - Sign ESP32Clock firmware for secure OTA updates

This script signs a firmware.bin file using an Ed25519 private key.
Used for manual signing when not using the GitHub Actions workflow.

Usage:
    python scripts/sign_firmware.py <firmware.bin> <private_key_hex>
    
Example:
    python scripts/sign_firmware.py .pio/build/esp32s3_n16r8/firmware.bin abc123...

Output:
    - Creates firmware.sig (signature file)
    - Creates firmware.sha256 (checksum file)
"""

import hashlib
import os
import sys

try:
    import nacl.signing
except ImportError:
    print("ERROR: PyNaCl library not installed.")
    print("Install with: pip install pynacl")
    sys.exit(1)


def main():
    if len(sys.argv) < 3:
        print("Usage: python sign_firmware.py <firmware.bin> <private_key_hex>")
        print()
        print("Generate keys first with: python scripts/generate_ota_keys.py")
        sys.exit(1)

    firmware_path = sys.argv[1]
    private_key_hex = sys.argv[2]

    if not os.path.exists(firmware_path):
        print(f"ERROR: Firmware file not found: {firmware_path}")
        sys.exit(1)

    # Parse private key
    try:
        key_bytes = bytes.fromhex(private_key_hex)
        if len(key_bytes) != 32:
            print(f"ERROR: Private key must be 32 bytes (64 hex chars), got {len(key_bytes)} bytes")
            sys.exit(1)
        signing_key = nacl.signing.SigningKey(key_bytes)
    except ValueError as e:
        print(f"ERROR: Invalid private key hex: {e}")
        sys.exit(1)

    # Read firmware
    print(f"Reading firmware: {firmware_path}")
    with open(firmware_path, "rb") as f:
        firmware_data = f.read()
    print(f"Firmware size: {len(firmware_data)} bytes")

    # Compute SHA-256 hash
    firmware_hash = hashlib.sha256(firmware_data).digest()
    hash_hex = firmware_hash.hex()
    print(f"SHA-256: {hash_hex}")

    # Sign the hash
    signed = signing_key.sign(firmware_hash)
    signature = signed.signature
    signature_hex = signature.hex()
    print(f"Signature: {signature_hex}")

    # Write signature file
    base_path = firmware_path.rsplit(".", 1)[0]
    sig_path = base_path + ".sig"
    sha_path = base_path + ".sha256"

    with open(sig_path, "w") as f:
        f.write(signature_hex)
    print(f"Signature written to: {sig_path}")

    with open(sha_path, "w") as f:
        f.write(hash_hex)
    print(f"Checksum written to: {sha_path}")

    print()
    print("Signing complete! Upload these files to your GitHub release:")
    print(f"  - {firmware_path}")
    print(f"  - {sig_path}")
    print(f"  - {sha_path}")


if __name__ == "__main__":
    main()
