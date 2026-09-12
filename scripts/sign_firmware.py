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
    nacl = None


def compute_firmware_sha256(firmware_data: bytes) -> str:
    """Compute hex-encoded SHA-256 checksum of firmware binary bytes."""
    return hashlib.sha256(firmware_data).hexdigest()


def get_artifact_paths(firmware_path: str) -> tuple:
    """Get the corresponding .sig and .sha256 artifact paths for a firmware path."""
    base_path = firmware_path.rsplit(".", 1)[0]
    return f"{base_path}.sig", f"{base_path}.sha256"


def parse_private_key(private_key_hex: str):
    """Parse private key from hex string and return nacl SigningKey."""
    if nacl is None:
        raise RuntimeError("PyNaCl library not installed. Install with: pip install pynacl")

    key_bytes = bytes.fromhex(private_key_hex)
    if len(key_bytes) != 32:
        raise ValueError(f"Private key must be 32 bytes (64 hex chars), got {len(key_bytes)} bytes")
    return nacl.signing.SigningKey(key_bytes)


def sign_hash(firmware_hash_bytes: bytes, signing_key) -> str:
    """Sign a 32-byte hash using an Ed25519 signing key and return hex signature."""
    signed = signing_key.sign(firmware_hash_bytes)
    return signed.signature.hex()


def sign_firmware_data(firmware_data: bytes, private_key_hex: str) -> tuple:
    """Compute SHA-256 and Ed25519 signature for raw firmware data. Returns (hash_hex, sig_hex)."""
    firmware_hash_bytes = hashlib.sha256(firmware_data).digest()
    hash_hex = firmware_hash_bytes.hex()
    signing_key = parse_private_key(private_key_hex)
    sig_hex = sign_hash(firmware_hash_bytes, signing_key)
    return hash_hex, sig_hex


def main():
    if len(sys.argv) < 3:
        print("Usage: python sign_firmware.py <firmware.bin> <private_key_hex>")
        print()
        print("Generate keys first with: python scripts/generate_ota_keys.py")
        sys.exit(1)

    if nacl is None:
        print("ERROR: PyNaCl library not installed.")
        print("Install with: pip install pynacl")
        sys.exit(1)

    firmware_path = sys.argv[1]
    private_key_hex = sys.argv[2]

    if not os.path.exists(firmware_path):
        print(f"ERROR: Firmware file not found: {firmware_path}")
        sys.exit(1)

    # Read firmware
    print(f"Reading firmware: {firmware_path}")
    with open(firmware_path, "rb") as f:
        firmware_data = f.read()
    print(f"Firmware size: {len(firmware_data)} bytes")

    try:
        hash_hex, signature_hex = sign_firmware_data(firmware_data, private_key_hex)
    except (ValueError, RuntimeError) as e:
        print(f"ERROR: {e}")
        sys.exit(1)

    print(f"SHA-256: {hash_hex}")
    print(f"Signature: {signature_hex}")

    # Write signature and checksum files
    sig_path, sha_path = get_artifact_paths(firmware_path)

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
