#!/bin/bash
DRIVER_BIN="/home/domelive/progetto_sistemi_virtuali/vfs2db/build/src/vfs2db"
DB_FILE="/home/domelive/progetto_sistemi_virtuali/vfs2db/databases/demo_db.db"
MNT_POINT="/tmp/test"

mkdir -p "$MNT_POINT"

$DRIVER_BIN -o db="$DB_FILE" -o foreign_keys=1 -o auto_cache "$MNT_POINT" 2>/dev/null

echo "--- Ricerca Utenti (Cognome S*) con prodotti 'Pro' ---"

# Cerchiamo direttamente nei "file" dei prodotti in modo case-insensitive (-i)
for product_file in $(grep -li "Pro" $MNT_POINT/orders/*/product_name.vfs2db 2>/dev/null); do
    order_dir=$(dirname "$product_file")

    # Risolviamo il symlink della Foreign Key singola (user_name)
    # per atterrare direttamente sulla directory dell'utente!
    user_name_file=$(readlink -f "$order_dir/user_name.vfs2db")
    user_dir=$(dirname "$user_name_file")

    # Ora che siamo nella cartella dell'utente, leggiamo il cognome
    surname=$(cat "$user_dir/surname.vfs2db")

    # Operatore di pattern matching nativo di Bash
    if [[ "$surname" == S* ]]; then
        name=$(cat "$user_dir/name.vfs2db")
        email=$(cat "$user_dir/email.vfs2db")
        product=$(cat "$product_file")

        echo "Match perfetto: $name $surname ($email) ha acquistato -> $product"
    fi
done

umount "$MNT_POINT"
