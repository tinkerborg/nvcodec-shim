#!/bin/sh
set -eu
dir="${1:-/nvcodec-shim}"
cp /libnvcodec-shim.so "$dir/libnvcodec-shim.so"
chmod 0444 "$dir/libnvcodec-shim.so"
echo "installed $dir/libnvcodec-shim.so"
