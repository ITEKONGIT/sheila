#!/usr/bin/env sh
set -eu

PORT="${PORT:-18877}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WEB_DIR="$ROOT/web"
SRC_DIR="$ROOT/src"
INC_DIR="$ROOT/include"

if ! command -v inotifywait >/dev/null 2>&1; then
    echo "Install inotify-tools: sudo apt install inotify-tools"
    exit 1
fi

echo "Building initial..."
cmake --build build/linux 2>&1 | tail -1

echo "Starting server on :${PORT}..."
"$ROOT/build/linux/ssheila" --port "$PORT" &
SERVER_PID=$!
trap "kill $SERVER_PID 2>/dev/null; exit" INT TERM

echo "Watching web/ src/ include/ for changes..."
while inotifywait -r -e modify,create,delete "$WEB_DIR" "$SRC_DIR" "$INC_DIR" 2>/dev/null; do
    echo "Changes detected, rebuilding..."
    cmake --build build/linux 2>&1 | tail -1
    kill $SERVER_PID 2>/dev/null || true
    sleep 0.5
    "$ROOT/build/linux/ssheila" --port "$PORT" &
    SERVER_PID=$!
    echo "Server restarted. Refresh browser."
done
