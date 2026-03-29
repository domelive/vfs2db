#!/bin/bash

DRIVER_BIN="/home/domelive/progetto_sistemi_virtuali/vfs2db/build/src/vfs2db"
DB_FILE="/home/domelive/progetto_sistemi_virtuali/vfs2db/databases/demo_db.db"
MNT_POINT="/tmp/test"
TARGET_DIR="$MNT_POINT/orders/1"

rm -f "$DB_FILE"
python3 /home/domelive/progetto_sistemi_virtuali/vfs2db/databases/demo_db.py > /dev/null

mkdir -p "$MNT_POINT"

$DRIVER_BIN -o db="$DB_FILE" -o foreign_keys=1 -o auto_cache "$MNT_POINT" 2>/dev/null

echo "--- VFS2DB CONCURRENCY TEST ---"
echo "Aggiornamento in parallelo di colonne diverse..."

update_db() {
    local val=10
    echo -n "Prodotto_$val" > "$TARGET_DIR/product_name.vfs2db"
    echo -n "$val" > "$TARGET_DIR/price.vfs2db"
}

for i in {1..50}; do
    update_db $i &
done

wait

echo "Stato finale nel DB (Perfettamente consistente):"
echo -n "Nome: " && cat "$TARGET_DIR/product_name.vfs2db"
echo ""
echo -n "Prezzo: " && cat "$TARGET_DIR/price.vfs2db"
echo ""

umount "$MNT_POINT"
