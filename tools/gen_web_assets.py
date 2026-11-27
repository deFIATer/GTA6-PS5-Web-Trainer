#!/usr/bin/env python3
"""Regenerate elf_trainer/web_assets.h from the web/ sources.

Keeps MANIFEST_JSON / ICON_SVG / VEHICLES_JSON from the existing header and re-embeds
INDEX_HTML, STYLES_CSS and APP_JS from disk.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HDR = ROOT / "elf_trainer" / "web_assets.h"
WEB = ROOT / "web"


def extract(s, sym):
    m = re.search(r"static const unsigned char " + sym + r"\[\] = \{(.*?)\};", s, re.S)
    if not m:
        raise SystemExit("symbol not found: " + sym)
    vals = re.findall(r"0x([0-9a-fA-F]{1,2})", m.group(1))
    return bytes(int(v, 16) for v in vals)


def main():
    orig = HDR.read_text(encoding="utf-8", errors="ignore")
    assets = {
        "ASSET_INDEX_HTML": (WEB / "index.html").read_bytes(),
        "ASSET_STYLES_CSS": (WEB / "styles.css").read_bytes(),
        "ASSET_APP_JS": (WEB / "app.js").read_bytes(),
        "ASSET_MANIFEST_JSON": extract(orig, "ASSET_MANIFEST_JSON"),
        "ASSET_ICON_SVG": extract(orig, "ASSET_ICON_SVG"),
        "ASSET_VEHICLES_JSON": extract(orig, "ASSET_VEHICLES_JSON"),
    }
    lines = ["#pragma once", "#include <stddef.h>", ""]
    for name, data in assets.items():
        lines.append("static const unsigned char %s[] = {" % name)
        chunk = []
        for b in data:
            chunk.append("0x%02x," % b)
            if len(chunk) == 16:
                lines.append("  " + " ".join(chunk))
                chunk = []
        if chunk:
            lines.append("  " + " ".join(chunk))
        lines.append("};")
        lines.append("static const size_t %s_len = %d;" % (name, len(data)))
        lines.append("")
    HDR.write_text("\n".join(lines), encoding="utf-8")
    print("wrote", HDR, "app.js", len(assets["ASSET_APP_JS"]), "index", len(assets["ASSET_INDEX_HTML"]))


if __name__ == "__main__":
    main()
