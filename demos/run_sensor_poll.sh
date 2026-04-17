#!/bin/bash

DATA_DIR="."
CONFIG_FILE="$DATA_DIR/config_threshold.txt"
OUTPUT_FILE="$DATA_DIR/sensor_1_out.txt"

mkdir -p "$DATA_DIR"

if [ ! -f "$CONFIG_FILE" ]; then
    echo -n "50" > "$CONFIG_FILE"
fi

echo "[LEGACY APP] Demone sensore avviato. Lettura config da: $CONFIG_FILE"
echo "[LEGACY APP] Scrittura output su: $OUTPUT_FILE"

while true; do
    THRESHOLD=$(cat "$CONFIG_FILE" 2>/dev/null)
    
    SENSOR_VAL=$((RANDOM % 101))
    
    if [ "$SENSOR_VAL" -ge "$THRESHOLD" ]; then
        echo "[LEGACY APP] Rilevato picco ($SENSOR_VAL >= $THRESHOLD). Scrivo su file..."
        echo -n "$SENSOR_VAL" > "$OUTPUT_FILE"
    else
        echo "[LEGACY APP] Valore normale ($SENSOR_VAL < $THRESHOLD)."
    fi
    
    sleep 2
done