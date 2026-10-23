#!/usr/bin/env python3
"""
DialOS macOS Native Installer & Recovery Tool (`./dialos-mac`)
==============================================================
100% macOS-native firmware builder, USB Mask-ROM flasher, and stock recovery
tool for the FiiO Snowsky DISC (Ingenic X2000 / Dual CS43131).

Tailored exclusively for macOS (Apple Silicon & Intel):
  - Uses macOS `ioreg -p IOUSB` to detect the Snowsky DISC in USB Mask-ROM mode
    (`VID=0xa108`, `PID=0xeaef`).
  - Uses macOS `openssl` + Python `hashlib.pbkdf2_hmac` to decrypt FiiO's
    `fo123` AES-256-CBC firmware chunks with zero third-party Python deps.
  - Uses macOS `hdiutil` to mount a temporary Case-Sensitive APFS sparse volume
    for extracting and repacking the Linux SquashFS (`-comp lzo -b 131072`).
  - Uses macOS `caffeinate -dimsu` to prevent macOS from sleeping during USB
    NAND programming.
  - Automatically backs up your untouched stock firmware to
    `~/Library/Application Support/DialOS/backups/` before building.

Commands:
  ./dialos-mac doctor
  ./dialos-mac build ~/Downloads/SNOWSKY_DISC_update_20260909_v257.zip
  ./dialos-mac flash
  ./dialos-mac restore-stock
"""

import argparse
import contextlib
import glob
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zipfile

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
VENDOR_DIR = os.path.join(REPO_ROOT, "macos", "vendor")
BUILD_DIR = os.path.join(REPO_ROOT, "build")
MACOS_BACKUP_DIR = os.path.expanduser("~/Library/Application Support/DialOS/backups")

FIIO_OTA_KEY = b"fo123"
SQUASHFS_MAGIC = b"hsqs"
NAND_IMG_SIZE = 100663296  # 768 x 128 KB NAND blocks (~96 MiB)
WRITER_SHA256 = "a3c3aa63e0c48f501181d542d9fa0526d36ded81e2e767f0c044af876674ea84"
PLAN_ADDR = "0xa0b00000"
FLASH_WAIT_SECS = "1080"

# Verified FiiO Snowsky DISC Stock Firmware Catalogue (Rootfs + On-Device Base Gate)
CATALOGUE = {
    "209": {
        "id": "v209-r17a",
        "name": "V2.09 (recovery 17)",
        "rootfs": (75919360, "f1e3c69fb0e88b923c135558e01f4387a661f68839c8118e8ad490bdc9fc74e6"),
        "main_kernel": (4415552, "f843cfcc28fe3c0d26bf27e35a2d8d595aa200ef82750200c888af5348f4f033"),
        "recovery_kernel": (4042816, "db8a2a7aaf286c381e8a760481fa13b3168a1716ef510834bdea03495a8876df"),
        "recovery_rootfs": (14151680, "367ea3159ad63c63ea8b2d752d3fa1f4d5fc292171db846c82fcfec360389331"),
    },
    "228": {
        "id": "v228-r17",
        "name": "V2.28 (recovery 17)",
        "rootfs": (72957952, "0ffd877bca2c69ddff9ca70f4494da0d9e580c18d0f587e2c6d9921f2db82bd2"),
        "main_kernel": (4423744, "f3c1820f5258a319eca820e0e6d64a4bac764ae36a9eac5de05f255d8910cfeb"),
        "recovery_kernel": (4042816, "5da45c40894c2101249c91063bc164a67f850e1f0b3e7bdf39856e940c26b314"),
        "recovery_rootfs": (14151680, "b093b73076bf4e6040b0d90f8e1a042818db3a8417ea6da520d3de4c10dbb4c5"),
    },
    "240": {
        "id": "v240-r17",
        "name": "V2.40 (recovery 17)",
        "rootfs": (88420352, "b479e159db5134325819b5f6e5a54388f3adefae373a4ee60680f02d5dcf0bb8"),
        "main_kernel": (4423744, "5ce48bb5b5e2149777ed367b4bc68146ff132ec45e3a461e62c916d0a3ab2a7a"),
        "recovery_kernel": (4042816, "5da45c40894c2101249c91063bc164a67f850e1f0b3e7bdf39856e940c26b314"),
        "recovery_rootfs": (14151680, "b093b73076bf4e6040b0d90f8e1a042818db3a8417ea6da520d3de4c10dbb4c5"),
    },
    "257": {
        "id": "v257-r18",
        "name": "V2.57 (recovery 18)",
        "rootfs": (80596992, "111e4dd7ee3d7ff91ba7e61181690be7ffd22bd1bbd13ad513f5f015ffb302ae"),
        "main_kernel": (4423744, "1a9484efcb64c391b23910306e867cb628af07e059d0d1ccc13e85269e030ed2"),
        "recovery_kernel": (4051008, "9561ccb7ad1bd752eb421509847760a5a88d4ccb49c8a75eb6710b1e9aaf9113"),
        "recovery_rootfs": (16527360, "73b0f8e834d36c0fae9b23f8832df59195bd0f4252a42a1afe4dd15ce0898780"),
    },
}
BASE_PARTS = ("main_kernel", "recovery_kernel", "recovery_rootfs")


def macos_detect_maskrom() -> int:
    """Use macOS native `ioreg` to count connected Ingenic X2000 Mask-ROM devices (a108:eaef)."""
    res = subprocess.run(["/usr/sbin/ioreg", "-p", "IOUSB", "-w0", "-l"], capture_output=True, text=True)
    if res.returncode != 0:
        return 0
    count = 0
    for block in res.stdout.split("+-o "):
        if '"idVendor" = 41224' in block and '"idProduct" = 60143' in block:
            count += 1
    return count


def openssl_decrypt_blob(data: bytes, label: str = "blob") -> bytes:
    """Decrypt FiiO OpenSSL 'Salted__' PBKDF2-HMAC-SHA256 AES-256-CBC blob using macOS `/usr/bin/openssl`."""
    if len(data) < 32 or data[:8] != b"Salted__":
        raise RuntimeError(f"{label}: missing OpenSSL 'Salted__' header")
    salt = data[8:16]
    ct = data[16:]
    if len(ct) == 0 or (len(ct) % 16) != 0:
        raise RuntimeError(f"{label}: ciphertext length ({len(ct)}) is not a multiple of 16 bytes")
    keyiv = hashlib.pbkdf2_hmac("sha256", FIIO_OTA_KEY, salt, 10000, 48)
    res = subprocess.run(
        ["/usr/bin/openssl", "enc", "-d", "-aes-256-cbc", "-K", keyiv[:32].hex(), "-iv", keyiv[32:48].hex()],
        input=ct,
        capture_output=True,
    )
    if res.returncode != 0:
        raise RuntimeError(f"{label}: AES-256-CBC decrypt failed: {res.stderr.decode('utf-8', 'ignore').strip()}")
    return res.stdout


@contextlib.contextmanager
def macos_case_sensitive_apfs(workdir: str):
    """Mount a temporary macOS Case-sensitive APFS sparse image via `hdiutil` and always detach it."""
    base = os.path.join(workdir, "dialos_cs")
    sparse_img = base + ".sparseimage"
    mnt = os.path.join(workdir, "mnt_cs")
    os.makedirs(mnt, exist_ok=True)
    print("      Mounting temporary macOS Case-sensitive APFS volume via hdiutil...")
    try:
        subprocess.run(
            ["/usr/bin/hdiutil", "create", "-size", "2g", "-type", "SPARSE", "-fs", "Case-sensitive APFS",
             "-volname", "DialOSBuild", base],
            check=True,
            capture_output=True,
        )
        subprocess.run(
            ["/usr/bin/hdiutil", "attach", sparse_img, "-mountpoint", mnt, "-nobrowse", "-owners", "off"],
            check=True,
            capture_output=True,
        )
        yield mnt
    finally:
        for i in range(5):
            if not os.path.ismount(mnt):
                break
            subprocess.run(
                ["/usr/bin/hdiutil", "detach", mnt] + (["-force"] if i >= 2 else []),
                capture_output=True,
            )
            time.sleep(0.3)
        if os.path.exists(sparse_img):
            os.remove(sparse_img)


def catalogue_fingerprint() -> bytes:
    h = hashlib.sha256()
    for ver in sorted(CATALOGUE):
        e = CATALOGUE[ver]
        h.update(f"{ver}|{e['id']}|".encode())
        for p in BASE_PARTS:
            length, sha = e[p]
            h.update(f"{length}:{sha}|".encode())
    return h.digest()


def build_dgsb_plan(main_os_ver: str, padded_image_path: str) -> bytes:
    """Build the 272-byte DGSB base-gate plan for my_write6_dram.bin at 0xa0b00000."""
    base = CATALOGUE[main_os_ver]
    h = hashlib.sha256()
    size = 0
    with open(padded_image_path, "rb") as fp:
        while True:
            chunk = fp.read(1 << 20)
            if not chunk:
                break
            h.update(chunk)
            size += len(chunk)

    def digest_words(hexdigest: str):
        b = bytes.fromhex(hexdigest)
        return [int.from_bytes(b[4 * i : 4 * i + 4], "big") for i in range(8)]

    w = [0x42534744, 1, 68 * 4, 0]  # BLOB_MAGIC="DGSB", schema=1, 272 bytes
    w += list(struct.unpack("<4I", base["id"].encode().ljust(16, b"\0")[:16]))
    for p in BASE_PARTS:
        length, sha = base[p]
        w += [length] + digest_words(sha)
    w += [size] + digest_words(h.hexdigest())
    w += list(struct.unpack(">8I", catalogue_fingerprint()))
    w += [0] * 8
    body = struct.pack("<60I", *w)
    w += list(struct.unpack(">8I", hashlib.sha256(body).digest()))
    return struct.pack("<68I", *w)


def pad_to_nand_size(src_squashfs: str, dst_bin: str):
    sz = os.path.getsize(src_squashfs)
    if sz > NAND_IMG_SIZE:
        raise RuntimeError(f"SquashFS ({sz} B) exceeds 96 MiB NAND partition ({NAND_IMG_SIZE} B)")
    shutil.copyfile(src_squashfs, dst_bin)
    with open(dst_bin, "ab") as fp:
        fp.truncate(NAND_IMG_SIZE)


def cmd_doctor(_args):
    print("==================================================================")
    print(" DialOS macOS Installer — System & Hardware Readiness (`doctor`)")
    print("==================================================================")
    if sys.platform != "darwin":
        print("[FAIL] This installer is tailored exclusively for macOS.")
        sys.exit(1)

    arch = os.uname().machine
    print(f"  macOS Host Architecture : {arch} (Darwin)")

    tools = {
        "usbboot (X2000 loader)": os.path.join(VENDOR_DIR, "usbboot"),
        "mksquashfs (LZO)": os.path.join(VENDOR_DIR, "mksquashfs"),
        "unsquashfs (LZO)": os.path.join(VENDOR_DIR, "unsquashfs"),
        "disc_spl_lpddr3.bin": os.path.join(VENDOR_DIR, "disc_spl_lpddr3.bin"),
        "my_write6_dram.bin": os.path.join(VENDOR_DIR, "my_write6_dram.bin"),
        "DialOS MIPS32r2 mq_ui": os.path.join(VENDOR_DIR, "mq_ui"),
    }
    all_ok = True
    for label, path in tools.items():
        exists = os.path.exists(path)
        status = "READY" if exists else "MISSING"
        if not exists:
            all_ok = False
        print(f"  {label:<24}: [{status}] ({os.path.relpath(path, REPO_ROOT)})")

    maskrom_n = macos_detect_maskrom()
    if maskrom_n == 1:
        print("  Snowsky DISC USB Mode   : [CONNECTED] 1 device in Mask-ROM (a108:eaef)")
    elif maskrom_n == 0:
        print("  Snowsky DISC USB Mode   : [NOT CONNECTED] (Power OFF -> Hold Vol Down -> Plug USB)")
    else:
        print(f"  Snowsky DISC USB Mode   : [WARNING] {maskrom_n} Ingenic devices detected")

    os.makedirs(MACOS_BACKUP_DIR, exist_ok=True)
    backups = sorted(glob.glob(os.path.join(MACOS_BACKUP_DIR, "stock_V*.bin")))
    if backups:
        for b in backups:
            print(f"  Saved Stock Backup      : [AVAILABLE] {b}")
    else:
        print(f"  Saved Stock Backup      : [NONE YET] (Created automatically on `./dialos-mac build`)")

    print("==================================================================")
    if not all_ok:
        sys.exit(1)


def cmd_build(args):
    stock_zip = os.path.expanduser(args.firmware_zip)
    print("==================================================================")
    print(" DialOS macOS Installer — Building Flashable Image from Stock ZIP")
    print("==================================================================")
    if not os.path.exists(stock_zip):
        raise RuntimeError(f"Firmware ZIP not found: {stock_zip}")

    file_sz = os.path.getsize(stock_zip)
    print(f"[1/4] Verifying official FiiO ZIP archive ({file_sz / (1024*1024):.1f} MB)...")
    try:
        with zipfile.ZipFile(stock_zip, "r") as zf:
            bad = zf.testzip()
            if bad is not None:
                raise RuntimeError(f"Corrupt ZIP entry: {bad}")
    except (zipfile.BadZipFile, OSError) as exc:
        raise RuntimeError(
            f"The firmware ZIP at '{stock_zip}' is incomplete or corrupted ({exc}).\n"
            f"      Current size on disk is {file_sz:,} bytes (~{file_sz/(1024*1024):.1f} MB), "
            f"whereas the complete V2.57 archive is ~105.6 MB.\n"
            f"      Please re-download the complete FiiO firmware ZIP and run again."
        ) from exc

    ui_bin = os.path.join(VENDOR_DIR, "mq_ui")
    if not os.path.exists(ui_bin):
        raise RuntimeError("Missing macos/vendor/mq_ui binary. Run `./dialos-mac doctor` first.")

    os.makedirs(BUILD_DIR, exist_ok=True)
    os.makedirs(MACOS_BACKUP_DIR, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="dialos-mac-") as workdir:
        unzip_dir = os.path.join(workdir, "unzip")
        with zipfile.ZipFile(stock_zip, "r") as zf:
            zf.extractall(unzip_dir)

        mans = sorted(glob.glob(os.path.join(unzip_dir, "**", "main_os", "ota_v*", "ota_update.in.enc"), recursive=True))
        if not mans:
            raise RuntimeError("No main_os/ota_v*/ota_update.in.enc found in ZIP.")
        ota_dir = os.path.dirname(mans[0])
        mver_match = re.search(r"ota_v(\d+)", ota_dir)
        if not mver_match or mver_match.group(1) not in CATALOGUE:
            raise RuntimeError(f"Unsupported firmware version in {ota_dir} (supported: V2.09, V2.28, V2.40, V2.57)")
        mver = mver_match.group(1)
        exp_sz, exp_sha = CATALOGUE[mver]["rootfs"]

        chunk_map = {}
        for f in glob.glob(os.path.join(ota_dir, "rootfs.squashfs.*.enc")):
            rest = os.path.basename(f)[len("rootfs.squashfs.") :]
            idx_s = rest.split(".", 1)[0]
            if idx_s.isdigit():
                chunk_map[int(idx_s)] = f

        idxs = sorted(chunk_map.keys())
        print(f"[2/4] Decrypting {len(idxs)} FiiO {CATALOGUE[mver]['name']} chunks natively on macOS...")
        stock_sqfs = os.path.join(BUILD_DIR, f"stock_V{mver}.squashfs")
        h = hashlib.sha256()
        with open(stock_sqfs, "wb") as out_fp:
            for idx in idxs:
                pt = openssl_decrypt_blob(open(chunk_map[idx], "rb").read(), f"chunk {idx:04d}")
                out_fp.write(pt)
                h.update(pt)

        if os.path.getsize(stock_sqfs) != exp_sz or h.hexdigest() != exp_sha:
            raise RuntimeError(f"Stock rootfs SHA-256 mismatch (got {h.hexdigest()[:16]}..., expected {exp_sha[:16]}...)")

        stock_backup_bin = os.path.join(MACOS_BACKUP_DIR, f"stock_V{mver}_nand.bin")
        pad_to_nand_size(stock_sqfs, stock_backup_bin)
        shutil.copyfile(stock_backup_bin, os.path.join(BUILD_DIR, "stock_nand.bin"))
        print(f"      Saved verified stock recovery image -> {stock_backup_bin}")

        print("[3/4] Patching SquashFS natively on macOS (hdiutil Case-Sensitive APFS + LZO)...")
        unsq = os.path.join(VENDOR_DIR, "unsquashfs")
        mksq = os.path.join(VENDOR_DIR, "mksquashfs")
        dialos_sqfs = os.path.join(BUILD_DIR, f"dialos_V{mver}.squashfs")

        with macos_case_sensitive_apfs(workdir) as cs_mnt:
            rf_dir = os.path.join(cs_mnt, "rootfs")
            subprocess.run([unsq, "-processors", "1", "-d", rf_dir, stock_sqfs], check=True)
            stock_ui_path = os.path.join(rf_dir, "usr", "bin", "mq_ui")
            backup_ui_path = os.path.join(rf_dir, "usr", "bin", "mq_ui.stock")
            if os.path.exists(stock_ui_path):
                shutil.copy2(stock_ui_path, backup_ui_path)
            shutil.copyfile(ui_bin, stock_ui_path)
            os.chmod(stock_ui_path, 0o755)

            if os.path.exists(dialos_sqfs):
                os.remove(dialos_sqfs)
            subprocess.run(
                [mksq, rf_dir, dialos_sqfs, "-comp", "lzo", "-b", "131072",
                 "-no-xattrs", "-all-root", "-noappend", "-processors", "1"],
                check=True,
            )

        out_nand_bin = os.path.join(BUILD_DIR, "dialos_nand.bin")
        pad_to_nand_size(dialos_sqfs, out_nand_bin)
        with open(os.path.join(BUILD_DIR, "dialos_nand.ver"), "w") as vf:
            vf.write(mver)

        print(f"[4/4] Success! Built macOS flashable NAND image:")
        print(f"      DialOS Image : {out_nand_bin} ({os.path.getsize(out_nand_bin):,} B, V{mver})")
        print(f"      Stock Backup : {stock_backup_bin}")
        print("\nNext step: Power OFF Snowsky DISC, hold Volume Down, plug in USB, and run:")
        print("  ./dialos-mac flash")


def run_usbboot_flash(image_path: str, mver: str, label: str):
    print("==================================================================")
    print(f" DialOS macOS Flasher — {label} (Base: {CATALOGUE[mver]['name']})")
    print("==================================================================")
    if not os.path.exists(image_path) or os.path.getsize(image_path) != NAND_IMG_SIZE:
        raise RuntimeError(f"Invalid NAND image {image_path} (must be {NAND_IMG_SIZE} bytes)")

    n_dev = macos_detect_maskrom()
    if n_dev == 0:
        raise RuntimeError(
            "No Snowsky DISC detected in USB Mask-ROM mode (a108:eaef).\n"
            "      1. Power the Snowsky DISC completely OFF.\n"
            "      2. Hold Down the Volume-Down button and plug in the USB-C cable (screen stays black).\n"
            "      3. Re-run this command."
        )
    if n_dev > 1:
        raise RuntimeError(f"Found {n_dev} Mask-ROM devices; disconnect other Ingenic devices first.")

    usbboot = os.path.join(VENDOR_DIR, "usbboot")
    spl = os.path.join(VENDOR_DIR, "disc_spl_lpddr3.bin")
    writer = os.path.join(VENDOR_DIR, "my_write6_dram.bin")
    if hashlib.sha256(open(writer, "rb").read()).hexdigest() != WRITER_SHA256:
        raise RuntimeError("my_write6_dram.bin SHA-256 mismatch; refusing to flash.")

    with tempfile.TemporaryDirectory(prefix="dialos-usb-") as tmp:
        plan_path = os.path.join(tmp, "plan.bin")
        poison_path = os.path.join(tmp, "poison.bin")
        dbg_path = os.path.join(tmp, "dbg.bin")
        log_path = os.path.join(BUILD_DIR, "macos_flash.log")

        with open(plan_path, "wb") as fp:
            fp.write(build_dgsb_plan(mver, image_path))
        with open(poison_path, "wb") as fp:
            fp.write(b"\xee" * 128)

        cmd = [
            "/usr/bin/caffeinate", "-dimsu",
            usbboot, "-v", "--cpu", "x2000", "--stage1", spl, "--wait", "2",
            "--addr", "0xa0c00000", "--download", writer,
            "--addr", "0xa1000000", "--download", os.path.abspath(image_path),
            "--addr", PLAN_ADDR, "--download", plan_path,
            "--addr", "0xa0a00000", "--download", poison_path,
            "--start1", "0xa0c00030", "--wait", FLASH_WAIT_SECS,
            "--addr", "0xa0a00000", "--length", "0x400", "--upload", dbg_path,
        ]
        print("  Keeping Mac awake via `caffeinate -dimsu`...")
        print("  Flashing over USB Mask-ROM (~18-20 minutes). DO NOT unplug USB!")
        print(f"  Live log: {log_path}")
        with open(log_path, "w") as logf:
            subprocess.run(cmd, stdout=logf, stderr=subprocess.STDOUT, check=True)

        if not os.path.exists(dbg_path) or os.path.getsize(dbg_path) < 1024:
            raise RuntimeError("No debug readback received from device! Wait 25 minutes before unplugging.")

        w = struct.unpack("<256I", open(dbg_path, "rb").read(1024))
        magic, done, result, gate_res = w[0], w[9], w[16], w[128]
        if magic == 0x4006E006 and done == 0x55555555 and result == 0x600DF10C and gate_res == 1:
            print("\n[SUCCESS] NAND flash & readback verification passed! Power-cycle your Snowsky DISC.")
        else:
            raise RuntimeError(
                f"Writer reported non-success: magic=0x{magic:08X} done=0x{done:08X} "
                f"result=0x{result:08X} gate=0x{gate_res:08X} (check {log_path})"
            )


def cmd_flash(_args):
    ver_file = os.path.join(BUILD_DIR, "dialos_nand.ver")
    img_file = os.path.join(BUILD_DIR, "dialos_nand.bin")
    if not os.path.exists(img_file) or not os.path.exists(ver_file):
        raise RuntimeError("No built image found. Run `./dialos-mac build <firmware.zip>` first.")
    mver = open(ver_file).read().strip()
    run_usbboot_flash(img_file, mver, "Installing DialOS v1.0.0 (Fjäll)")


def cmd_restore_stock(_args):
    backups = sorted(glob.glob(os.path.join(MACOS_BACKUP_DIR, "stock_V*_nand.bin")))
    if not backups:
        raise RuntimeError(
            f"No saved stock backup in {MACOS_BACKUP_DIR}.\n"
            "      Run `./dialos-mac build <official_fiio_update.zip>` first to extract the stock image."
        )
    latest = backups[-1]
    m = re.search(r"stock_V(\d+)_nand\.bin", os.path.basename(latest))
    mver = m.group(1) if m else "257"
    run_usbboot_flash(latest, mver, "Restoring 100% Stock FiiO Firmware")


def main():
    parser = argparse.ArgumentParser(description="DialOS macOS Native Installer & Recovery Tool")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("doctor", help="Check macOS tools, USB Mask-ROM connection, and saved stock backups")
    p_build = sub.add_parser("build", help="Verify official FiiO ZIP, save stock backup, and build DialOS NAND image")
    p_build.add_argument("firmware_zip", help="Path to official FiiO SNOWSKY_DISC_update_V2.XX.zip")
    sub.add_parser("flash", help="Flash built DialOS image to Snowsky DISC in USB Mask-ROM mode")
    sub.add_parser("restore-stock", help="Restore saved stock FiiO firmware backup in USB Mask-ROM mode")

    args = parser.parse_args()
    try:
        if args.command == "doctor":
            cmd_doctor(args)
        elif args.command == "build":
            cmd_build(args)
        elif args.command == "flash":
            cmd_flash(args)
        elif args.command == "restore-stock":
            cmd_restore_stock(args)
    except RuntimeError as err:
        print(f"\n[ERROR] {err}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
