#!/bin/bash
# Run the source build with Ubuntu's Qt runtime through XWayland.
set -euo pipefail
DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [ ! -x "$DIR/build/Release/VoltaOS" ]; then
    echo "Build missing: cmake --build build/Release" >&2
    exit 1
fi
export VOLTA_DIR="$DIR"
export PYTHONPATH="$DIR/.runtime/python${PYTHONPATH:+:$PYTHONPATH}"
export LD_LIBRARY_PATH="$DIR/.runtime/sysroot/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QML_IMPORT_PATH="$DIR/.runtime/sysroot/usr/lib/x86_64-linux-gnu/qt6/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
export QT_XCB_GL_INTEGRATION="${QT_XCB_GL_INTEGRATION:-xcb_glx}"
exec "$DIR/build/Release/VoltaOS" "$@"
