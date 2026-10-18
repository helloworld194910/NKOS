"""Record executable/source identities alongside the captured evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
from datetime import datetime
from zoneinfo import ZoneInfo

p = argparse.ArgumentParser()
p.add_argument('--qemu', required=True)
p.add_argument('--source', required=True)
p.add_argument('--archive')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
artifact_root = root.parent/'report' if (root.parent/'report/report.md').exists() else root
source = Path(a.source).resolve()
def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def command(argv):
    return subprocess.check_output(argv, text=True, stderr=subprocess.STDOUT).strip()
data = {
    'captured_at': datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(),
    'host': platform.platform(),
    'os_release': Path('/etc/os-release').read_text(),
    'qemu_executable': str(Path(a.qemu).resolve()),
    'qemu_source': str(source),
    'qemu_version': command([a.qemu, '--version']),
    'configure': './configure --target-list=riscv64-softmmu --enable-debug --disable-werror --disable-docs --python=python3',
    'compiler': command(['riscv64-unknown-elf-gcc', '--version']),
    'host_gdb': command(['gdb','--version']),
    'guest_gdb': command(['gdb-multiarch','--version']),
    'qemu_sha256': sha(a.qemu),
    'integrated_kernel_sha256': sha(root/'bin/kernel'),
    'source_sha256': {},
    'member_c_sha256': {},
}
for name in ('target/riscv/cpu_helper.c', 'accel/tcg/cputlb.c', 'tcg/i386/tcg-target.inc.c',
             'include/exec/cpu_ldst.h', 'include/exec/cpu-defs.h'):
    data['source_sha256'][name] = sha(source/name)
for name in ('slub.c','slub.h','slub_check.c','mmu_probe.c'):
    data['member_c_sha256'][name] = sha(root/'kern/mm'/name)
if a.archive:
    data['source_archive_sha256'] = sha(a.archive)
    data['source_archive_url'] = 'https://download.qemu.org/qemu-4.1.1.tar.xz'
target = artifact_root/'evidence/member-c/environment.json'
target.write_text(json.dumps(data, indent=2, ensure_ascii=False)+'\n')
# Keep relevant original source statements with exact version line numbers.
snippets = []
for name, ranges in {
    'target/riscv/cpu_helper.c': [(155,208),(223,290),(339,366),(434,484)],
    'accel/tcg/cputlb.c': [(868,880),(977,1006),(1248,1282),(1337,1355)],
    'tcg/i386/tcg-target.inc.c': [(1699,1780)],
}.items():
    lines = (source/name).read_text().splitlines()
    for first,last in ranges:
        snippets.append(f'\n### QEMU 4.1.1 {name}:{first}-{last}\n')
        snippets.extend(f'{i}: {lines[i-1]}' for i in range(first,last+1))
(target.parent/'qemu-source-excerpts.txt').write_text('\n'.join(snippets)+'\n')
print(target)
