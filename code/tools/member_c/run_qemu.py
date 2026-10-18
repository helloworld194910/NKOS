"""Run a built kernel, requiring a real SLUB success marker, then stop QEMU."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--qemu', default='qemu-system-riscv64')
p.add_argument('--manager', choices=('best_fit_pmm_manager', 'buddy_pmm_manager'),
               default='best_fit_pmm_manager')
p.add_argument('--output', default='slub-qemu-final.log')
p.add_argument('--pmm-only', action='store_true')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
artifact_root = root.parent/'report' if (root.parent/'report/report.md').exists() else root
work = root
image = work/'bin/ucore.img'
if not image.exists():
    raise SystemExit('Build kernel first: missing ' + str(image))
command = [a.qemu, '-machine','virt','-nographic','-bios','default',
           '-device',f'loader,file={image},addr=0x80200000']
process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
try:
    output, _ = process.communicate(timeout=12)
except subprocess.TimeoutExpired:
    process.terminate()
    output, _ = process.communicate(timeout=5)
target = artifact_root/'evidence/member-c'/a.output
target.parent.mkdir(parents=True, exist_ok=True)
target.write_bytes(output)
print(output.decode(errors='replace'))
required = [('memory management: ' + a.manager).encode(),
            b'check_alloc_page() succeeded!']
if not a.pmm_only:
    required.append(b'slub_check() succeeded!')
if any(marker not in output for marker in required) or b'panic' in output.lower():
    raise SystemExit('Manager/PMM/SLUB validation failed; inspect ' + str(target))
