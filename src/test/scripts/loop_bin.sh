#!/usr/bin/env bash

# Loop an executable until an error is encountered

rc=0
iter=0
while [[ $rc == 0 ]]; do
  echo "Launch instance $iter:" "$@"
  "$@"
  rc=$?
  iter=$((iter+1))
done

