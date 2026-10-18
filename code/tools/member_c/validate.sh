#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
artifact_root=.
if [ -f ../report/report.md ]; then artifact_root=../report; fi
mkdir -p "$artifact_root/evidence/member-c"
make member-c-host
make buddy-host-test > "$artifact_root/evidence/member-c/buddy-host.log" 2>&1
mkdir -p "$artifact_root/evidence/member-c/buddy-host-cases"
cp obj/buddy-host/logs/*.log "$artifact_root/evidence/member-c/buddy-host-cases/"
make clean > "$artifact_root/evidence/member-c/build-default.log" 2>&1
make >> "$artifact_root/evidence/member-c/build-default.log" 2>&1
python3 tools/member_c/run_qemu.py --pmm-only --output pmm-default.log
make clean > "$artifact_root/evidence/member-c/build-buddy.log" 2>&1
make BUDDY=1 SLUB_CHECK=1 >> "$artifact_root/evidence/member-c/build-buddy.log" 2>&1
python3 tools/member_c/run_qemu.py --manager buddy_pmm_manager --output slub-qemu-buddy.log
make clean > "$artifact_root/evidence/member-c/build-bestfit.log" 2>&1
make SLUB_CHECK=1 MMU_PROBE=1 >> "$artifact_root/evidence/member-c/build-bestfit.log" 2>&1
python3 tools/member_c/run_qemu.py
if [ -n "${QEMU_DEBUG_SOURCE:-}" ]; then
    debug_qemu="$QEMU_DEBUG_SOURCE/riscv64-softmmu/qemu-system-riscv64"
    python3 tools/member_c/capture_debug.py --qemu "$debug_qemu" --source "$QEMU_DEBUG_SOURCE"
    python3 tools/member_c/capture_debug.py --qemu "$debug_qemu" --source "$QEMU_DEBUG_SOURCE" --step
    python3 tools/member_c/record_environment.py --qemu "$debug_qemu" --source "$QEMU_DEBUG_SOURCE"
fi
make grade > "$artifact_root/evidence/member-c/grade.log" 2>&1
cat "$artifact_root/evidence/member-c/grade.log"
make clean
