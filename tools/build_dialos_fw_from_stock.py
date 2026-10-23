#!/usr/bin/env python3
"""
DialOS — Standalone Stock FiiO Firmware Patcher & OTA Zip Builder
==================================================================
Builds a bootable DialOS firmware image directly from an official FiiO
Snowsky DISC firmware archive (V2.09, V2.28, V2.40, or V2.57) with ZERO
dependency on diskOS.

How Stock FiiO Snowsky DISC Firmware Works:
  1. Official FiiO update zips (`SNOWSKY_DISC_update_V2.XX.zip`) contain
     `main_os/ota_v*/ota_update.in.enc` and encrypted SquashFS chunks
     (`rootfs.squashfs.NNNN.<sha256>.enc`).
  2. Each `.enc` file is encrypted with standard OpenSSL AES-256-CBC
     (`-pbkdf2 -iter 10000`) using FiiO's fixed OTA key `"fo123"` (no RSA/ECDSA
     signature is used by stock OTA).
  3. Inside `rootfs.squashfs` (LZO-compressed, 128 KB block size), the stock UI
     binary is `/usr/bin/mq_ui`, launched at boot by `/usr/project/fiio_init.sh`.

What This Tool Produces (from a 100% stock FiiO .zip):
  - `build/stock_rootfs.squashfs`       : Untouched backup of your stock FiiO rootfs
  - `build/dialos_rootfs.squashfs`      : Patched SquashFS with DialOS as `/usr/bin/mq_ui`
  - `build/SNOWSKY_DISC_DialOS_ota.zip` : Repacked FiiO SD-card OTA `.zip` (chunked +
                                          re-encrypted with `"fo123"` and updated SHA-256
                                          manifest) for stock microSD System Update.

Usage:
  python3 tools/build_dialos_fw_from_stock.py --stock-zip SNOWSKY_DISC_update_V2.57.zip
"""

import argparse
import glob
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

FIIO_OTA_KEY = "fo123"
SQUASHFS_MAGIC = b"hsqs"


def openssl_decrypt(enc_path: str) -> bytes:
    """Decrypt a FiiO .enc chunk using system `openssl enc -d -aes-256-cbc -pbkdf2`."""
    res = subprocess.run(
        ["openssl", "enc", "-d", "-aes-256-cbc", "-pbkdf2", "-k", FIIO_OTA_KEY, "-in", enc_path],
        capture_output=True,
    )
    if res.returncode != 0:
        raise RuntimeError(f"OpenSSL decrypt failed on {enc_path}: {res.stderr.decode('utf-8', 'ignore')}")
    return res.stdout


def openssl_encrypt(raw_bytes: bytes, out_enc_path: str) -> None:
    """Encrypt a chunk using FiiO's exact `openssl enc -aes-256-cbc -pbkdf2 -k fo123` format."""
    res = subprocess.run(
        ["openssl", "enc", "-e", "-aes-256-cbc", "-pbkdf2", "-k", FIIO_OTA_KEY, "-out", out_enc_path],
        input=raw_bytes,
        capture_output=True,
    )
    if res.returncode != 0:
        raise RuntimeError(f"OpenSSL encrypt failed for {out_enc_path}: {res.stderr.decode('utf-8', 'ignore')}")


def parse_rootfs_manifest(manifest_text: str):
    in_rootfs = False
    img_name = None
    img_size = None
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
            if img_name and img_size is not None:
                break
    return img_name, img_size


def extract_stock_rootfs(stock_zip: str, out_squashfs: str, workdir: str):
    print(f"[1/4] Extracting official FiiO firmware archive: {stock_zip}")
    extract_dir = os.path.join(workdir, "unzipped")
    with zipfile.ZipFile(stock_zip, "r") as zf:
        zf.extractall(extract_dir)

    manifests = sorted(
        glob.glob(os.path.join(extract_dir, "**", "main_os", "ota_v*", "ota_update.in.enc"), recursive=True)
    )
    if not manifests:
        raise RuntimeError("Could not find main_os/ota_v*/ota_update.in.enc in the provided zip.")

    manifest_enc = manifests[0]
    ota_dir = os.path.dirname(manifest_enc)
    manifest_text = openssl_decrypt(manifest_enc).decode("utf-8", "ignore")
    img_name, expected_size = parse_rootfs_manifest(manifest_text)
    if not img_name:
        raise RuntimeError("Could not parse img_name from decrypted ota_update.in")

    chunk_files = {}
    for path in glob.glob(os.path.join(ota_dir, f"{img_name}.*.enc")):
        suffix = os.path.basename(path)[len(img_name) + 1 :]
        idx_str = suffix.split(".", 1)[0]
        if idx_str.isdigit():
            chunk_files[int(idx_str)] = path

    if not chunk_files:
        raise RuntimeError(f"No encrypted chunks found for {img_name} in {ota_dir}")

    print(f"[2/4] Decrypting & reassembling {len(chunk_files)} FiiO OTA chunks (key='fo123')...")
    with open(out_squashfs, "wb") as out_fp:
        for idx in sorted(chunk_files.keys()):
            out_fp.write(openssl_decrypt(chunk_files[idx]))

    actual_size = os.path.getsize(out_squashfs)
    if expected_size and actual_size != expected_size:
        raise RuntimeError(f"Size mismatch: assembled {actual_size} bytes, expected {expected_size} bytes")

    with open(out_squashfs, "rb") as fp:
        if fp.read(4) != SQUASHFS_MAGIC:
            raise RuntimeError("Reassembled rootfs does not have valid SquashFS ('hsqs') magic")

    sha256 = hashlib.sha256(open(out_squashfs, "rb").read()).hexdigest()
    print(f"      Saved stock backup -> {out_squashfs} ({actual_size} bytes, SHA256={sha256[:16]}...)")
    return extract_dir, ota_dir, manifest_text, img_name, len(chunk_files)


def patch_squashfs_with_dialos(stock_squashfs: str, ui_binary: str, out_squashfs: str):
    print(f"[3/4] Replacing stock /usr/bin/mq_ui with DialOS ({ui_binary})...")
    with open(ui_binary, "rb") as fp:
        hdr = fp.read(20)
    if hdr[:4] != b"\x7fELF" or hdr[4:6] != b"\x01\x01" or hdr[18:20] != b"\x08\x00":
        raise RuntimeError(
            f"{ui_binary} is not a 32-bit little-endian MIPS ELF binary. "
            "Build it first via Docker (`docker build -t dialos-mipsel .`)."
        )

    # Use Docker (which has case-sensitive Linux filesystem + LZO squashfs-tools)
    abs_stock = os.path.abspath(stock_squashfs)
    abs_ui = os.path.abspath(ui_binary)
    abs_out = os.path.abspath(out_squashfs)
    build_dir = os.path.dirname(abs_out)

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
            "cp /in/mq_ui /tmp/rootfs/usr/bin/mq_ui && chmod 0755 /tmp/rootfs/usr/bin/mq_ui && "
            "mksquashfs /tmp/rootfs /out/" + os.path.basename(abs_out) + " "
            "-comp lzo -b 131072 -no-xattrs -all-root -noappend"
        ),
    ]
    res = subprocess.run(docker_cmd)
    if res.returncode != 0:
        raise RuntimeError("Docker SquashFS unpack/repack failed (ensure Docker Desktop is running).")
    print(f"      Built bootable DialOS SquashFS -> {out_squashfs} ({os.path.getsize(out_squashfs)} bytes)")


def repack_fiio_ota_zip(extract_dir: str, ota_dir: str, manifest_text: str, img_name: str,
                        num_orig_chunks: int, dialos_squashfs: str, out_zip: str):
    print(f"[4/4] Repacking FiiO SD-card OTA archive -> {out_zip}...")
    for old_enc in glob.glob(os.path.join(ota_dir, f"{img_name}.*.enc")):
        os.remove(old_enc)

    raw_data = open(dialos_squashfs, "rb").read()
    new_size = len(raw_data)
    chunk_size = max(1 << 20, (new_size + num_orig_chunks - 1) // max(1, num_orig_chunks))

    offset = 0
    idx = 0
    while offset < new_size:
        piece = raw_data[offset : offset + chunk_size]
        piece_sha = hashlib.sha256(piece).hexdigest()
        enc_name = f"{img_name}.{idx:04d}.{piece_sha}.enc"
        openssl_encrypt(piece, os.path.join(ota_dir, enc_name))
        offset += len(piece)
        idx += 1

    # Update img_size in ota_update.in and re-encrypt
    updated_manifest = re.sub(
        r"(img_type\s*=\s*rootfs[\s\S]*?img_size\s*=\s*)\d+",
        rf"\g<1>{new_size}",
        manifest_text,
        count=1,
    )
    openssl_encrypt(updated_manifest.encode("utf-8"), os.path.join(ota_dir, "ota_update.in.enc"))

    with zipfile.ZipFile(out_zip, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for root, _dirs, files in os.walk(extract_dir):
            for file in sorted(files):
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, extract_dir)
                zf.write(full_path, rel_path)

    print(f"      Done! Copy {out_zip} to a microSD card to install on a stock Snowsky DISC.")


def main():
    parser = argparse.ArgumentParser(description="Build DialOS directly from an official FiiO Snowsky DISC firmware zip.")
    parser.add_argument("--stock-zip", required=True, help="Path to official FiiO SNOWSKY_DISC_update_V2.XX.zip")
    parser.add_argument("--ui-bin", default="build/mq_ui", help="Path to compiled MIPS32r2 static DialOS mq_ui binary")
    parser.add_argument("--extract-only", action="store_true", help="Only decrypt and extract stock_rootfs.squashfs")
    args = parser.parse_args()

    os.makedirs("build", exist_ok=True)
    stock_sqfs = "build/stock_rootfs.squashfs"
    dialos_sqfs = "build/dialos_rootfs.squashfs"
    out_zip = "build/SNOWSKY_DISC_DialOS_v1.0.0_update.zip"

    with tempfile.TemporaryDirectory(prefix="dialos-fw-") as workdir:
        extract_dir, ota_dir, manifest_text, img_name, num_chunks = extract_stock_rootfs(
            args.stock_zip, stock_sqfs, workdir
        )
        if args.extract_only:
            return
        patch_squashfs_with_dialos(stock_sqfs, args.ui_bin, dialos_sqfs)
        repack_fiio_ota_zip(extract_dir, ota_dir, manifest_text, img_name, num_chunks, dialos_sqfs, out_zip)


if __name__ == "__main__":
    main()
