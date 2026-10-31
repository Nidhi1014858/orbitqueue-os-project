#!/usr/bin/env bash
# OWNER: Rimjhim - replace this stub with the real implementation. Do not edit if you are not the owner.
set -euo pipefail

mkdir -p outputs
OUT_FILE="outputs/env_check.txt"

{
    echo "=== uname -a ==="
    uname -a || true
    echo ""
    echo "=== /etc/os-release ==="
    cat /etc/os-release || true
    echo ""
    echo "=== g++ --version ==="
    g++ --version || true
    echo ""
    echo "=== make --version ==="
    make --version || true
    echo ""
    echo "=== python3 --version ==="
    python3 --version || true
    echo ""
    echo "=== nproc ==="
    nproc || true
    echo ""
    echo "=== free -h ==="
    free -h || true
    echo ""
    echo "=== df -h ==="
    df -h || true
    echo ""
    echo "=== uptime ==="
    uptime || true
} > "$OUT_FILE"

cat "$OUT_FILE"
