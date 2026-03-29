#!/bin/bash
DRIVER_BIN="/home/domelive/progetto_sistemi_virtuali/vfs2db/build/src/vfs2db"
DB_FILE="/home/domelive/progetto_sistemi_virtuali/vfs2db/databases/demo_db.db"
MNT_POINT="/tmp/test"
SOGLIA=100

rm -f "$DB_FILE"
python3 /home/domelive/progetto_sistemi_virtuali/vfs2db/databases/demo_db.py > /dev/null

mkdir -p "$MNT_POINT"

$DRIVER_BIN -o db="$DB_FILE" -o foreign_keys=1 -o auto_cache "$MNT_POINT" 2>/dev/null

echo "--- Applicazione Sconto 10% per Ordini > €$SOGLIA (Utenti Gmail) ---"

for user_email_file in $(grep -l "@gmail.com" $MNT_POINT/users/*/email.vfs2db); do
    user_dir=$(dirname "$user_email_file")
    user_name=$(cat "$user_dir/name.vfs2db")

    for order_dir in $MNT_POINT/orders/*/; do
        if [ "$(cat $order_dir/user_name.vfs2db)" = "$(cat $user_dir/name.vfs2db)" ]; then
            price=$(cat "$order_dir/price.vfs2db")

            if [ "$(echo "$price > $SOGLIA" | bc)" -eq 1 ]; then
                new_price=$(echo "scale=2; $price * 0.9" | bc)
                echo -n "$new_price" > "$order_dir/price.vfs2db"

                product=$(cat "$order_dir/product_name.vfs2db")
                echo "[!] Sconto applicato a $user_name per '$product': €$price -> €$new_price"
            fi
        fi
    done
done

echo "--- Operazione completata ---"

umount "$MNT_POINT"
