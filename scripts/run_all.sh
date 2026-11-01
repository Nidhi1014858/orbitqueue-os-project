#!/usr/bin/env bash
# OWNER: Rimjhim - replace this stub with the real implementation. Do not edit if you are not the owner.
set -u

echo "=========================================="
echo "    OrbitQueue: Full Simulation Pipeline   "
echo "=========================================="

LOG_DIR="outputs/logs"
mkdir -p outputs "$LOG_DIR"

PASSED=0
SKIPPED=0
FAILED=0
STEP_COUNT=0

run_step() {
    local desc="$1"
    shift
    STEP_COUNT=$((STEP_COUNT + 1))
    local log_slug
    log_slug=$(echo "$desc" | tr -cs 'a-zA-Z0-9_' '_' | tr '[:upper:]' '[:lower:]' | sed 's/^_//;s/_$//')
    local log_file="${LOG_DIR}/step_${STEP_COUNT}_${log_slug}.log"

    echo -n "[RUN] $desc ... "
    set +e
    "$@" > "$log_file" 2>&1
    local code=$?
    set -u

    if [ "$code" -eq 0 ]; then
        echo "PASS"
        PASSED=$((PASSED + 1))
    elif [ "$code" -eq 77 ]; then
        echo "SKIPPED (stub / not implemented)"
        SKIPPED=$((SKIPPED + 1))
    else
        echo "FAIL (exit code $code)"
        FAILED=$((FAILED + 1))
        echo "       Log saved to: $log_file"
        echo "       --- Captured Output (stderr/stdout) ---"
        if [ -s "$log_file" ]; then
            sed 's/^/       | /' "$log_file"
        else
            echo "       | (No output captured; command exited with code $code)"
        fi
        echo "       ---------------------------------------"
    fi
}

# 1. Environment check
echo "--- Step 1: Environment Check ---"
if [ -f scripts/env_check.sh ]; then
    bash scripts/env_check.sh
fi

# 2. Build
echo ""
echo "--- Step 2: Build Binaries ---"
run_step "make build" make

# 3. CSV Check on data files
echo ""
echo "--- Step 3: Validate CSV Data ---"
if [ -f build/csv_check ]; then
    run_step "csv_check data/satellite_data.csv" build/csv_check data/satellite_data.csv
fi

# 4. ML Model Training
echo ""
echo "--- Step 4: Machine Learning Router ---"
if [ -f ml/train_router.py ]; then
    run_step "train_router.py" python3 ml/train_router.py --train data/training_scenarios.csv --packets data/satellite_data.csv --out outputs/decisions.csv --seed 42
fi

# Fallback check for decisions.csv
if [ ! -f outputs/decisions.csv ]; then
    echo "WARNING: outputs/decisions.csv is missing!"
    echo "WARNING: Using data/decisions_sample.csv as fallback for downstream tasks."
    mkdir -p outputs
    cp data/decisions_sample.csv outputs/decisions.csv || true
fi

# 5. Router App
echo ""
echo "--- Step 5: Router Pipeline ---"
if [ -f build/router_app ]; then
    run_step "router_app execution" build/router_app --csv data/satellite_data.csv --decisions outputs/decisions.csv --workers 2 --out outputs/routing_log.csv --trace outputs/page_trace.txt --summary outputs/routing_summary.txt
fi

# Trace file for page replacement
TRACE_FILE="outputs/page_trace.txt"
if [ ! -f "$TRACE_FILE" ]; then
    echo "Note: outputs/page_trace.txt missing, using data/page_trace_sample.txt for page replacement."
    TRACE_FILE="data/page_trace_sample.txt"
fi

# 6. Memory Management (Exp 8)
echo ""
echo "--- Step 6: Memory Management (Exp 8) ---"
if [ -f build/memory_management ]; then
    run_step "memory_management address tests" build/memory_management --test data/addr_tests.txt
fi

# 7. Banker's Algorithm (Exp 9)
echo ""
echo "--- Step 7: Banker's Algorithm (Exp 9) ---"
if [ -f build/bankers ]; then
    run_step "bankers scenario check" build/bankers --scenario all
fi

# 8. Page Replacement (Exp 10)
echo ""
echo "--- Step 8: Page Replacement (Exp 10) ---"
if [ -f build/page_replacement ]; then
    run_step "page_replacement evaluation" build/page_replacement --ref-file "$TRACE_FILE" --frames 1,3,4,5
fi

# Summary
echo ""
echo "=========================================="
echo "Pipeline Summary:"
echo "  PASS:    $PASSED"
echo "  SKIPPED: $SKIPPED"
echo "  FAIL:    $FAILED"
echo "=========================================="

if [ "$FAILED" -gt 0 ]; then
    echo "Pipeline execution FAILED."
    exit 1
else
    echo "Pipeline execution finished successfully (all executed steps passed or skipped cleanly)."
    exit 0
fi
