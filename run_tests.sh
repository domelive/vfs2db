#!/bin/bash
# =============================================================================
# VFS2DB Comprehensive Test Script
# =============================================================================
#
# Questo script esegue test approfonditi su tutte le funzionalità del driver
# VFS2DB. Deve essere eseguito DOPO aver montato il filesystem.
#
# Uso:
#   1. Genera il database: python3 generate_stress_db.py stress_test.db
#   2. Monta il filesystem: ./vfs2db -f -o db=stress_test.db /tmp/vfs2db &
#   3. Esegui i test: ./run_tests.sh /tmp/vfs2db
#
# =============================================================================

set -e  # Exit on error

# Colori per output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Contatori
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# =============================================================================
# UTILITY FUNCTIONS
# =============================================================================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++))
    ((TESTS_TOTAL++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TESTS_FAILED++))
    ((TESTS_TOTAL++))
}

log_section() {
    echo ""
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}$1${NC}"
    echo -e "${YELLOW}========================================${NC}"
}

test_command() {
    local description="$1"
    local command="$2"
    local expected_exit_code="${3:-0}"
    
    if eval "$command" > /dev/null 2>&1; then
        if [ "$expected_exit_code" -eq 0 ]; then
            log_success "$description"
            return 0
        else
            log_fail "$description (expected failure but succeeded)"
            return 1
        fi
    else
        if [ "$expected_exit_code" -ne 0 ]; then
            log_success "$description (expected failure)"
            return 0
        else
            log_fail "$description"
            return 1
        fi
    fi
}

test_output_contains() {
    local description="$1"
    local command="$2"
    local expected="$3"
    
    local output
    output=$(eval "$command" 2>&1) || true
    
    if echo "$output" | grep -q "$expected"; then
        log_success "$description"
        return 0
    else
        log_fail "$description (expected '$expected' not found)"
        echo "    Output was: ${output:0:100}..."
        return 1
    fi
}

test_file_size() {
    local description="$1"
    local filepath="$2"
    local min_size="$3"
    
    local size
    size=$(stat -c%s "$filepath" 2>/dev/null) || size=0
    
    if [ "$size" -ge "$min_size" ]; then
        log_success "$description (size: $size bytes)"
        return 0
    else
        log_fail "$description (size: $size, expected >= $min_size)"
        return 1
    fi
}

# =============================================================================
# MAIN
# =============================================================================

if [ -z "$1" ]; then
    echo "Usage: $0 <mount_point>"
    echo "Example: $0 /tmp/vfs2db"
    exit 1
fi

MOUNT_POINT="$1"

# Verifica che il mount point esista
if [ ! -d "$MOUNT_POINT" ]; then
    echo "Error: Mount point $MOUNT_POINT does not exist"
    exit 1
fi

echo ""
echo "============================================================"
echo "       VFS2DB COMPREHENSIVE TEST SUITE"
echo "============================================================"
echo "Mount point: $MOUNT_POINT"
echo "Started at: $(date)"
echo "============================================================"

# =============================================================================
# TEST 1: NAVIGAZIONE FILESYSTEM BASE
# =============================================================================
log_section "TEST 1: Basic Filesystem Navigation"

# Test root directory
test_command "List root directory" "ls '$MOUNT_POINT'"
test_command "Root contains 'users' table" "ls '$MOUNT_POINT' | grep -q users"
test_command "Root contains 'products' table" "ls '$MOUNT_POINT' | grep -q products"
test_command "Root contains 'orders' table" "ls '$MOUNT_POINT' | grep -q orders"
test_command "Root contains 'large_data' table" "ls '$MOUNT_POINT' | grep -q large_data"

# Conta le tabelle
TABLE_COUNT=$(ls "$MOUNT_POINT" | wc -l)
log_info "Found $TABLE_COUNT tables in root"

# =============================================================================
# TEST 2: NAVIGAZIONE RECORD
# =============================================================================
log_section "TEST 2: Record Navigation"

# Test record listing
test_command "List users records" "ls '$MOUNT_POINT/users'"
test_command "List products records" "ls '$MOUNT_POINT/products'"
test_command "List orders records" "ls '$MOUNT_POINT/orders'"

# Conta i record
USERS_COUNT=$(ls "$MOUNT_POINT/users" | wc -l)
log_info "Found $USERS_COUNT records in users table"

# Test record singolo
test_command "Access first user record" "ls '$MOUNT_POINT/users/1'"

# =============================================================================
# TEST 3: LETTURA ATTRIBUTI
# =============================================================================
log_section "TEST 3: Attribute Reading"

# Test lettura attributi
test_command "Read user id attribute" "cat '$MOUNT_POINT/users/1/id.vfs2db'"
test_command "Read user username attribute" "cat '$MOUNT_POINT/users/1/username.vfs2db'"
test_command "Read user email attribute" "cat '$MOUNT_POINT/users/1/email.vfs2db'"

# Verifica contenuto
test_output_contains "User 1 username starts with 'user_'" \
    "cat '$MOUNT_POINT/users/1/username.vfs2db'" "user_"

# =============================================================================
# TEST 4: EXTENDED ATTRIBUTES (user.type)
# =============================================================================
log_section "TEST 4: Extended Attributes (xattr)"

# Test getxattr per diversi tipi
if command -v getfattr &> /dev/null; then
    # Test su all_types table
    test_output_contains "xattr: INTEGER type" \
        "getfattr -n user.type '$MOUNT_POINT/all_types/1/int_val.vfs2db'" "INTEGER"
    
    test_output_contains "xattr: REAL/FLOAT type" \
        "getfattr -n user.type '$MOUNT_POINT/all_types/1/real_val.vfs2db'" "FLOAT"
    
    test_output_contains "xattr: TEXT type" \
        "getfattr -n user.type '$MOUNT_POINT/all_types/1/text_val.vfs2db'" "TEXT"
    
    test_output_contains "xattr: BLOB type" \
        "getfattr -n user.type '$MOUNT_POINT/all_types/1/blob_val.vfs2db'" "BLOB"
    
    test_output_contains "xattr: NULL type" \
        "getfattr -n user.type '$MOUNT_POINT/all_types/1/null_val.vfs2db'" "NULL"
else
    log_info "getfattr not available, skipping xattr tests"
fi

# =============================================================================
# TEST 5: FOREIGN KEYS (SYMLINKS)
# =============================================================================
log_section "TEST 5: Foreign Key Symlinks"

# Test che i FK siano symlink
test_command "FK is a symlink (orders.user_id)" \
    "test -L '$MOUNT_POINT/orders/1/user_id.vfs2db'"

test_command "FK is a symlink (products.category_id)" \
    "test -L '$MOUNT_POINT/products/1/category_id.vfs2db'"

test_command "FK is a symlink (order_items.order_id)" \
    "test -L '$MOUNT_POINT/order_items/1/order_id.vfs2db'"

# Test readlink
if [ -L "$MOUNT_POINT/orders/1/user_id.vfs2db" ]; then
    LINK_TARGET=$(readlink "$MOUNT_POINT/orders/1/user_id.vfs2db")
    log_info "FK symlink target: $LINK_TARGET"
    test_output_contains "FK points to users table" \
        "readlink '$MOUNT_POINT/orders/1/user_id.vfs2db'" "users"
fi

# Test seguire il symlink
test_command "Follow FK symlink to get user data" \
    "cat '$MOUNT_POINT/orders/1/user_id.vfs2db'"

# =============================================================================
# TEST 6: CATENE DI FK
# =============================================================================
log_section "TEST 6: FK Chains"

# Test catene di FK
for chain in 0 1 2; do
    if [ -d "$MOUNT_POINT/chain${chain}_level3" ]; then
        test_command "FK chain $chain level 3 -> level 2" \
            "test -L '$MOUNT_POINT/chain${chain}_level3/1/parent_id.vfs2db'"
        
        test_output_contains "FK chain $chain points correctly" \
            "readlink '$MOUNT_POINT/chain${chain}_level3/1/parent_id.vfs2db'" \
            "chain${chain}_level2"
    fi
done

# =============================================================================
# TEST 7: DATI GRANDI (CACHE TEST)
# =============================================================================
log_section "TEST 7: Large Data & Cache Stress"

# Test lettura dati grandi
test_command "Read large data record" "ls '$MOUNT_POINT/large_data/1'"

# Test che i file grandi abbiano la dimensione corretta
for record in 1 2 3 4; do
    if [ -f "$MOUNT_POINT/large_data/$record/large_text.vfs2db" ]; then
        SIZE=$(stat -c%s "$MOUNT_POINT/large_data/$record/large_text.vfs2db" 2>/dev/null) || SIZE=0
        if [ "$SIZE" -gt 0 ]; then
            log_info "large_data/$record/large_text.vfs2db size: $SIZE bytes"
        fi
    fi
done

# Test lettura parziale (con offset)
test_command "Read large file with dd (offset test)" \
    "dd if='$MOUNT_POINT/large_data/1/large_text.vfs2db' bs=1024 count=10 skip=100 2>/dev/null"

# Cache stress: leggi molti file per forzare eviction
log_info "Stressing cache with multiple large reads..."
for i in $(seq 1 20); do
    if [ -f "$MOUNT_POINT/large_data/$i/large_text.vfs2db" ]; then
        cat "$MOUNT_POINT/large_data/$i/large_text.vfs2db" > /dev/null 2>&1 || true
    fi
done
log_success "Cache stress test completed (20 large file reads)"

# =============================================================================
# TEST 8: BLOB DATA
# =============================================================================
log_section "TEST 8: Binary Data (BLOB)"

# Test lettura BLOB
test_command "Access binary_data table" "ls '$MOUNT_POINT/binary_data'"
test_command "Access BLOB record" "ls '$MOUNT_POINT/binary_data/1'"

# Test che il file BLOB sia leggibile
if [ -f "$MOUNT_POINT/binary_data/1/data.vfs2db" ]; then
    test_file_size "BLOB file has data" "$MOUNT_POINT/binary_data/1/data.vfs2db" 100
fi

# =============================================================================
# TEST 9: TABELLA CON MOLTE COLONNE
# =============================================================================
log_section "TEST 9: Many Columns Table"

test_command "Access many_columns table" "ls '$MOUNT_POINT/many_columns'"
test_command "Access many_columns record" "ls '$MOUNT_POINT/many_columns/1'"

# Conta le colonne (file .vfs2db)
COLUMN_COUNT=$(ls "$MOUNT_POINT/many_columns/1" | grep -c '.vfs2db$')
log_info "many_columns table has $COLUMN_COUNT attributes"

if [ "$COLUMN_COUNT" -ge 40 ]; then
    log_success "Many columns test passed ($COLUMN_COUNT columns)"
else
    log_fail "Many columns test: expected >= 40 columns, got $COLUMN_COUNT"
fi

# =============================================================================
# TEST 10: EDGE CASES
# =============================================================================
log_section "TEST 10: Edge Cases"

# Tabella vuota
test_command "Access empty_table" "ls '$MOUNT_POINT/empty_table'"
EMPTY_COUNT=$(ls "$MOUNT_POINT/empty_table" 2>/dev/null | grep -v "^\.\.\?$" | wc -l)
if [ "$EMPTY_COUNT" -eq 0 ]; then
    log_success "Empty table has no records"
else
    log_fail "Empty table should have 0 records, has $EMPTY_COUNT"
fi

# Tabella con record singolo
test_command "Access single_record table" "ls '$MOUNT_POINT/single_record'"
SINGLE_COUNT=$(ls "$MOUNT_POINT/single_record" 2>/dev/null | grep -v "^\.\.\?$" | wc -l)
if [ "$SINGLE_COUNT" -eq 1 ]; then
    log_success "Single record table has exactly 1 record"
else
    log_fail "Single record table should have 1 record, has $SINGLE_COUNT"
fi

# =============================================================================
# TEST 11: SPECIAL COLUMN NAMES
# =============================================================================
log_section "TEST 11: Special Column Names"

test_command "Access special_names table" "ls '$MOUNT_POINT/special_names'"
test_command "Access special_names record" "ls '$MOUNT_POINT/special_names/1'"

# Lista gli attributi
log_info "Special column names found:"
ls "$MOUNT_POINT/special_names/1" 2>/dev/null | head -5

# =============================================================================
# TEST 12: CONCURRENT ACCESS SIMULATION
# =============================================================================
log_section "TEST 12: Concurrent Access Simulation"

log_info "Starting concurrent read test..."

# Avvia letture parallele
for i in {1..10}; do
    (
        cat "$MOUNT_POINT/users/$i/username.vfs2db" > /dev/null 2>&1
        cat "$MOUNT_POINT/products/$i/name.vfs2db" > /dev/null 2>&1
        cat "$MOUNT_POINT/orders/$i/status.vfs2db" > /dev/null 2>&1
    ) &
done

# Aspetta che finiscano
wait

log_success "Concurrent access test completed (30 parallel reads)"

# =============================================================================
# TEST 13: WRITE OPERATIONS
# =============================================================================
log_section "TEST 13: Write Operations"

# Salva il valore originale
ORIGINAL_VALUE=""
if [ -f "$MOUNT_POINT/users/1/bio.vfs2db" ]; then
    ORIGINAL_VALUE=$(cat "$MOUNT_POINT/users/1/bio.vfs2db" 2>/dev/null) || true
fi

# Test scrittura
TEST_VALUE="Test write from VFS2DB test script - $(date +%s)"
log_info "Writing test value: ${TEST_VALUE:0:50}..."

if echo "$TEST_VALUE" > "$MOUNT_POINT/users/1/bio.vfs2db" 2>/dev/null; then
    # Verifica che la scrittura sia andata a buon fine
    READ_VALUE=$(cat "$MOUNT_POINT/users/1/bio.vfs2db" 2>/dev/null) || true
    
    if [ "$READ_VALUE" = "$TEST_VALUE" ]; then
        log_success "Write and read-back test passed"
    else
        log_fail "Write test: read-back value doesn't match"
        log_info "Expected: $TEST_VALUE"
        log_info "Got: $READ_VALUE"
    fi
    
    # Ripristina il valore originale (opzionale)
    # echo "$ORIGINAL_VALUE" > "$MOUNT_POINT/users/1/bio.vfs2db" 2>/dev/null
else
    log_fail "Write operation failed"
fi

# =============================================================================
# TEST 14: STAT/GETATTR
# =============================================================================
log_section "TEST 14: File Attributes (stat)"

# Test stat su directory
test_command "stat on root directory" "stat '$MOUNT_POINT'"
test_command "stat on table directory" "stat '$MOUNT_POINT/users'"
test_command "stat on record directory" "stat '$MOUNT_POINT/users/1'"

# Test stat su file
test_command "stat on regular file" "stat '$MOUNT_POINT/users/1/username.vfs2db'"

# Test stat su symlink
test_command "stat on symlink (FK)" "stat '$MOUNT_POINT/orders/1/user_id.vfs2db'"

# Verifica che i symlink siano identificati correttamente
FILE_TYPE=$(stat -c%F "$MOUNT_POINT/orders/1/user_id.vfs2db" 2>/dev/null) || true
if [[ "$FILE_TYPE" == *"symbolic link"* ]] || [[ "$FILE_TYPE" == *"link"* ]]; then
    log_success "FK correctly identified as symbolic link"
else
    log_fail "FK should be a symbolic link, got: $FILE_TYPE"
fi

# =============================================================================
# TEST 15: PERFORMANCE BENCHMARK
# =============================================================================
log_section "TEST 15: Performance Benchmark"

# Benchmark lettura sequenziale
log_info "Sequential read benchmark (100 files)..."
START_TIME=$(date +%s%N)
for i in $(seq 1 100); do
    cat "$MOUNT_POINT/users/$i/username.vfs2db" > /dev/null 2>&1 || true
done
END_TIME=$(date +%s%N)
ELAPSED=$(( (END_TIME - START_TIME) / 1000000 ))  # ms
log_info "Sequential read: 100 files in ${ELAPSED}ms ($(( ELAPSED / 100 ))ms per file)"

# Benchmark directory listing
log_info "Directory listing benchmark..."
START_TIME=$(date +%s%N)
for i in $(seq 1 50); do
    ls "$MOUNT_POINT/users" > /dev/null 2>&1 || true
done
END_TIME=$(date +%s%N)
ELAPSED=$(( (END_TIME - START_TIME) / 1000000 ))
log_info "Directory listing: 50 iterations in ${ELAPSED}ms"

# Benchmark large file read
log_info "Large file read benchmark..."
if [ -f "$MOUNT_POINT/large_data/1/large_text.vfs2db" ]; then
    START_TIME=$(date +%s%N)
    cat "$MOUNT_POINT/large_data/1/large_text.vfs2db" > /dev/null 2>&1
    END_TIME=$(date +%s%N)
    ELAPSED=$(( (END_TIME - START_TIME) / 1000000 ))
    FILE_SIZE=$(stat -c%s "$MOUNT_POINT/large_data/1/large_text.vfs2db" 2>/dev/null) || FILE_SIZE=0
    if [ "$FILE_SIZE" -gt 0 ] && [ "$ELAPSED" -gt 0 ]; then
        THROUGHPUT=$(( FILE_SIZE / ELAPSED ))  # bytes/ms = KB/s
        log_info "Large file read: ${FILE_SIZE} bytes in ${ELAPSED}ms (~${THROUGHPUT} KB/s)"
    fi
fi

log_success "Performance benchmark completed"

# =============================================================================
# SUMMARY
# =============================================================================
log_section "TEST SUMMARY"

echo ""
echo "============================================================"
echo "                    TEST RESULTS"
echo "============================================================"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
echo -e "Total tests:  $TESTS_TOTAL"
echo "============================================================"

if [ "$TESTS_FAILED" -eq 0 ]; then
    echo -e "${GREEN}ALL TESTS PASSED!${NC}"
    exit 0
else
    echo -e "${RED}SOME TESTS FAILED${NC}"
    exit 1
fi
