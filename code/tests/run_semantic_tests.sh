#!/usr/bin/env bash

set -e

for f in tests/semantic_errors/*.c; do
  echo "===== $f ====="
  # We expect mccomp to fail on all these
  if ./mccomp "$f" > /dev/null 2>&1; then
    echo "ERROR: $f compiled without error, expected semantic error"
    exit 1
  else
    echo "OK: $f rejected with a semantic error"
  fi
  echo
done

echo "All semantic error tests behaved as expected"
