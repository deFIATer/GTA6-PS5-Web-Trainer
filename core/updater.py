"""GitHub self-update check for GTA VI PS5 Web Trainer."""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

GITHUB_REPO = "deFIATer/GTA6-PS5-Web-Trainer"
GITHUB_BRANCH = "main"
LOCAL_VERSION = "v2.0.0"


def _parse_ver(tag: str) -> tuple[int, ...]:
    nums = re.findall(r"\d+", tag)
    return tuple(int(n) for n in nums) if nums else (0,)


def _prompt_update(prompt: str) -> bool:
    try:
        answer = input(prompt)
    except (EOFError, KeyboardInterrupt):
        print()
        return False
    return answer.strip().lower() in ("y", "yes")


def _restart_process() -> None:
    print("[+] Restarting Web Trainer...")
    os.execv(sys.executable, [sys.executable, *sys.argv])


def _run_git(repo_dir: Path, args: list[str], timeout: int = 10) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(repo_dir), *args],
        capture_output=True,
        text=True,
        timeout=timeout,
    )


def _check_git_update(repo_dir: Path) -> None:
    print("[*] Checking GitHub for updates...")
    try:
        fetch = _run_git(repo_dir, ["fetch", "origin", GITHUB_BRANCH, "--quiet"], timeout=8)
    except (subprocess.SubprocessError, OSError):
        print("[!] Could not reach GitHub to check for updates. Continuing...")
        return

    if fetch.returncode != 0:
        print("[!] Could not check GitHub for updates (offline or remote error). Continuing...")
        return

    count_res = _run_git(repo_dir, ["rev-list", "--count", f"HEAD..origin/{GITHUB_BRANCH}"])
    if count_res.returncode != 0:
        return

    try:
        behind = int(count_res.stdout.strip() or "0")
    except ValueError:
        behind = 0

    if behind <= 0:
        print(f"[+] You are on the latest version (GitHub {GITHUB_BRANCH}).")
        return

    latest_res = _run_git(repo_dir, ["log", "-1", "--format=%h - %s", f"origin/{GITHUB_BRANCH}"])
    latest_info = latest_res.stdout.strip() if latest_res.returncode == 0 else "new commits available"

    print(f"[!] Update available on GitHub ({behind} commit(s) behind origin/{GITHUB_BRANCH}).")
    print(f"    Latest: {latest_info}")
    if not _prompt_update("[?] Do you want to update to the latest version now? [y/N]: "):
        print("[*] Skipping update. Starting current version...")
        return

    print("[*] Pulling latest changes from GitHub...")
    try:
        pull = _run_git(repo_dir, ["pull", "--ff-only", "origin", GITHUB_BRANCH], timeout=30)
    except (subprocess.SubprocessError, OSError) as exc:
        print(f"[!] Update failed ({exc}). Continuing with current version...")
        return

    if pull.returncode != 0:
        err = (pull.stderr or pull.stdout).strip()
        print(f"[!] Update failed: {err}")
        print("[*] Continuing with current version...")
        return

    print("[+] Updated successfully!")
    _restart_process()


def _fetch_latest_github_tag() -> str | None:
    headers = {"User-Agent": "GTA6-PS5-Web-Trainer", "Accept": "application/vnd.github+json"}
    for url in (
        f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest",
        f"https://api.github.com/repos/{GITHUB_REPO}/tags?per_page=1",
    ):
        req = urllib.request.Request(url, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=6) as resp:
                data = json.loads(resp.read().decode("utf-8"))
        except (urllib.error.URLError, TimeoutError, ValueError):
            continue
        if isinstance(data, dict) and data.get("tag_name"):
            return str(data["tag_name"])
        if isinstance(data, list) and data and isinstance(data[0], dict) and data[0].get("name"):
            return str(data[0]["name"])
    return None


def _apply_zip_update(repo_dir: Path) -> bool:
    zip_url = f"https://github.com/{GITHUB_REPO}/archive/refs/heads/{GITHUB_BRANCH}.zip"
    req = urllib.request.Request(zip_url, headers={"User-Agent": "GTA6-PS5-Web-Trainer"})
    skip_names = {".venv", ".git", "__pycache__", "trainer.log", ".DS_Store"}

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        zip_path = tmp_path / "update.zip"
        with urllib.request.urlopen(req, timeout=30) as resp, zip_path.open("wb") as out:
            shutil.copyfileobj(resp, out)

        extract_dir = tmp_path / "extracted"
        with zipfile.ZipFile(zip_path, "r") as zf:
            zf.extractall(extract_dir)

        roots = [p for p in extract_dir.iterdir() if p.is_dir()]
        if not roots:
            return False
        src_root = roots[0]

        for item in src_root.iterdir():
            if item.name in skip_names:
                continue
            dest = repo_dir / item.name
            if item.is_dir():
                shutil.copytree(item, dest, dirs_exist_ok=True, ignore=shutil.ignore_patterns("__pycache__", ".DS_Store"))
            else:
                shutil.copy2(item, dest)
    return True


def _check_zip_update(repo_dir: Path) -> None:
    print("[*] Checking GitHub for updates...")
    latest_tag = _fetch_latest_github_tag()
    if not latest_tag:
        print("[!] Could not check GitHub for updates. Continuing...")
        return

    if _parse_ver(latest_tag) <= _parse_ver(LOCAL_VERSION):
        print(f"[+] You are on the latest version ({LOCAL_VERSION}).")
        return

    print(f"[!] Update available on GitHub: {latest_tag} (current: {LOCAL_VERSION}).")
    if not _prompt_update("[?] Do you want to update to the latest version now? [y/N]: "):
        print("[*] Skipping update. Starting current version...")
        return

    print("[*] Downloading latest version from GitHub...")
    try:
        ok = _apply_zip_update(repo_dir)
    except (urllib.error.URLError, zipfile.BadZipFile, OSError, TimeoutError) as exc:
        print(f"[!] Update failed ({exc}). Continuing with current version...")
        return

    if not ok:
        print("[!] Update archive was empty. Continuing with current version...")
        return

    print("[+] Updated successfully!")
    _restart_process()


def check_for_updates(repo_dir: Path) -> None:
    """Check GitHub for updates and prompt the user in English before starting."""
    if (repo_dir / ".git").exists() and shutil.which("git"):
        _check_git_update(repo_dir)
    else:
        _check_zip_update(repo_dir)
