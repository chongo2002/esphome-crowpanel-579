#!/usr/bin/env bash
set -euo pipefail

CONFIG_PATH=/data/options.json

DASHBOARD_URL=$(python3 -c "import json;print(json.load(open('$CONFIG_PATH'))['dashboard_url'])")
WIDTH=$(python3 -c "import json;print(json.load(open('$CONFIG_PATH'))['width'])")
HEIGHT=$(python3 -c "import json;print(json.load(open('$CONFIG_PATH'))['height'])")
INTERVAL=$(python3 -c "import json;print(json.load(open('$CONFIG_PATH'))['interval_seconds'])")
OUTPUT_PATH=$(python3 -c "import json;print(json.load(open('$CONFIG_PATH'))['output_path'])")

echo "[dashboard_render] starting: url=${DASHBOARD_URL} size=${WIDTH}x${HEIGHT} interval=${INTERVAL}s output=${OUTPUT_PATH}"

while true; do
  python3 /app/render.py \
    --url "$DASHBOARD_URL" \
    --width "$WIDTH" \
    --height "$HEIGHT" \
    --output "$OUTPUT_PATH" \
    && echo "[dashboard_render] wrote ${OUTPUT_PATH}" \
    || echo "[dashboard_render] render failed, will retry next cycle"
  sleep "$INTERVAL"
done
