#!/bin/sh
set -eu

: "${PS5_HOST:?set PS5_HOST to the FW 5.50 console address}"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sequence_gate="$script_dir/run_fw550_2048_sequence0.sh"
pinned_sequence_gate_sha256=77a35be5a271b16aabf516c5043b6821208e1a7bc86b9780dfab2e084f285eeb
qualification_runs=20

if [ ! -f "$sequence_gate" ]; then
    echo "missing pinned 2048 sequence-zero gate: $sequence_gate" >&2
    exit 2
fi
actual=$(shasum -a 256 "$sequence_gate" | awk '{print $1}')
if [ "$actual" != "$pinned_sequence_gate_sha256" ]; then
    echo "2048 sequence-zero gate SHA-256 mismatch: expected=$pinned_sequence_gate_sha256 actual=$actual" >&2
    exit 2
fi

run=1
while [ "$run" -le "$qualification_runs" ]; do
    echo "eden-ps5 multithreaded JIT stress run $run/$qualification_runs"
    "$sequence_gate"
    run=$((run + 1))
done

echo "eden-ps5 FW 5.50 multithreaded JIT ${qualification_runs}-relaunch gate: PASS"
