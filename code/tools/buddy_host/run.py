#!/usr/bin/env python3
"""Run each real-allocator scenario in a separate bounded process."""
import pathlib
import re
import subprocess
import sys

NORMAL = {
    "empty": "Case 1",
    "orders": "Case 2",
    "rounding": "Case 3",
    "split": "Case 4",
    "merge": "Case 5",
    "nonpartners": "Case 6",
    "irregular": "Case 7",
    "registration": "Case 8",
    "failure": "Case 9",
    "interleave": "Case 10",
}

# Match the exact assertion AND its source file, after the armed operation.
NEGATIVE = {
    "alloc_zero": "initialized && n > 0",
    "free_zero": "n > 0",
    "free_null": "page != NULL && address >= start",
    "free_outside": "index < count",
    "free_unaligned": "offset % sizeof(struct Page) == 0",
    "free_interior": "records[index].state == BUDDY_ALLOC_HEAD",
    "free_length": "records[index].requested == n",
    "free_rounded": "records[index].requested == n",
    "free_twice": "records[index].state == BUDDY_ALLOC_HEAD",
    "free_reserved": "records[index].state == BUDDY_ALLOC_HEAD",
    "free_unregistered": "records[index].state == BUDDY_ALLOC_HEAD",
    "register_zero": "n > 0 && n <= count - index",
    "register_start": "index < count",
    "register_end": "n > 0 && n <= count - index",
    "register_nonreserved": "PageReserved(&pages[i])",
    "register_twice": "records[i].state == BUDDY_UNMANAGED",
    "register_overlap": "records[i].state == BUDDY_UNMANAGED",
    "capacity": "count <= BUDDY_CAPACITY",
}


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: run.py BUDDY_TEST_EXECUTABLE")
    executable = str(pathlib.Path(sys.argv[1]).resolve())
    logs = pathlib.Path(executable).parent / "logs"
    logs.mkdir(exist_ok=True)
    passed = 0
    for name in list(NORMAL) + list(NEGATIVE):
        try:
            result = subprocess.run(
                [executable, name], stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, text=True, timeout=15,
            )
            output = result.stdout
            if name in NORMAL:
                okay = result.returncode == 0 and "NORMAL_CASE_PASS" in output
                label = NORMAL[name]
            else:
                label = "Case 12" if name.startswith("register_") or name == "capacity" else "Case 11"
                pattern = (r"^BUDDY_PANIC\|[^|\r\n]*buddy_pmm\.c\|[0-9]+\|"
                           r"assertion failed: " + re.escape(NEGATIVE[name]) + r"$")
                armed = output.split("EXPECTED_OPERATION\n")
                okay = (result.returncode == 86 and len(armed) == 2
                        and "BUDDY_PANIC|" not in armed[0]
                        and re.search(pattern, armed[1], re.MULTILINE) is not None
                        and "INVALID_OPERATION_RETURNED" not in output)
            (logs / (name + ".log")).write_text(
                "exit_code=" + str(result.returncode) + "\n" + output,
                encoding="utf-8",
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            okay = False
            label = NORMAL.get(name, "Case 11/12")
            output = str(error)
            (logs / (name + ".log")).write_text(output, encoding="utf-8")
        print(("PASS" if okay else "FAIL") + " | " + label + " | " + name)
        if okay:
            passed += 1
        else:
            print(output)
    total = len(NORMAL) + len(NEGATIVE)
    print("RESULT: {}/{} passed; logs: {}".format(passed, total, logs))
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
