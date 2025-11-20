#!/bin/bash

# Path to your compiler executable
COMPILER="../../mccomp"

echo "=============================================="
echo "         MiniC Error-Printing Test Suite       "
echo "=============================================="
echo

TOTAL=0
FAILED=0

for f in *.c; do
    TOTAL=$((TOTAL + 1))

    echo "========== Running $f =========="

    OUTPUT=$($COMPILER "$f" 2>&1)
    RET=$?

    echo "$OUTPUT"
    echo

    # Decide pass / fail based on error text and exit code
    if echo "$OUTPUT" | grep -q "error:"; then
        # Compiler reported an error
        if [ $RET -ne 0 ]; then
            echo "[PASS]  Correctly reported an error and exited with status $RET"
        else
            echo "[FAIL]  Reported an error but exit status was 0"
            FAILED=$((FAILED + 1))
        fi
    else
        # Compiler reported no error
        if [ $RET -eq 0 ]; then
            echo "[PASS]  Accepted program with no errors"
        else
            echo "[FAIL]  No error text but exit status was $RET"
            FAILED=$((FAILED + 1))
        fi
    fi

    echo "----------------------------------------------"
    echo
done

echo "=============================================="
echo "Tests completed: $TOTAL"
echo "Failures:        $FAILED"
echo "Successes:       $((TOTAL - FAILED))"
echo "=============================================="
if [ $FAILED -eq 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "Some tests failed."
    exit 1
fi