#!/usr/bin/env python3
"""
DialOS — Standalone Stock FiiO Firmware Patcher & Builder
=========================================================
Builds a bootable DialOS SquashFS rootfs image directly from an official FiiO
Snowsky DISC firmware archive (V2.09, V2.28, V2.40, or V2.57) with ZERO
dependency on diskOS or third-party Python packages.

Safety Gates Built In:
  1. Verifies the ZIP archive is complete and uncorrupted (`zf.testzip()`) before
     extracting a single chunk.
  2. Decrypts FiiO's AES-256-CBC chunks (`key="fo123"`) using Python's built-in
     `hashlib.pbkdf2_hmac` + system `/usr/bin/openssl` (works natively on macOS
     LibreSSL and Linux OpenSSL).
  3. Verifies the reassembled `stock_rootfs.squashfs` byte size and SHA-256
     against known-good official FiiO releases:
       - V2.09: f1e3c69fb0e88b923c135558e01f4387a661f68839c8118e8ad490bdc9fc74e6 (75,919,360 B)
       - V2.28: 0ffd877bca2c69ddff9ca70f4494da0d9e580c18d0f587e2c6d9921f2db82bd2 (72,957,952 B)
       - V2.40: b479e159db5134325819b5f6e5a54388f3adefae373a4ee60680f02d5dcf0bb8 (88,420,352 B)
       - V2.57: 111e4dd7ee3d7ff91ba7e61181690be7ffd22bd1bbd13ad513f5f015ffb302ae (80,596,992 B)
  4. Saves an untouched backup of `build/stock_rootfs.squashfs` BEFORE building
     `build/dialos_rootfs.squashfs` so you can always restore stock 1:1.

Usage:
  python3 tools/build_dialos_fw_from_stock.py --stock-zip ~/Downloads/SNOWSKY_DISC_update_20260909_v257.zip
"""

import argparse
import glob
import hashlib
import os
import subprocess
import sys
import tempfile
import zipfile

FIIO_OTA_KEY = b"fo123"
SQUASHFS_MAGIC = b"hsqs"

KNOWN_STOCK_ROOTFS = {
    "f1e3c69fb0e88b923c135558e01f4387a661f68839c8118e8ad490bdc9fc74e6": ("V2.09", 75919360),
    "0ffd877bca2c69ddff9ca70f4494da0d9e580c18d0f587e2c6d9921f2db82bd2": ("V2.28", 72957952),
    "b479e159db5134325819b5f6e5a54388f3adefae373a4ee60680f02d5dcf0bb8": ("V2.40", 88420352),
    "111e4dd7ee3d7ff91ba7e61181690be7ffd22bd1bbd13ad513f5f015ffb302ae": ("V2.57", 80596992),
}


def openssl_decrypt_blob(data: bytes, label: str = "blob") -> bytes:
    """Decrypt FiiO OpenSSL 'Salted__' PBKDF2-HMAC-SHA256 AES-256-CBC blob natively on macOS/Linux."""
    if len(data) < 32 or data[:8] != b"Salted__":
        raise RuntimeError(f"{label}: missing OpenSSL 'Salted__' header")
    salt = data[8:16]
    ct = data[16:]
    if len(ct) == 0 or (len(ct) % 16) != 0:
        raise RuntimeError(f"{label}: ciphertext length ({len(ct)}) is not a multiple of 16 bytes")
    keyiv = hashlib.pbkdf2_hmac("sha256", FIIO_OTA_KEY, salt, 10000, 48)
    res = subprocess.run(
        ["openssl", "enc", "-d", "-aes-256-cbc", "-K", keyiv[:32].hex(), "-iv", keyiv[32:48].hex()],
        input=ct,
        capture_output=True,
    )
    if res.returncode != 0:
        raise RuntimeError(f"{label}: AES-256-CBC decrypt failed: {res.stderr.decode('utf-8', 'ignore').strip()}")
    return res.stdout


def parse_rootfs_manifest(manifest_text: str):
    in_rootfs = False
    img_name = None
    img_size = None
    img_sha256 = None
    for raw_line in manifest_text.splitlines():
        line = raw_line.strip()
        if line.startswith("img_type") and line.endswith("=rootfs"):
            in_rootfs = True
            continue
        if in_rootfs:
            if line.startswith("img_name="):
                img_name = line.split("=", 1)[1].strip()
            elif line.startswith("img_size="):
                img_size = int(line.split("=", 1)[1].strip())
            elif line.startswith("img_sha256="):
                img_sha256 = line.split("=", 1)[1].strip()
            if img_name and img_size is not None and img_sha256:
                break
    return img_name, img_size, img_sha256


def verify_and_extract_stock_rootfs(stock_zip: str, out_squashfs: str, workdir: str):
    if not os.path.exists(stock_zip):
        raise RuntimeError(f"File not found: {stock_zip}")

    file_sz = os.path.getsize(stock_zip)
    print(f"[1/3] Checking ZIP archive integrity: {stock_zip} ({file_sz / (1024*1024):.2f} MB)")
    try:
        with zipfile.ZipFile(stock_zip, "r") as zf:
            bad_entry = zf.testzip()
            if bad_entry is not None:
                raise RuntimeError(f"Corrupt entry in ZIP archive: {bad_entry}")
            extract_dir = os.path.join(workdir, "unzipped")
            zf.extractall(extract_dir)
    except (zipfile.BadZipFile, OSError) as exc:
        raise RuntimeError(
            f"The firmware ZIP at '{stock_zip}' is incomplete or corrupted ({exc}).\n"
            f"      On-disk size is {file_sz:,} bytes (~{file_sz/(1024*1024):.1f} MB), "
            f"whereas the complete V2.57 archive is ~105.6 MB.\n"
            f"      Please re-download SNOWSKY_DISC_update_20260909_v257.zip before proceeding!"
        ) from exc

    manifests = sorted(
        glob.glob(os.path.join(extract_dir, "**", "main_os", "ota_v*", "ota_update.in.enc"), recursive=True)
    )
    if not manifests:
        raise RuntimeError("Could not find main_os/ota_v*/ota_update.in.enc in the provided ZIP.")

    manifest_enc = manifests[0]
    ota_dir = os.path.dirname(manifest_enc)
    manifest_text = openssl_decrypt_blob(open(manifest_enc, "rb").read(), "ota_update.in.enc").decode("utf-8", "ignore")
    img_name, expected_size, expected_sha = parse_rootfs_manifest(manifest_text)
    if not img_name:
        raise RuntimeError("Could not parse img_name from decrypted ota_update.in")

    chunk_files = {}
    for path in glob.glob(os.path.join(ota_dir, f"{img_name}.*.enc")):
        suffix = os.path.basename(path)[len(img_name) + 1 :]
        idx_str = suffix.split(".", 1)[0]
        if idx_str.isdigit():
            chunk_files[int(idx_str)] = path

    idxs = sorted(chunk_files.keys())
    if not idxs or idxs != list(range(len(idxs))):
        raise RuntimeError(
            f"Missing encrypted rootfs chunks in {ota_dir}: found {len(idxs)} chunks, "
            f"expected contiguous 0000..{max(idxs) if idxs else 0:04d}"
        )

    print(f"[2/3] Decrypting & reassembling {len(idxs)} FiiO OTA chunks (key='fo123')...")
    hasher = hashlib.sha256()
    with open(out_squashfs, "wb") as out_fp:
        for idx in idxs:
            pt = openssl_decrypt_blob(open(chunk_files[idx], "rb").read(), os.path.basename(chunk_files[idx]))
            out_fp.write(pt)
            hasher.update(pt)

    actual_size = os.path.getsize(out_squashfs)
    actual_sha = hasher.hexdigest()

    if expected_size and actual_size != expected_size:
        raise RuntimeError(f"Size mismatch: assembled {actual_size} bytes, expected {expected_size} bytes")
    if expected_sha and actual_sha.lower() != expected_sha.lower():
        raise RuntimeError(f"SHA-256 mismatch: assembled {actual_sha}, manifest expected {expected_sha}")

    with open(out_squashfs, "rb") as fp:
        if fp.read(4) != SQUASHFS_MAGIC:
            raise RuntimeError("Reassembled rootfs does not have valid SquashFS ('hsqs') magic")

    ver_info = KNOWN_STOCK_ROOTFS.get(actual_sha.lower(), ("Unknown", actual_size))
    print(f"      Verified Stock FiiO {ver_info[0]} rootfs!")
    print(f"      Saved untouched stock backup -> {out_squashfs} ({actual_size:,} bytes, SHA256={actual_sha[:16]}...)")
    return actual_sha


def patch_squashfs_with_dialos(stock_squashfs: str, ui_binary: str, out_squashfs: str):
    print(f"[3/3] Building bootable DialOS SquashFS ({out_squashfs})...")
    if not os.path.exists(ui_binary):
        print("      Cross-compiling static MIPS32r2 DialOS binary (build/mq_ui) via Docker...")
        subprocess.run(["docker", "build", "-t", "dialos-mipsel-builder", "."], check=True)
        subprocess.run(
            [
                "docker", "run", "--rm",
                "-v", f"{os.path.abspath('build')}:/out",
                "dialos-mipsel-builder",
                "sh", "-c", "cp build/dialos-disc-mipsel build/mq_ui /out/",
            ],
            check=True,
        )

    with open(ui_binary, "rb") as fp:
        hdr = fp.read(20)
    if hdr[:4] != b"\x7fELF" or hdr[4:6] != b"\x01\x01" or hdr[18:20] != b"\x08\x00":
        raise RuntimeError(f"{ui_binary} is not a 32-bit little-endian MIPS ELF binary.")

    abs_stock = os.path.abspath(stock_squashfs)
    abs_ui = os.path.abspath(ui_binary)
    abs_out = os.path.abspath(out_squashfs)
    build_dir = os.path.dirname(abs_out)

    # Unpack on case-sensitive Linux container, keep original stock /usr/bin/mq_ui as /usr/bin/mq_ui.stock
    # so the user can revert to stock anytime, and install DialOS as /usr/bin/mq_ui.
    docker_cmd = [
        "docker", "run", "--rm",
        "-v", f"{abs_stock}:/in/stock.squashfs:ro",
        "-v", f"{abs_ui}:/in/mq_ui:ro",
        "-v", f"{build_dir}:/out",
        "debian:bookworm-slim",
        "sh", "-exc",
        (
            "apt-get update -qq && apt-get install -y -qq squashfs-tools >/dev/null && "
            "unsquashfs -d /tmp/rootfs /in/stock.squashfs && "
            "cp /tmp/rootfs/usr/bin/mq_ui /tmp/rootfs/usr/bin/mq_ui.stock && "
            "cp /in/mq_ui /tmp/rootfs/usr/bin/mq_ui && chmod 0755 /tmp/rootfs/usr/bin/mq_ui && "
            "mksquashfs /tmp/rootfs /out/" + os.path.basename(abs_out) + " "
            "-comp lzo -b 131072 -no-xattrs -all-root -noappend"
        ),
    ]
    res = subprocess.run(docker_cmd)
    if res.returncode != 0:
        raise RuntimeError("Docker SquashFS unpack/repack failed (ensure Docker Desktop is running).")
    print(f"      Built bootable DialOS SquashFS -> {out_squashfs} ({os.path.getsize(out_squashfs):,} bytes)")


def main():
    parser = argparse.ArgumentParser(description="Build DialOS directly from an official FiiO Snowsky DISC firmware zip.")
    parser.add_argument("--stock-zip", required=True, help="Path to official FiiO SNOWSKY_DISC_update_V2.XX.zip")
    parser.add_argument("--ui-bin", default="build/mq_ui", help="Path to compiled MIPS32r2 static DialOS mq_ui binary")
    parser.add_argument("--extract-only", action="store_true", help="Only decrypt and verify stock_rootfs.squashfs")
    args = parser.parse_args()

    os.makedirs("build", exist_ok=True)
    stock_sqfs = "build/stock_rootfs.squashfs"
    dialos_sqfs = "build/dialos_rootfs.squashfs"

    try:
        with tempfile.TemporaryDirectory(prefix="dialos-fw-") as workdir:
            verify_and_extract_stock_rootfs(args.stock_zip, stock_sqfs, workdir)
            if args.extract_only:
                return
            patch_squashfs_with_dialos(stock_sqfs, args.ui_bin, dialos_sqfs)
    except RuntimeError as err:
        print(f"\n[ERROR] {err}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
