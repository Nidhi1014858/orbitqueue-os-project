#!/usr/bin/env bash
# OWNER: Rimjhim - replace this stub with the real implementation. Do not edit if you are not the owner.
set -euo pipefail

echo "Building OrbitQueue..."
if ! make; then
    echo "ERROR: make failed!" >&2
    exit 1
fi
echo "Build succeeded."
