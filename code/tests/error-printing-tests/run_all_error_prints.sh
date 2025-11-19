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

    # run compiler and capture return code
    OUTPUT=$($COMPILER "$f" 2>&1)
    RET=$?

    echo "$OUTPUT"
    echo

    # check exit status
    if [ $RET -ne 0 ]; then
        echo "[FAIL]  Compiler exited with status $RET"
        FAILED=$((FAILED + 1))
    else
        echo "[PASS]  Compiler exited normally"
    fi

    echo "----------------------------------------------"
    echo
done

echo "=============================================="
echo "Tests completed: $TOTAL"
echo "Failures:        $FAILED"
echo "Successes:       $((TOTAL - FAILED))"
echo "=============================================="
