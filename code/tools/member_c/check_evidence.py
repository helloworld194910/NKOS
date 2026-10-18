"""Audit final evidence without treating old fixtures as final integration."""
from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parents[2]
repo = root.parent if (root.parent/'report/report.md').exists() else root
artifact_root = repo/'report' if repo != root else root
e = artifact_root / 'evidence/member-c'
def read(name):
    return (e / name).read_text(encoding='utf-8')

host, guest, step = (read(n) for n in (
    'qemu-host-trace.log', 'ucore-guest-trace.log', 'qemu-host-step.log'))
assert 'Traceback' not in host and 'Python Exception' not in host
assert 'satp=0 access=1' in host
assert 'TLB_QUERY' in host and 'tag=18446744073709551615' in host
assert 'PTE level_iteration=0 ptshift=18 idx=511' in host
assert 'FIRST LOAD' in guest and 'SECOND LOAD' in guest
assert guest.count('0x1122334455667788') == 2
walks = re.findall(r'REAL_WALK addr=(0xffffffff[0-9a-f]+).*access=0', host)
assert len(walks) == 1, walks
assert host.count('GUEST_STOP') == 3
assert 'STEP: translation result before TLB insertion' in step
assert 'Value returned is $1 = 0' in step and '0x80208000' in step
for name, manager in (
    ('slub-qemu-final.log', 'best_fit_pmm_manager'),
    ('slub-qemu-buddy.log', 'buddy_pmm_manager'),
):
    output = read(name)
    assert 'memory management: ' + manager in output
    assert 'check_alloc_page() succeeded!' in output
    assert 'panic' not in output.lower()
    initial = re.search(r'register (\d+) free pages', output)
    restored = re.search(r'slub_check\(\) succeeded! free pages restored: (\d+)', output)
    assert initial and restored and initial[1] == restored[1], name
    if manager.startswith('buddy'):
        assert output.count('buddy_check() basic invariants succeeded!') == 2
default = read('pmm-default.log')
assert 'memory management: best_fit_pmm_manager' in default
assert 'check_alloc_page() succeeded!' in default and 'panic' not in default.lower()
output = read('slub-host.log')
assert output.count('slub_check() succeeded!') == 2
assert 'injected PMM failure and fragmentation passed' in output
assert not any(s in output for s in ('AddressSanitizer', 'runtime error:', 'PANIC'))
assert 'RESULT: 28/28 passed' in read('buddy-host.log')
assert 'Total Score: 20/20' in read('grade.log')
provenance = json.loads(read('integration.json'))
for name, entry in provenance['comparison_with_B_code'].items():
    data = (root/name).read_bytes().replace(b'\r\n', b'\n')
    assert hashlib.sha256(data).hexdigest() == entry['final_sha256_lf'], name
best = (root/'kern/mm/best_fit_pmm.c').read_text(encoding='utf-8')
tests = best[best.index('static void\nbasic_check(void)'):].strip()
assert hashlib.sha256(tests.encode()).hexdigest() == provenance['bestfit_framework_tests_sha256_lf_stripped']
cases = list((e/'buddy-host-cases').glob('*.log'))
assert len(cases) == 28
assert sum(p.read_text().startswith('exit_code=0\n') for p in cases) == 10
assert sum(p.read_text().startswith('exit_code=86\n') for p in cases) == 18
env = json.loads(read('environment.json'))
for name, digest in env['member_c_sha256'].items():
    assert hashlib.sha256((root/'kern/mm'/name).read_bytes()).hexdigest() == digest, name
for item in json.loads((artifact_root/'images/sources.json').read_text(encoding='utf-8')):
    assert hashlib.sha256((artifact_root/'images'/item['file']).read_bytes()).hexdigest() == item['sha256']
for doc in repo.rglob('*.md'):
    body = re.sub(r'```.*?```', '', doc.read_text(encoding='utf-8'), flags=re.S)
    for target in re.findall(r'\]\(([^)]+)\)', body):
        if '://' in target or target.startswith('#'):
            continue
        path = target.split('#', 1)[0]
        assert (doc.parent/path).exists(), (doc.relative_to(root), path)
print('Evidence audit passed: final Best-Fit/Buddy SLUB, 28 Buddy cases, grade 20/20,')
print('Bare/Sv39 stepping, two-load TLB evidence, source hashes and image links.')
