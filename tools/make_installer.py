#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Generate a standalone single-file HTML WebMIDI installer with the .fwsc
inlined as a data: URL so it works directly when opened from disk (file://)
in Chrome or Edge."""
import base64
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "base"
sys.path.insert(0, str(BASE / "tools"))
from build import read_conf  # noqa: E402
from make_site import fill, product_of, strip_module  # noqa: E402


def make_installer(app_dir: Path, pkg_path: Path, version: str, out_paths: list[Path]) -> None:
    conf = read_conf(app_dir / "fm1.toml")
    raw = pkg_path.read_bytes()
    product = product_of(raw)
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg_path}: no update loader in package")

    html = (BASE / "web" / "installer.html").read_text(encoding="utf-8")
    lib = (
        strip_module((BASE / "web" / "fm1pkg.js").read_text(encoding="utf-8"))
        + "\n"
        + strip_module((BASE / "web" / "fm1ota.js").read_text(encoding="utf-8")).replace(
            "/fm-?1|felucca|ota|composite|sinco|usb-midi/i",
            "/fm-?1|felucca|ota|composite|sinco|usb-midi|entropy|helios/i",
        )
    )
    b64 = base64.b64encode(raw).decode("ascii")
    meta = json.dumps({
        "version": version,
        "product": product,
        "pkg": f"data:application/octet-stream;base64,{b64}",
    })

    repo = conf.get("repo", "https://github.com/charlesvestal/fm1-base")
    repo_short = re.sub(r"^https?://", "", repo) or conf.get("name", "entropy-helios")
    credits = conf.get("credits", "")
    values = {
        "NAME": conf.get("usb_product", conf.get("name", "ENTROPY HELIOS")),
        "VERSION": version,
        "PKG": pkg_path.name,
        "PRODUCT": product,
        "TAGLINE": conf.get("tagline", ""),
        "CREDITS": (" " + credits) if credits else "",
        "REPO": repo,
        "REPO_SHORT": repo_short,
        "LICENSE": conf.get("licence", "MPL-2.0"),
    }
    filled = fill(html, values, "installer.html")
    filled = filled.replace("/*LIB*/", lib).replace("/*META*/", meta)

    for dst in out_paths:
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(filled, encoding="utf-8", newline="\n")
        print(f"wrote {dst} ({len(filled)} B, product={product}, fwsc={len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        raise SystemExit("usage: make_installer.py APP_DIR PKG.fwsc VERSION [OUT.html ...]")
    app = Path(sys.argv[1]).resolve()
    pkg = Path(sys.argv[2]).resolve()
    ver = sys.argv[3]
    outs = [Path(p).resolve() for p in sys.argv[4:]] or [pkg.with_suffix(".html")]
    make_installer(app, pkg, ver, outs)
