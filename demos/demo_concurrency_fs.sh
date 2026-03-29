#!/bin/bash
TARGET_DIR="/tmp/test_concurrency/"

mkdir -p "$TARGET_DIR"

echo "--- VFS2DB CONCURRENCY TEST ---"
echo "Aggiornamento in parallelo di colonne diverse..."

update_db() {
    local val=10
    echo -n "Prodotto_$val" > "$TARGET_DIR/target_name.txt"
    echo -n "$val" > "$TARGET_DIR/target_price.txt"
}

for i in {1..50}; do
    update_db $i &
done

wait

echo "Stato finale nel FileSystem (Perfettamente inconsistente):"
echo -n "Nome: " && cat "$TARGET_DIR/target_name.txt"
echo ""
echo -n "Prezzo: " && cat "$TARGET_DIR/target_price.txt"
echo ""