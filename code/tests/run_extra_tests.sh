#!/usr/bin/env bash

set -e

run_test() {
  name="$1"
  dir="tests/$name"
  cfile="$dir/$name.c"
  driver="$dir/driver.cpp"
  exe="$dir/${name}_test"    # put executable inside the test directory

  echo "===== $name ====="

  # Compile MiniC to LLVM IR (stays as output.ll in code/)
  ./mccomp "$cfile"

  # Build and run the test driver with the IR
  clang++ "$driver" output.ll -o "$exe"
  "$exe"

  echo
}

run_test scope_shadow
run_test big_mixed

echo "All extra positive tests ran"
