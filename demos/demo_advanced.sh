#!/bin/bash

DRIVER_BIN="/home/domelive/progetto_sistemi_virtuali/vfs2db/build/src/vfs2db"
DB_FILE="/home/domelive/progetto_sistemi_virtuali/vfs2db/databases/legacy.db"
MNT_POINT="/tmp/test"

LEGACY_PATH_THRESHOLD="./config_threshold.txt"
LEGACY_PATH_SENSOR="./sensor_1_out.txt"
LEGACY_PATH_SCRIPT="./run_sensor_poll.sh"

mkdir -p "$MNT_POINT"

rm -f "$DB_FILE"
sqlite3 "$DB_FILE" "VACUUM;"

$DRIVER_BIN -o db="$DB_FILE" -o foreign_keys=1 -o auto_cache "$MNT_POINT" 2>/dev/null

mkdir "$MNT_POINT/sensors_config"
mkdir "$MNT_POINT/sensors_output"

touch "$MNT_POINT/sensors_config/.schema/threshold.INTEGER.ATTR.col"
touch "$MNT_POINT/sensors_output/.schema/last_value.INTEGER.ATTR.col"

mkdir "$MNT_POINT/sensors_config/1"
mkdir "$MNT_POINT/sensors_output/1"

echo "50" > "$MNT_POINT/sensors_config/1/threshold.at"

ln -s "$MNT_POINT/sensors_config/1/threshold.at" "$LEGACY_PATH_THRESHOLD"
ln -s "$MNT_POINT/sensors_output/1/last_value.at" "$LEGACY_PATH_SENSOR"

$LEGACY_PATH_SCRIPT

rm "$LEGACY_PATH_THRESHOLD"
rm "$LEGACY_PATH_SENSOR"

umount "$MNT_POINT"
