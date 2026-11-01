#!/usr/bin/env bash
# OWNER: Rimjhim - pipeline measurement and performance profiling script
set -euo pipefail

# -----------------------------------------------------------------------------
# Configuration and Output Paths
# -----------------------------------------------------------------------------
OUTPUT_DIR="outputs"
LOG_DIR="${OUTPUT_DIR}/logs"
REPORT_FILE="${OUTPUT_DIR}/measurements.md"
PIPELINE_LOG="${OUTPUT_DIR}/measure_pipeline.log"
TIME_LOG="${OUTPUT_DIR}/measure_time.log"

if ! mkdir -p "$OUTPUT_DIR" "$LOG_DIR"; then
    echo "ERROR: Failed to create output directories '$OUTPUT_DIR' or '$LOG_DIR'." >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# Pre-flight Checks
# -----------------------------------------------------------------------------
if [ ! -x "/usr/bin/time" ]; then
    echo "ERROR: /usr/bin/time is not installed or not executable." >&2
    echo "Please install it with: sudo apt update && sudo apt install -y time" >&2
    exit 1
fi

if [ ! -f "scripts/run_all.sh" ]; then
    echo "ERROR: scripts/run_all.sh not found. Ensure script is run from project root." >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# Pipeline Execution under /usr/bin/time -v
# -----------------------------------------------------------------------------
echo "=========================================================="
echo "  OrbitQueue Pipeline Performance & Resource Measurement"
echo "=========================================================="
echo "Target Command : bash scripts/run_all.sh"
echo "Timer Utility  : /usr/bin/time -v"
echo "Pipeline Output: $PIPELINE_LOG"
echo "Timing Log     : $TIME_LOG"
echo "Report Output  : $REPORT_FILE"
echo "----------------------------------------------------------"
echo "Executing measured pipeline..."

# Run under /usr/bin/time -v and preserve exit code
set +e
/usr/bin/time -v -o "$TIME_LOG" bash scripts/run_all.sh > "$PIPELINE_LOG" 2>&1
PIPELINE_EXIT_CODE=$?
set -e

echo "Pipeline execution finished with exit code: $PIPELINE_EXIT_CODE"

# -----------------------------------------------------------------------------
# Verify /usr/bin/time Output Integrity
# -----------------------------------------------------------------------------
# Distinguish: COMPLETE record, INCOMPLETE record, or FAILED invocation
TIME_TOOL_STATUS="COMPLETE"
TIME_TOOL_ERROR=""

if [ ! -f "$TIME_LOG" ] || [ ! -s "$TIME_LOG" ]; then
    TIME_TOOL_STATUS="FAILED"
    TIME_TOOL_ERROR="Timing log ($TIME_LOG) was not generated or is empty (/usr/bin/time failed to execute or write)."
elif ! grep -q "Elapsed (wall clock) time" "$TIME_LOG" || ! grep -q "Maximum resident set size" "$TIME_LOG"; then
    TIME_TOOL_STATUS="INCOMPLETE"
    TIME_TOOL_ERROR="Timing log ($TIME_LOG) is missing core metrics ('Elapsed (wall clock) time' or 'Maximum resident set size')."
fi

# -----------------------------------------------------------------------------
# Parse Metrics from /usr/bin/time -v output
# -----------------------------------------------------------------------------
get_time_val() {
    local key="$1"
    if [ "$TIME_TOOL_STATUS" = "FAILED" ]; then
        echo "Unavailable (timing tool invocation failed)"
        return 0
    fi
    if [ -f "$TIME_LOG" ]; then
        local line
        line=$(grep -E "${key}" "$TIME_LOG" | head -n 1 || true)
        if [ -n "$line" ]; then
            local val
            val=$(echo "$line" | sed -e 's/^[[:space:]]*//' -e "s/^${key}:[[:space:]]*//")
            if [ -n "$val" ]; then
                echo "$val"
                return 0
            fi
        fi
    fi
    echo "Unavailable (metric '${key}' not found in timing log)"
}

WALL_CLOCK=$(get_time_val "Elapsed \(wall clock\) time \(h:mm:ss or m:ss\)")
MAX_RSS=$(get_time_val "Maximum resident set size \(kbytes\)")
USER_TIME=$(get_time_val "User time \(seconds\)")
SYS_TIME=$(get_time_val "System time \(seconds\)")
CPU_PERCENT=$(get_time_val "Percent of CPU this job got")
MINOR_FAULTS=$(get_time_val "Minor \(reclaiming a frame\) page faults")
MAJOR_FAULTS=$(get_time_val "Major \(requiring I\/O\) page faults")
VOL_SWITCHES=$(get_time_val "Voluntary context switches")
INVOL_SWITCHES=$(get_time_val "Involuntary context switches")

# If essential metrics were missing while status was marked COMPLETE, flag as INCOMPLETE
if [ "$TIME_TOOL_STATUS" = "COMPLETE" ]; then
    if [[ "$WALL_CLOCK" =~ ^Unavailable ]] || [[ "$MAX_RSS" =~ ^Unavailable ]]; then
        TIME_TOOL_STATUS="INCOMPLETE"
        TIME_TOOL_ERROR="One or more essential metrics could not be parsed from timing log."
    fi
fi

MEASURE_DATE=$(date -u +"%Y-%m-%d %H:%M:%S UTC" 2>/dev/null || date)
OS_UNAME=$(uname -srm 2>/dev/null || echo "Unknown Linux")
OS_NAME="Linux"
if [ -f "/etc/os-release" ]; then
    OS_NAME=$(grep -E '^PRETTY_NAME=' /etc/os-release | cut -d= -f2 | tr -d '"' 2>/dev/null || echo "Linux")
fi

# -----------------------------------------------------------------------------
# Parse Summary from Pipeline Output Log (scripts/run_all.sh)
# -----------------------------------------------------------------------------
extract_summary_metric() {
    local key="$1"
    if [ ! -f "$PIPELINE_LOG" ] || [ ! -s "$PIPELINE_LOG" ]; then
        echo "Unavailable (pipeline output log is missing or empty)"
        return 0
    fi

    if ! grep -q "Pipeline Summary:" "$PIPELINE_LOG"; then
        echo "Unavailable (Pipeline Summary block not reached; pipeline may have aborted early)"
        return 0
    fi

    local val
    val=$(grep -E "^[[:space:]]*${key}:[[:space:]]+[0-9]+" "$PIPELINE_LOG" | tail -n 1 | awk '{print $2}' || true)
    if [[ "$val" =~ ^[0-9]+$ ]]; then
        echo "$val"
    else
        echo "Unavailable (could not parse numeric ${key} count from Pipeline Summary)"
    fi
}

PASS_COUNT=$(extract_summary_metric "PASS")
SKIP_COUNT=$(extract_summary_metric "SKIPPED")
FAIL_COUNT=$(extract_summary_metric "FAIL")

SUMMARY_PARSED=0
if [[ "$PASS_COUNT" =~ ^[0-9]+$ ]] && [[ "$SKIP_COUNT" =~ ^[0-9]+$ ]] && [[ "$FAIL_COUNT" =~ ^[0-9]+$ ]]; then
    SUMMARY_PARSED=1
fi

# -----------------------------------------------------------------------------
# Inventory Required Pipeline Artifacts
# -----------------------------------------------------------------------------
REQUIRED_ARTIFACTS=(
    "outputs/env_check.txt:Host environment diagnostics"
    "outputs/decisions.csv:ML routing decisions"
    "outputs/routing_log.csv:Integrated router dispatch log"
    "outputs/page_trace.txt:Memory access trace"
    "outputs/routing_summary.txt:Router summary statistics"
    "outputs/logs:Per-step execution logs directory"
)

MISSING_REQUIRED_COUNT=0
MISSING_REQUIRED_LIST=()

for item in "${REQUIRED_ARTIFACTS[@]}"; do
    fpath="${item%%:*}"
    if [ ! -e "$fpath" ]; then
        MISSING_REQUIRED_COUNT=$((MISSING_REQUIRED_COUNT + 1))
        MISSING_REQUIRED_LIST+=("$fpath")
    fi
done

# Distinguish overall pipeline exit status from individual step outcomes.
# A zero exit code must not imply full success if steps were skipped, required artifacts are missing,
# or summary counts are unavailable. Do not overwrite a nonzero pipeline exit code with a different status.
STATUS_DESC="UNKNOWN"
if [ "$PIPELINE_EXIT_CODE" -eq 0 ]; then
    if [ "$SUMMARY_PARSED" -ne 1 ]; then
        STATUS_DESC="INDETERMINATE (Exit code 0, but pipeline summary counts unavailable - cannot verify step execution)"
    elif [ "$MISSING_REQUIRED_COUNT" -gt 0 ] && [ "$SKIP_COUNT" -gt 0 ]; then
        STATUS_DESC="PARTIAL PASS (Exit code 0, but ${SKIP_COUNT} step(s) were SKIPPED and ${MISSING_REQUIRED_COUNT} required artifact(s) are missing)"
    elif [ "$MISSING_REQUIRED_COUNT" -gt 0 ]; then
        STATUS_DESC="INCOMPLETE (Exit code 0, but ${MISSING_REQUIRED_COUNT} required artifact(s) are missing from disk)"
    elif [ "$SKIP_COUNT" -gt 0 ]; then
        STATUS_DESC="PARTIAL PASS (Exit code 0, but ${SKIP_COUNT} step(s) were SKIPPED/stubbed - not all steps passed)"
    elif [ "$FAIL_COUNT" -gt 0 ]; then
        STATUS_DESC="ANOMALOUS (Exit code 0, but ${FAIL_COUNT} step(s) reported FAIL in summary)"
    elif [ "$PASS_COUNT" -gt 0 ] && [ "$SKIP_COUNT" -eq 0 ] && [ "$FAIL_COUNT" -eq 0 ] && [ "$MISSING_REQUIRED_COUNT" -eq 0 ] && [ "$TIME_TOOL_STATUS" = "COMPLETE" ]; then
        STATUS_DESC="FULL PASS (Exit code 0, all ${PASS_COUNT} executed steps passed with all required artifacts verified)"
    else
        STATUS_DESC="INCOMPLETE (Exit code 0, but requirements for full pass not satisfied)"
    fi
elif [ "$PIPELINE_EXIT_CODE" -eq 77 ]; then
    STATUS_DESC="SKIPPED (Exit code 77 - entire pipeline command exited with status 77)"
else
    STATUS_DESC="FAIL (Exit code ${PIPELINE_EXIT_CODE})"
    if [ "$SUMMARY_PARSED" -eq 1 ] && [ "$FAIL_COUNT" -gt 0 ]; then
        STATUS_DESC="${STATUS_DESC} - ${FAIL_COUNT} step(s) failed"
    fi
    if [ "$MISSING_REQUIRED_COUNT" -gt 0 ]; then
        STATUS_DESC="${STATUS_DESC}, ${MISSING_REQUIRED_COUNT} required artifact(s) missing"
    fi
fi

# -----------------------------------------------------------------------------
# Generate Markdown Report
# -----------------------------------------------------------------------------
REPORT_WRITE_SUCCESS=0

if {
    echo "# OrbitQueue Pipeline Performance & Resource Measurements"
    echo ""
    echo "## 1. Execution Metadata"
    echo "- **Timestamp:** ${MEASURE_DATE} (Source: system \`date\`)"
    echo "- **Operating System:** ${OS_NAME} (\`${OS_UNAME}\`, Source: \`/etc/os-release\` and \`uname -srm\`)"
    echo "- **Command Measured:** \`/usr/bin/time -v bash scripts/run_all.sh\`"
    echo "- **Raw Output Log:** \`${PIPELINE_LOG}\`"
    echo "- **Raw Timing Log:** \`${TIME_LOG}\`"
    echo "- **Timing Record Status:** \`${TIME_TOOL_STATUS}\`"
    if [ "$TIME_TOOL_STATUS" != "COMPLETE" ]; then
        echo "  - *Timing Tool Diagnostics:* ${TIME_TOOL_ERROR}"
    fi
    echo "- **Pipeline Exit Status:** \`${PIPELINE_EXIT_CODE}\` (${STATUS_DESC})"
    echo ""
    echo "## 2. Resource Utilization & Timing Benchmark"
    if [ "$TIME_TOOL_STATUS" = "FAILED" ]; then
        echo "> [!CAUTION]"
        echo "> **Timing Tool Failed:** ${TIME_TOOL_ERROR}"
        echo ""
    elif [ "$TIME_TOOL_STATUS" = "INCOMPLETE" ]; then
        echo "> [!WARNING]"
        echo "> **Incomplete Timing Record:** ${TIME_TOOL_ERROR} Essential metrics are missing or marked Unavailable."
        echo ""
    fi
    echo "*Extracted directly from \`/usr/bin/time -v\` output (${TIME_LOG}):*"
    echo ""
    echo "| Metric | Measured Value | Unit | Source |"
    echo "|---|---|---|---|"
    echo "| Elapsed (Wall Clock) Time | ${WALL_CLOCK} | h:mm:ss or m:ss | \`/usr/bin/time -v\` |"
    echo "| Maximum Resident Set Size | ${MAX_RSS} | Kilobytes (KB) | \`/usr/bin/time -v\` |"
    echo "| User CPU Time | ${USER_TIME} | Seconds | \`/usr/bin/time -v\` |"
    echo "| System CPU Time | ${SYS_TIME} | Seconds | \`/usr/bin/time -v\` |"
    echo "| CPU Utilization | ${CPU_PERCENT} | Percentage | \`/usr/bin/time -v\` |"
    echo "| Minor (Soft) Page Faults | ${MINOR_FAULTS} | Count | \`/usr/bin/time -v\` |"
    echo "| Major (I/O) Page Faults | ${MAJOR_FAULTS} | Count | \`/usr/bin/time -v\` |"
    echo "| Voluntary Context Switches | ${VOL_SWITCHES} | Count | \`/usr/bin/time -v\` |"
    echo "| Involuntary Context Switches | ${INVOL_SWITCHES} | Count | \`/usr/bin/time -v\` |"
    echo ""
    echo "## 3. Pipeline Step Results"
    echo "- **Passed Steps:** ${PASS_COUNT}"
    echo "- **Skipped Steps:** ${SKIP_COUNT}"
    echo "- **Failed Steps:** ${FAIL_COUNT}"
    echo ""

    # Clear alerting on failures, skipped steps, or indeterminate results
    if [ "$SUMMARY_PARSED" -ne 1 ]; then
        echo "> [!WARNING]"
        echo "> **Indeterminate Execution Results:** Pipeline summary counts could not be parsed from pipeline output log. Step execution outcomes cannot be verified."
        echo ""
    fi

    if [[ "$FAIL_COUNT" =~ ^[0-9]+$ ]] && [ "$FAIL_COUNT" -gt 0 ]; then
        echo "> [!CAUTION]"
        echo "> **Pipeline Failure Detected:** ${FAIL_COUNT} step(s) failed during execution (Pipeline exit code: ${PIPELINE_EXIT_CODE}). See logs in \`outputs/logs/\`."
        echo ""
    elif [ "$PIPELINE_EXIT_CODE" -ne 0 ] && [ "$PIPELINE_EXIT_CODE" -ne 77 ]; then
        echo "> [!CAUTION]"
        echo "> **Pipeline Command Failed:** The pipeline script exited with non-zero status \`${PIPELINE_EXIT_CODE}\`."
        echo ""
    fi

    if [[ "$SKIP_COUNT" =~ ^[0-9]+$ ]] && [ "$SKIP_COUNT" -gt 0 ]; then
        echo "> [!WARNING]"
        echo "> **Incomplete Coverage (Skipped Steps Present):** ${SKIP_COUNT} step(s) were SKIPPED (stubs or non-implemented components). Even though the overall pipeline command returned exit code 0, not all required steps were executed or passed."
        echo ""
    elif [ "$PIPELINE_EXIT_CODE" -eq 0 ] && [ "$FAIL_COUNT" = "0" ] && [ "$SKIP_COUNT" = "0" ] && [[ "$PASS_COUNT" =~ ^[0-9]+$ ]] && [ "$PASS_COUNT" -gt 0 ]; then
        if [ "$MISSING_REQUIRED_COUNT" -eq 0 ] && [ "$TIME_TOOL_STATUS" = "COMPLETE" ]; then
            echo "> [!NOTE]"
            echo "> **Full Pipeline Pass:** All ${PASS_COUNT} executed pipeline steps completed successfully with zero failures, zero skipped components, all required artifacts present, and complete timing metrics."
            echo ""
        fi
    fi

    echo "### Step Execution Summary"
    echo '```'
    if [ -f "$PIPELINE_LOG" ] && [ -s "$PIPELINE_LOG" ]; then
        grep -E '^\[RUN\]' "$PIPELINE_LOG" || echo "No [RUN] step entries found in pipeline log."
    else
        echo "Pipeline output log is missing or empty."
    fi
    echo '```'
    echo ""
    echo "## 4. Experiment Output Artifacts State"
    echo "*Inspected real output artifacts on disk (sizes and line counts):*"
    echo ""

    check_required_artifact() {
        local fpath="$1"
        local desc="$2"
        if [ -f "$fpath" ]; then
            local lines
            lines=$(wc -l < "$fpath" 2>/dev/null || echo "0")
            local size
            size=$(wc -c < "$fpath" 2>/dev/null || echo "0")
            echo "- \`${fpath}\`: **Present** (${desc}) - ${lines} lines, ${size} bytes."
        elif [ -d "$fpath" ]; then
            local count
            count=$(find "$fpath" -type f 2>/dev/null | wc -l || echo "0")
            echo "- \`${fpath}\`: **Present** (${desc}) - ${count} log files recorded."
        else
            echo "- \`${fpath}\`: **Missing (Required Pipeline Artifact)** (${desc})."
        fi
    }

    check_optional_artifact() {
        local fpath="$1"
        local desc="$2"
        if [ -f "$fpath" ]; then
            local lines
            lines=$(wc -l < "$fpath" 2>/dev/null || echo "0")
            local size
            size=$(wc -c < "$fpath" 2>/dev/null || echo "0")
            echo "- \`${fpath}\`: **Present (Optional Evidence)** (${desc}) - ${lines} lines, ${size} bytes."
        else
            echo "- \`${fpath}\`: **Not Generated / Optional** (${desc}) - Generated during standalone experiment test runs; not generated by \`scripts/run_all.sh\`."
        fi
    }

    echo "### 4.1 Required Pipeline Artifacts (\`scripts/run_all.sh\`)"
    echo "- **Required Artifacts Missing:** ${MISSING_REQUIRED_COUNT}"
    if [ "$MISSING_REQUIRED_COUNT" -gt 0 ]; then
        echo "> [!WARNING]"
        echo "> **Required Artifacts Missing on Disk:** ${MISSING_REQUIRED_COUNT} required artifact(s) were not found. Full success cannot be claimed."
        echo ""
    fi
    for item in "${REQUIRED_ARTIFACTS[@]}"; do
        fpath="${item%%:*}"
        desc="${item##*:}"
        check_required_artifact "$fpath" "$desc"
    done
    echo ""

    echo "### 4.2 Measurement & Logging Artifacts (\`scripts/measure.sh\`)"
    check_required_artifact "outputs/measure_pipeline.log" "Pipeline execution standard output and error log"
    check_required_artifact "outputs/measure_time.log" "Timing and resource usage log from /usr/bin/time -v"
    echo ""

    echo "### 4.3 Optional Standalone Evidence Files"
    check_optional_artifact "outputs/exp8_tests.txt" "Exp 8 address translation output (from standalone CLI run)"
    check_optional_artifact "outputs/exp10_comparison.txt" "Exp 10 algorithm comparison output (from standalone CLI run)"
    echo ""
    echo "*Note: Absence of optional evidence files does not indicate a pipeline execution failure; pipeline runs capture step output directly in \`outputs/logs/\`.*"

    echo ""
    echo "---"
    echo "*Report generated automatically by \`scripts/measure.sh\` without manual or synthetic fabrication.*"
} > "$REPORT_FILE"; then
    REPORT_WRITE_SUCCESS=1
    echo "Report generated successfully at: $REPORT_FILE"
else
    REPORT_WRITE_SUCCESS=0
    echo "ERROR: Failed to write report file to '$REPORT_FILE'." >&2
fi

# -----------------------------------------------------------------------------
# Exit Status Handling & Precedence
# -----------------------------------------------------------------------------
# Priority 1: Report-writing failure (measurement/reporting failure)
if [ "$REPORT_WRITE_SUCCESS" -ne 1 ]; then
    echo "[MEASUREMENT FAILURE] Measurement aborted due to failure writing report '$REPORT_FILE'." >&2
    if [ "$PIPELINE_EXIT_CODE" -ne 0 ]; then
        exit "$PIPELINE_EXIT_CODE"
    else
        exit 1
    fi
fi

# Priority 2: Timing tool invocation failure (measurement/reporting failure)
if [ "$TIME_TOOL_STATUS" = "FAILED" ]; then
    echo "[MEASUREMENT FAILURE] Timing utility (/usr/bin/time) failed to execute or produce output (${TIME_TOOL_ERROR})." >&2
    if [ "$PIPELINE_EXIT_CODE" -ne 0 ]; then
        exit "$PIPELINE_EXIT_CODE"
    else
        exit 1
    fi
fi

# Priority 3: Actual pipeline failure (checked before evaluating summary availability)
if [ "$PIPELINE_EXIT_CODE" -ne 0 ] && [ "$PIPELINE_EXIT_CODE" -ne 77 ]; then
    if [ "$SUMMARY_PARSED" -eq 1 ] && [ "$FAIL_COUNT" -gt 0 ]; then
        echo "[PIPELINE FAILURE] Pipeline execution failed with exit code ${PIPELINE_EXIT_CODE} (${FAIL_COUNT} step(s) failed)." >&2
    else
        echo "[PIPELINE FAILURE] Pipeline execution failed with exit code ${PIPELINE_EXIT_CODE}." >&2
    fi
    exit "$PIPELINE_EXIT_CODE"
fi

# Priority 4: Timing verification failure (essential metrics missing, measurement/reporting failure)
if [ "$TIME_TOOL_STATUS" = "INCOMPLETE" ]; then
    echo "[MEASUREMENT FAILURE] Measurement verification failed: essential timing metrics are incomplete (${TIME_TOOL_ERROR})." >&2
    if [ "$PIPELINE_EXIT_CODE" -eq 77 ]; then
        exit 77
    else
        exit 1
    fi
fi

# Priority 5: Pipeline summary unavailable on non-failing pipeline run (indeterminate result)
if [ "$SUMMARY_PARSED" -ne 1 ]; then
    echo "[MEASUREMENT FAILURE] Indeterminate result: pipeline summary counts could not be parsed from pipeline log." >&2
    if [ "$PIPELINE_EXIT_CODE" -eq 77 ]; then
        exit 77
    else
        exit 1
    fi
fi

# Priority 6: Skipped pipeline command (exit code 77)
if [ "$PIPELINE_EXIT_CODE" -eq 77 ]; then
    echo "[PARTIAL PASS] Pipeline command exited with code 77 (SKIPPED components)."
    exit 77
fi

# Priority 7: Missing required pipeline artifacts after zero pipeline exit
if [ "$MISSING_REQUIRED_COUNT" -gt 0 ]; then
    echo "[PARTIAL PASS] Pipeline exited with code ${PIPELINE_EXIT_CODE}, but ${MISSING_REQUIRED_COUNT} required pipeline artifact(s) are missing from disk." >&2
    exit 1
fi

# Priority 8: Skipped steps present after zero pipeline exit
if [ "$SKIP_COUNT" -gt 0 ]; then
    echo "[PARTIAL PASS] Pipeline completed with exit code 0, but ${SKIP_COUNT} step(s) were skipped (stubs). Full success not claimed."
    exit 1
fi

# Priority 9: Full pass (all strict criteria verified)
if [ "$PASS_COUNT" -gt 0 ] && [ "$SKIP_COUNT" -eq 0 ] && [ "$FAIL_COUNT" -eq 0 ] && [ "$MISSING_REQUIRED_COUNT" -eq 0 ] && [ "$TIME_TOOL_STATUS" = "COMPLETE" ] && [ "$PIPELINE_EXIT_CODE" -eq 0 ]; then
    echo "[FULL PASS] Pipeline completed all steps successfully with all required artifacts verified."
    exit 0
else
    echo "[MEASUREMENT FAILURE] Incomplete or unverified execution state." >&2
    exit 1
fi
