#!/usr/bin/env python3
"""Reproducible QEMU tests of production B sources, without C's PMM/handler.

Run from a Linux/WSL environment with the existing RISC-V tools in PATH.
No production source or grading script is injected or rewritten.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[2]
REPO = ROOT.parent
EXTENSION = Path(__file__).resolve().parent
SOURCES = ["kern/init/entry.S", "kern/trap/trapentry.S", "kern/trap/trap.c",
           "kern/driver/clock.c", "kern/driver/intr.c", "libs/sbi.c",
           "libs/printfmt.c", "libs/string.c", str(EXTENSION / "fixture.c"),
           str(EXTENSION / "vector.S")]
CASES = {
    "handler": (1, [], ["PASS: classifications", "PASS: timer boundaries"], 1),
    "timer-default": (2, [], ["PASS: real sustained timer", "Shutdown after 10 reports (ticks=1000)"], 10),
    "timer-three": (3, ["-DSHUTDOWN_AFTER=3"], ["PASS: real sustained timer", "Shutdown after 3 reports (ticks=300)"], 3),
    "debug-grade": (4, ["-DDEBUG_GRADE"], ["End of Test.", "PASS: DEBUG_GRADE"], 1),
    "sync": (5, [], ["PASS: initial SIE=0", "PASS: initial SIE=1 and nested",
                     "PASS: save/restore macros", "PASS: pending timer deferred",
                     "PASS: existing atomic"], 0),
    "stvec": (6, [], ["PASS: mode=0 synchronous", "PASS: mode=0 software",
                      "PASS: mode=0 timer", "PASS: stvec restored"], 0),
}

def select_march(cc, build, logs):
    """Probe CSR/fence/AMO assembly instead of guessing from GCC's version.

    Older toolchains include CSR and fence.i in their rv64imac ISA spelling;
    newer ones require explicit Zicsr/Zifencei extension names.
    """
    source = ".text\ncsrr a0, sstatus\nfence.i\namoadd.d a0, a1, (a2)\n"
    with (logs / "isa-probe-build.log").open("w", encoding="utf-8") as log:
        for march in ("rv64imac_zicsr_zifencei", "rv64imac"):
            command = [cc, "-march=" + march, "-mabi=lp64", "-x", "assembler",
                       "-c", "-", "-o", str(build / "isa-probe.o")]
            probe = subprocess.run(command, cwd=ROOT, input=source,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True)
            log.write("+ " + " ".join(command) + "\n" + probe.stdout)
            log.write("exit=" + str(probe.returncode) + "\n")
            if probe.returncode == 0:
                return march
    raise RuntimeError("Compiler cannot assemble required CSR/fence/AMO instructions; "
                       "inspect " + str(logs / "isa-probe-build.log"))

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--cases", nargs="+", choices=list(CASES), default=list(CASES))
    args = ap.parse_args()
    # Keep isolated artifacts outside obj/, which make clean removes.
    build = ROOT / "build/member-b"
    logs = ROOT / "logs/member-b"
    build.mkdir(parents=True, exist_ok=True)
    logs.mkdir(parents=True, exist_ok=True)
    cc = shutil.which("riscv64-unknown-elf-gcc")
    oc = shutil.which("riscv64-unknown-elf-objcopy")
    od = shutil.which("riscv64-unknown-elf-objdump")
    qemu = shutil.which("qemu-system-riscv64")
    if not all((cc, oc, od, qemu)):
        raise RuntimeError("Need RISC-V GCC/objcopy/objdump and QEMU in PATH")
    march = select_march(cc, build, logs)
    flags = ["-march=" + march, "-mabi=lp64", "-mcmodel=medany",
             "-Dsbadaddr=0x143",
             "-mno-relax", "-msmall-data-limit=0", "-std=gnu99", "-O2", "-g",
             "-nostdinc", "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
             "-ffunction-sections", "-fdata-sections", "-Wall", "-Werror",
             "-Ilibs", "-Ikern/mm", "-Ikern/trap", "-Ikern/driver",
             "-Ikern/debug", "-Ikern/sync"]
    hashes = {p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in
              SOURCES + ["kern/sync/sync.h", "libs/atomic.h", "tools/kernel.ld",
                         str(Path(__file__).resolve()), "libs/riscv.h", "libs/defs.h",
                         "kern/trap/trap.h", "libs/sbi.h", "libs/stdio.h",
                         "libs/stdarg.h", "kern/mm/mmu.h", "kern/mm/memlayout.h",
                         "kern/driver/clock.h", "kern/driver/intr.h"]}
    report = {"scope": "B production handler + A assembly, isolated boot/console/exception fixture; NOT full make grade/PMM/C exception tests",
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "base_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip(),
              "gcc": subprocess.check_output([cc, "--version"], text=True).splitlines()[0],
              "march": march,
              "qemu": subprocess.check_output([qemu, "--version"], text=True).splitlines()[0],
              "source_sha256": hashes, "cases": {}, "passed": False}
    result_file = logs / "result.json"
    def save():
        result_file.write_text(json.dumps(report, indent=2, ensure_ascii=False)+"\n", encoding="utf-8")
    save()
    print(report["gcc"], report["qemu"], sep="\n", flush=True)
    print("ISA flags: -march=" + march, flush=True)

    def run_build(cmd, log):
        log.write("+ " + " ".join(map(str, cmd)) + "\n"); log.flush()
        p = subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if p.returncode:
            raise RuntimeError("Build failed; inspect " + log.name)

    for name in args.cases:
        number, defs, markers, expected_reports = CASES[name]
        directory = build / name
        directory.mkdir(exist_ok=True)
        report["cases"][name] = {"passed": False, "defines": [f"-DTEST_CASE={number}", *defs]}
        save()
        print("Building " + name, flush=True)
        with (logs / (name + "-build.log")).open("w", encoding="utf-8") as log:
            objects = []
            for i, source in enumerate(SOURCES):
                obj = directory / f"part{i}.o"
                test_dispatch = ["-Dtrap=member_b_unused_production_trap",
                                 "-Dexception_handler=member_b_unused_production_exception"] if source == "kern/trap/trap.c" else []
                run_build([cc, *flags, f"-DTEST_CASE={number}", *defs, *test_dispatch,
                           "-c", source, "-o", str(obj)], log)
                objects.append(str(obj))
            kernel = directory / "kernel"
            run_build([cc, *flags, "-nostdlib", "-Wl,--gc-sections,--no-relax,-T,tools/kernel.ld",
                       *objects, "-o", str(kernel)], log)
            img = directory / "ucore.img"
            run_build([oc, str(kernel), "--strip-all", "-O", "binary", str(img)], log)
        if name == "stvec":
            disasm = subprocess.check_output([od, "-d", str(kernel)], text=True)
            (logs / "stvec-disassembly.log").write_text(disasm, encoding="utf-8")
        command = [qemu, "-machine", "virt", "-nographic", "-bios", "default",
                   "-m", "128M", "-smp", "1", "-device", f"loader,file={img},addr=0x80200000"]
        print("Running " + name, flush=True)
        try:
            proc = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, timeout=25)
        except subprocess.TimeoutExpired as exc:
            (logs / (name + "-qemu.log")).write_bytes(exc.stdout or b"")
            raise RuntimeError(name + " timed out; no PASS inferred")
        output = proc.stdout.decode("utf-8", errors="replace")
        (logs / (name + "-qemu.log")).write_text(output, encoding="utf-8")
        actual_reports = len(re.findall(r"(?m)^100 ticks\r?$", output))
        missing = [m for m in markers if m not in output]
        if name == "handler" and output.count("Unknown interrupt: cause 15") != 2:
            missing.append("both raw and flagged unknown diagnostics")
        if name == "stvec":
            if "SKIP: Vectored request not retained" in output:
                report["cases"][name]["vectored"] = "unsupported/readback mismatch; skipped, not passed"
            else:
                for m in ["PASS: mode=1 synchronous", "PASS: mode=1 software interrupt slot=1",
                          "PASS: mode=1 timer slot=5"]:
                    if m not in output: missing.append(m)
                report["cases"][name]["vectored"] = "tested"
        if "Shutdown after" in output and name not in ("timer-default", "timer-three"):
            missing.append("unexpected normal shutdown path")
        ok = proc.returncode == 0 and "FAIL:" not in output and not missing and actual_reports == expected_reports
        report["cases"][name].update(passed=ok, qemu_exit_code=proc.returncode,
            statistics_lines=actual_reports, expected_statistics_lines=expected_reports,
            missing=missing, command=command)
        save()
        print("\n".join(line for line in output.splitlines()
                        if line.startswith(("PASS:", "FAIL:", "SKIP:", "stvec ", "Shutdown"))), flush=True)
        if not ok:
            raise RuntimeError(f"{name} failed: missing={missing}, reports={actual_reports}, rc={proc.returncode}")
        print(name + ": PASS", flush=True)

    # Invalid configuration must be rejected by the actual source at compile time.
    invalid = subprocess.run([cc, *flags, "-DSHUTDOWN_AFTER=0", "-c", "kern/trap/trap.c",
                              "-o", str(build / "invalid.o")], cwd=ROOT,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (logs / "invalid-threshold-build.log").write_text(invalid.stdout, encoding="utf-8")
    ok = invalid.returncode != 0 and "SHUTDOWN_AFTER must be a positive" in invalid.stdout
    report["invalid_threshold_rejected"] = ok
    report["passed"] = ok and all(v["passed"] for v in report["cases"].values())
    report["completed_utc"] = datetime.now(timezone.utc).isoformat()
    save()
    if not report["passed"]:
        raise RuntimeError("invalid-threshold rejection did not work")
    print(f"RESULT: {len(args.cases)}/{len(args.cases)} isolated cases passed; invalid threshold rejected. NOT original grading.", flush=True)

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as exc:
        print("ERROR:", exc, file=sys.stderr)
        sys.exit(1)
