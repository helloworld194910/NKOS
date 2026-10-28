#!/usr/bin/env python3
"""Build an isolated kernel with the real production trapentry.S."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

MARKERS = ["PASS: 32-bit breakpoint and all GPRs",
           "PASS: 16-bit breakpoint and all GPRs",
           "PASS: handler edits GPRs, epc and SPIE",
           "RESULT: 3/3 context scenarios passed"]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = ap.parse_args()
    root = args.root.resolve()
    build = root / "obj/member-a-context"
    logs = root / "logs"
    build.mkdir(parents=True, exist_ok=True)
    logs.mkdir(exist_ok=True)
    cc = shutil.which("riscv64-unknown-elf-gcc")
    oc = shutil.which("riscv64-unknown-elf-objcopy")
    qemu = shutil.which("qemu-system-riscv64")
    if not all((cc, oc, qemu)):
        raise RuntimeError("Need RISC-V GCC, objcopy and QEMU in PATH")
    sources = ["kern/init/entry.S", "kern/trap/trapentry.S",
               "tests/member_a/context_fixture.c", "tests/member_a/context_probe.S"]
    report = {"scope": "isolated member-A context test, NOT full kernel/grade/U-mode",
              "source_sha256": {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources},
              "passed": False}
    (logs/"member-a-context-result.json").write_text(json.dumps(report, indent=2)+"\n")
    flags = ["-mcmodel=medany", "-mno-relax", "-msmall-data-limit=0", "-std=gnu99", "-O0", "-g",
             "-nostdinc", "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
             "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror",
             "-Ilibs", "-Ikern/mm", "-Ikern/trap"]
    with (logs/"member-a-context-build.log").open("w") as log:
        def run(cmd):
            print("+", " ".join(map(str, cmd)), flush=True)
            log.write("+ " + " ".join(map(str, cmd)) + "\n"); log.flush()
            p = subprocess.run(cmd, cwd=root, stdout=log, stderr=subprocess.STDOUT)
            if p.returncode:
                raise RuntimeError("Build failed; see logs/member-a-context-build.log")
        objects=[]
        for n, src in enumerate(sources):
            obj=build/f"part{n}.o"
            run([cc, *flags, "-c", src, "-o", str(obj)]); objects.append(str(obj))
        kernel=build/"kernel"
        run([cc, "-mcmodel=medany", "-mno-relax", "-nostdlib",
             "-Wl,--gc-sections,--no-relax,-T,tools/kernel.ld", *objects, "-o", str(kernel)])
        image=build/"ucore.img"
        run([oc, str(kernel), "--strip-all", "-O", "binary", str(image)])
    cmd=[qemu, "-machine", "virt", "-nographic", "-bios", "default", "-m", "128M",
         "-smp", "1", "-device", f"loader,file={image},addr=0x80200000"]
    print("Launching isolated QEMU (15 second limit)...", flush=True)
    try:
        proc=subprocess.run(cmd, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)
        output=proc.stdout.decode("utf-8", errors="replace")
        rc=proc.returncode
    except subprocess.TimeoutExpired as exc:
        output=(exc.stdout or b"").decode("utf-8", errors="replace")
        (logs/"member-a-context-qemu.log").write_text(output, encoding="utf-8")
        print(output)
        raise RuntimeError("QEMU timed out; no PASS inferred")
    (logs/"member-a-context-qemu.log").write_text(output, encoding="utf-8")
    print(output)
    if rc != 0 or "FAIL" in output or any(x not in output for x in MARKERS):
        raise RuntimeError(f"QEMU failed/incomplete (exit={rc}); see QEMU log")
    report.update(passed=True, qemu_exit_code=rc)
    (logs/"member-a-context-result.json").write_text(json.dumps(report, indent=2)+"\n")
    print("ISOLATED_CONTEXT: PASS. Full-kernel grading and U-mode challenge are still pending.")

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError) as exc:
        print("ERROR:", exc, file=sys.stderr)
        sys.exit(1)
