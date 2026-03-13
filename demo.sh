#!/bin/bash

set -x

DRIVER_BIN="/home/domelive/progetto_sistemi_virtuali/vfs2db/build/src/vfs2db"
MNT_POINT="/tmp/vfs2db_mnt"
DB_FILE="/tmp/demo.db"

PICTURE="/home/domelive/progetto_sistemi_virtuali/vfs2db/picture.webp"

pause() {
    read -p "Press ENTER to continue..."
}

rm -f "$DB_FILE"
sqlite3 "$DB_FILE" "VACUUM;"

mkdir -p "$MNT_POINT"

$DRIVER_BIN -o db="$DB_FILE" -o cache_enabled=0 "$MNT_POINT"

echo "vfs2db driver is running on $MNT_POINT"
pause

mkdir "$MNT_POINT/users" 
mkdir "$MNT_POINT/projects"

echo "Created tables users and projects"
pause

touch "$MNT_POINT/users/.schema/name.TEXT.ATTR.vfs2db"
touch "$MNT_POINT/users/.schema/profile_picture.BLOB.ATTR.vfs2db"

touch "$MNT_POINT/projects/.schema/user_id.users(rowid).FK.vfs2db"

echo "Created attributes for tables users and projects"
pause

mkdir "$MNT_POINT/users/48"
mkdir "$MNT_POINT/projects/1"
mkdir "$MNT_POINT/projects/2"

echo "Created records for tables users and projects"
pause

echo -n "Joe" > "$MNT_POINT/users/48/name.vfs2db"
cp "$PICTURE" "$MNT_POINT/users/48/profile_picture.vfs2db"

echo "User 48 has been initialized"
pause

echo 48 > "$MNT_POINT/projects/1/user_id.vfs2db"

echo "Project 1 has been initialized"
echo "Step finished"
pause

umount "$MNT_POINT"
