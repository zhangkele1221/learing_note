#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"

python3 "$DIR/00_dense_attention.py"
python3 "$DIR/01_block_table.py"
python3 "$DIR/02_paged_kv_cache.py"
python3 "$DIR/03_paged_decode.py"
python3 "$DIR/04_prefix_cow.py"
python3 "$DIR/05_prefix_cache.py"
