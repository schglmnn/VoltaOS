#!/bin/bash
# Run the bundled AppImage with project-local Ubuntu/Python dependencies.
set -euo pipefail
DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/.runtime/sysroot/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PYTHONPATH="$DIR/.runtime/python${PYTHONPATH:+:$PYTHONPATH}"
# Without --window, VoltaOS uses the screen resolution in fullscreen mode.
exec "$DIR/AppImage/VoltaOS-1.0-x86_64.AppImage" --appimage-extract-and-run "$@"
