#!/usr/bin/env bash
# Fetch a stock Mozilla Firefox + geckodriver for the Firefox conformance engine,
# into <repo>/tmp/tools/ (the canon_firefox.js default location). Re-runnable.
# Override locations with $FIREFOX_BIN / $GECKODRIVER instead of running this.
set -euo pipefail
GECKO_VER="v0.37.0"
root="$(cd "$(dirname "$0")/../../../.." && pwd)"
tools="$root/tmp/tools"
mkdir -p "$tools"; cd "$tools"

if [ ! -x geckodriver ]; then
  echo "[geckodriver $GECKO_VER]"
  curl -fsSL -o geckodriver.tar.gz \
    "https://github.com/mozilla/geckodriver/releases/download/$GECKO_VER/geckodriver-$GECKO_VER-linux64.tar.gz"
  tar xzf geckodriver.tar.gz && rm -f geckodriver.tar.gz
fi

if [ ! -x firefox/firefox ]; then
  echo "[firefox latest]"
  curl -fsSL -o firefox.tar.xz "https://download.mozilla.org/?product=firefox-latest&os=linux64&lang=en-US"
  tar xf firefox.tar.xz && rm -f firefox.tar.xz
fi

echo "[runtime libs (best-effort; needs sudo)]"
sudo apt-get install -y libgtk-3-0 libdbus-glib-1-2 libxt6 libx11-xcb1 libxcb1 \
  libxtst6 libpci3 libgl1 fonts-liberation libasound2t64 >/dev/null 2>&1 || \
  echo "  (skipped — install Firefox's runtime libs manually if it fails to launch)"

./geckodriver --version | head -1
./firefox/firefox --version
echo "ready: $tools/geckodriver + $tools/firefox/firefox"
