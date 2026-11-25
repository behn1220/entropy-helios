#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Generate a standalone single-file HTML WebMIDI installer using the custom
Entropy Helios Hardware Loader chassis (tools/installer_template.html) powered
by upstream fm1-base's untouched fm1ports.js, fm1pkg.js, fm1ota.js & OTA script."""
import base64
import json
import re
import sys
from pathlib import Path


def make_installer(base_dir: Path, app_dir: Path, pkg_path: Path, version: str, out_paths: list[Path]) -> None:
    sys.path.insert(0, str(base_dir / "tools"))
    from build import read_conf  # noqa: E402
    from make_site import product_of, strip_module  # noqa: E402

    conf = read_conf(app_dir / "fm1.toml")
    raw = pkg_path.read_bytes()
    product = product_of(raw)
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg_path}: no update loader in package")

    upstream_html = (base_dir / "web" / "installer.html").read_text(encoding="utf-8")
    m = re.search(r"<script type=\"module\">[\s\S]*?</script>", upstream_html)
    if not m:
        raise SystemExit("upstream web/installer.html missing <script type=\"module\"> block")
    upstream_script = m.group(0)

    tpl_path = Path(__file__).resolve().parent / "installer_template.html"
    if tpl_path.exists():
        html = tpl_path.read_text(encoding="utf-8").replace("/*UPSTREAM_SCRIPT*/", upstream_script)
    else:
        html = upstream_html

    lib = "\n".join(
        strip_module((base_dir / "web" / f).read_text(encoding="utf-8"))
        for f in ("fm1ports.js", "fm1pkg.js", "fm1ota.js")
    )
    b64 = base64.b64encode(raw).decode("ascii")
    meta = json.dumps({
        "version": version,
        "product": product,
        "pkg": f"data:application/octet-stream;base64,{b64}",
        "backup": bool(conf.get("backup")),
        "res": None,
        "boot": None,
    })

    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"installer template must contain {mark} once")

    repo = conf.get("repo", "https://github.com/behn1220/entropy-helios")
    credits = conf.get("credits", "")
    name = conf.get("title", conf.get("name", "Entropy Helios"))
    for key, val in (
        ("RES", conf.get("resources_what", f"{name}'s data")),
        ("RES_JA", conf.get("resources_what_ja", f"{name} のデータ")),
        ("NAME", name),
        ("VERSION", version),
        ("PRODUCT", product),
        ("REPO", repo),
        ("REPO_SHORT", re.sub(r"^https?://", "", repo)),
        ("LICENSE", conf.get("licence", "MPL-2.0")),
        ("CREDITS", (" " + credits) if credits else ""),
    ):
        html = html.replace("{{" + key + "}}", val)

    filled = html.replace("/*LIB*/", lib).replace("/*META*/", meta).replace("/*APP*/", "null")
    for dst in out_paths:
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(filled, encoding="utf-8", newline="\n")
        print(f"wrote {dst} ({len(filled)} B, product={product}, fwsc={len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) < 5:
        raise SystemExit("usage: make_installer.py BASE_DIR APP_DIR PKG.fwsc VERSION [OUT.html ...]")
    base = Path(sys.argv[1]).resolve()
    app = Path(sys.argv[2]).resolve()
    pkg = Path(sys.argv[3]).resolve()
    ver = sys.argv[4]
    outs = [Path(p).resolve() for p in sys.argv[5:]] or [pkg.with_suffix(".html")]
    make_installer(base, app, pkg, ver, outs)
