#!/usr/bin/env python3
"""Build member A switch code with test-only upper-layer adapters."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

MARKERS = ["PASS: S-U-S with user resume and kernel context restoration",
           "PASS: repeated S-U-S and nested-entry rejection",
           "PASS: invalid callback action restores kernel context",
           "RESULT: 3/3 switch cases passed"]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = ap.parse_args()
    root = args.root.resolve()
    build = root / "obj/member-a-switch"
    logs = root / "logs"
    build.mkdir(parents=True, exist_ok=True)
    logs.mkdir(exist_ok=True)
    cc = shutil.which("riscv64-unknown-elf-gcc")
    oc = shutil.which("riscv64-unknown-elf-objcopy")
    qemu = shutil.which("qemu-system-riscv64")
    if not all((cc, oc, qemu)):
        raise RuntimeError("Need RISC-V GCC, objcopy and QEMU in PATH")
    qemu_version = subprocess.check_output([qemu, "--version"], text=True).splitlines()[0]
    print("QEMU_VERSION: " + qemu_version, flush=True)
    sources = ["kern/init/entry.S", "extensions/member_a_switch/switch.S",
               "tests/member_a_switch/fixture.c", "tests/member_a_switch/user_probe.S",
               "tests/member_a_switch/abi_probe.S"]
    report = {"scope": "isolated A switch candidate with test-only upper-layer fixture; NOT joint integration/grade",
              "qemu_version": qemu_version, "passed": False,
              "source_sha256": {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources}}
    (logs/"member-a-switch-result.json").write_text(json.dumps(report, indent=2)+"\n")
    flags = ["-mcmodel=medany", "-mno-relax", "-msmall-data-limit=0", "-std=gnu99", "-O0", "-g",
             "-nostdinc", "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
             "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror",
             "-Ilibs", "-Ikern/mm", "-Ikern/trap", "-Iextensions/member_a_switch"]
    (logs/"member-a-switch-environment.log").write_text(qemu_version+"\n", encoding="utf-8")
    with (logs/"member-a-switch-build.log").open("w") as log:
        def run(cmd):
            print("+", " ".join(map(str, cmd)), flush=True)
            log.write("+ " + " ".join(map(str, cmd)) + "\n"); log.flush()
            p = subprocess.run(cmd, cwd=root, stdout=log, stderr=subprocess.STDOUT)
            if p.returncode:
                raise RuntimeError("Build failed; see logs/member-a-switch-build.log")
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
        (logs/"member-a-switch-qemu.log").write_text(output, encoding="utf-8")
        print(output)
        raise RuntimeError("QEMU timed out; no PASS inferred")
    (logs/"member-a-switch-qemu.log").write_text(output, encoding="utf-8")
    print(output)
    if rc != 0 or "FAIL" in output or any(x not in output for x in MARKERS):
        raise RuntimeError(f"QEMU failed/incomplete (exit={rc}); see QEMU log")
    report.update(passed=True, qemu_exit_code=rc)
    (logs/"member-a-switch-result.json").write_text(json.dumps(report, indent=2)+"\n")
    print("ISOLATED_SWITCH: PASS. Upper-layer agreement, joint integration and full-kernel grading are still pending.")

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as exc:
        print("ERROR:", exc, file=sys.stderr)
        sys.exit(1)
