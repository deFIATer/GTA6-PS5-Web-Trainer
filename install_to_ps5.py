#!/usr/bin/env python3
"""One-click FTP installer for GTA VI Web Trainer on PS5 (etaHEN + ShadowMount)."""

import argparse
import ftplib
from pathlib import Path


def ensure_dir(ftp: ftplib.FTP, path: str) -> None:
    cur = ""
    for part in path.strip("/").split("/"):
        cur += "/" + part
        try:
            ftp.mkd(cur)
        except Exception:
            pass


def upload(ftp: ftplib.FTP, local: Path, remote: str) -> None:
    with local.open("rb") as f:
        ftp.storbinary(f"STOR {remote}", f)
    try:
        ftp.sendcmd(f"SITE CHMOD 755 {remote}")
    except Exception:
        pass
    print(f"[+] Uploaded {remote}")


def main() -> None:
    ap = argparse.ArgumentParser(description="Install GTA6WebTrainer.elf & PPSA38599 ShadowMount app via FTP")
    ap.add_argument("--ip", default="192.168.1.100", help="PS5 IP address")
    ap.add_argument("--ftp-port", type=int, default=1337, help="PS5 FTP port (default 1337)")
    ap.add_argument("--app-root", default="/data/homebrew/PPSA38599", help="ShadowMount folder on PS5")
    args = ap.parse_args()

    root = Path(__file__).resolve().parent
    ftp = ftplib.FTP()
    ftp.connect(args.ip, args.ftp_port, timeout=10)
    ftp.login()

    ensure_dir(ftp, "/data/etaHEN/payloads")
    upload(ftp, root / "elf_trainer" / "GTA6WebTrainer.elf", "/data/etaHEN/payloads/GTA6WebTrainer.elf")

    ensure_dir(ftp, f"{args.app_root}/sce_sys")
    upload(ftp, root / "PPSA38599" / "eboot.bin", f"{args.app_root}/eboot.bin")
    upload(ftp, root / "PPSA38599" / "sce_sys" / "icon0.png", f"{args.app_root}/sce_sys/icon0.png")
    upload(ftp, root / "PPSA38599" / "sce_sys" / "param.json", f"{args.app_root}/sce_sys/param.json")

    ftp.quit()
    print("[+] Done! Enable GTA6WebTrainer.elf in etaHEN Payloads and mount PPSA38599 with ShadowMount.")


if __name__ == "__main__":
    main()
