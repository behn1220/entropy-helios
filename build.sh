#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Build ENTROPY HELIOS for the M-VAVE FM-1 using fm1-base (based on Styves Goupil's build.sh template).
#   ./build.sh [--release X.Y] [--host | --web]
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/fw-AC79_AIoT_SDK)
#   PYTHON           Python with Pillow and fontTools (default: .venv/bin/python or python3)
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
PY="${PYTHON:-.venv/bin/python}"
[ -x "$PY" ] || PY=python3

if [ ! -f base/tools/build.py ]; then
    echo "build.sh: fetching fm1-base into base/..."
    git clone --recursive --depth 1 https://github.com/charlesvestal/fm1-base.git base
fi

"$PY" -c 'import PIL' 2>/dev/null || {
    echo "build.sh: $PY has no Pillow (install with: pip install Pillow fonttools)"
    exit 1
}

V="$(tr -d ' \r\n' < VERSION 2>/dev/null || true)"
case " $* " in
*" --release "*) ;;
*) [ -n "$V" ] && set -- --release "$V" "$@" ;;
esac

mkdir -p app/build
exec "$PY" base/tools/build.py app "$@"
