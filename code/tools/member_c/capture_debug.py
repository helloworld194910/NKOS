"""Linux/WSL: capture two GDB sessions against a QEMU 4.1.1 debug build."""
import argparse
from pathlib import Path
import subprocess
import time
import os
import signal

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', required=True)
parser.add_argument('--source', required=True)
parser.add_argument('--step', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
artifact_root = root.parent/'report' if (root.parent/'report/report.md').exists() else root
fixture = root  # Debug the integrated kernel, never a substitute PMM fixture.
evidence = artifact_root / 'evidence/member-c'
evidence.mkdir(parents=True, exist_ok=True)
build = root / 'build/member-c-debug'
build.mkdir(parents=True, exist_ok=True)
nm = subprocess.check_output(['riscv64-unknown-elf-nm', str(fixture/'bin/kernel')], text=True)
symbols = {line.split()[2]: int(line.split()[0], 16) for line in nm.splitlines() if len(line.split()) == 3}
va = symbols['mmu_probe_word']
bare_word = symbols['boot_hartid'] - 0xffffffff40000000
source = Path(args.source).resolve()
helper = source/'target/riscv/cpu_helper.c'
lines = helper.read_text().splitlines()
pte_line = next(i+1 for i, line in enumerate(lines) if 'target_ulong ppn = pte >>' in line)
fill_line = next(i+1 for i, line in enumerate(lines) if 'if (mode == PRV_M && access_type != MMU_INST_FETCH)' in line and i > 400)
tlb_source = source/'accel/tcg/cputlb.c'
tlb_lines = tlb_source.read_text().splitlines()
query_line = next(i+1 for i, line in enumerate(tlb_lines) if 'if (!tlb_hit(tlb_addr, addr))' in line and i > 1240)
tag = 'step' if args.step else 'trace'
host_script = build/f'host-{tag}.gdb'
guest_script = build/f'guest-{tag}.gdb'
host_python = f'''
import gdb
WATCH = {va}
BARE_WORD = {bare_word}
class WalkReturn(gdb.FinishBreakpoint):
    def __init__(self):
        self.out = gdb.parse_and_eval('physical')
        self.prot = gdb.parse_and_eval('prot')
        super().__init__(internal=True)
    def stop(self):
        gdb.write('WALK_RETURN result=%s physical_page=%s prot=%s\\n' % (self.return_value, self.out.dereference(), self.prot.dereference()))
        return False
class Walk(gdb.Breakpoint):
    def stop(self):
        addr = int(gdb.parse_and_eval('addr'))
        caller = gdb.newest_frame().older().name()
        if addr not in (WATCH, 0x80200000, BARE_WORD) or caller != 'riscv_cpu_tlb_fill': return False
        gdb.write('REAL_WALK addr=%#x satp=%s access=%s mmu_idx=%s\\n' % (addr, gdb.parse_and_eval('env->satp'), gdb.parse_and_eval('access_type'), gdb.parse_and_eval('mmu_idx')))
        gdb.execute('bt 7')
        WalkReturn()
        return False
class Pte(gdb.Breakpoint):
    def stop(self):
        if int(gdb.parse_and_eval('addr')) != WATCH: return False
        gdb.write('PTE level_iteration=%s ptshift=%s idx=%s pte_addr=%s pte=%s\\n' % tuple(gdb.parse_and_eval(x) for x in ('i','ptshift','idx','pte_addr','pte')))
        return False
class Fill(gdb.Breakpoint):
    def stop(self):
        if int(gdb.parse_and_eval('address')) not in (WATCH, 0x80200000, BARE_WORD): return False
        gdb.write('FILL addr=%s pa=%s ret=%s prot=%s access=%s\\n' % tuple(gdb.parse_and_eval(x) for x in ('address','pa','ret','prot','access_type')))
        return False
class Query(gdb.Breakpoint):
    def stop(self):
        if int(gdb.parse_and_eval('addr')) != WATCH: return False
        gdb.write('TLB_QUERY index=%s tag=%s addend=%s code_read=%s\\n' % tuple(gdb.parse_and_eval(x) for x in ('index','tlb_addr','entry->addend','code_read')))
        return False
class DebugStop(gdb.Breakpoint):
    def stop(self):
        pc = int(gdb.parse_and_eval('((RISCVCPU*)cpu)->env.pc'))
        if pc not in ({symbols['mmu_probe_load_first']}, {symbols['mmu_probe_load_second']}, {symbols['mmu_probe_load_second']+2}): return False
        gdb.execute('set $fast = &((RISCVCPU*)cpu)->neg.tlb.f[1]')
        gdb.execute('set $index = (%d >> 12) & ($fast->mask / sizeof(CPUTLBEntry))' % WATCH)
        gdb.write('GUEST_STOP pc=%#x TLB_index=%s mask=%s entry=%s\\n' % (pc, gdb.parse_and_eval('$index'),gdb.parse_and_eval('$fast->mask'), gdb.parse_and_eval('$fast->table[$index]')))
        return False
Walk('get_physical_address')
Pte('{helper}:{pte_line}')
Fill('{helper}:{fill_line}')
Query('{tlb_source}:{query_line}')
DebugStop('cpu_handle_debug_exception')
'''
if args.step:
    # Stop inside the actual guest load's fill, then step source statements.
    commands = f'''
break riscv_cpu_tlb_fill if address == {va} && access_type == 0
run
printf "\\nSTEP: real data-load refill\\n"
info args
bt 7
break get_physical_address
continue
info args
next 12
info locals
break {helper}:{pte_line}
continue
info locals
next
next
next
next
next
info locals
finish
info locals
printf "\\nSTEP: translation result before TLB insertion\\n"
print/x pa
print prot
disable breakpoints
continue
'''
else:
    commands = 'python\n' + host_python + '\nend\nrun\n'
host_script.write_text('set pagination off\nset confirm off\nset print pretty off\n' + commands)
guest_script.write_text(f'''set pagination off
set confirm off
set architecture riscv:rv64
file {fixture}/bin/kernel
set tcp auto-retry on
set tcp connect-timeout 30
target remote localhost:12349
break *0x80200000
continue
printf "BARE ENTRY: startup stores precede csrw satp\\n"
print/x $satp
x/10i $pc
delete breakpoints
break mmu_probe_load_first
continue
printf "FIRST LOAD\\n"
print/x $pc
print/x $satp
print/x &mmu_probe_word
x/i $pc
stepi
printf "SECOND LOAD\\n"
x/i $pc
stepi
printf "AFTER BOTH LOADS\\n"
info registers t0 t1 t2 a0 a1 a2 a3 a4 a5
disconnect
quit
''')
qemu_command = [str(Path(args.qemu).resolve()), '-machine', 'virt', '-nographic', '-bios', 'default',
                '-device', f'loader,file={fixture}/bin/ucore.img,addr=0x80200000',
                '-gdb', 'tcp:127.0.0.1:12349', '-S']
host_log = evidence/f'qemu-host-{tag}.log'
guest_log = evidence/f'ucore-guest-{tag}.log'
with host_log.open('w') as hf, guest_log.open('w') as gf:
    proc = subprocess.Popen(['gdb', '-q', '-batch', '-x', str(host_script), '--args', *qemu_command],
                            stdout=hf, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        time.sleep(2)
        result = subprocess.run(['gdb-multiarch','-q','-batch','-x',str(guest_script)],
                                stdout=gf, stderr=subprocess.STDOUT, timeout=55)
        if result.returncode:
            raise RuntimeError(f'guest GDB failed; see {guest_log}')
        time.sleep(1)
    finally:
        if proc.poll() is None:
            os.killpg(proc.pid, signal.SIGTERM)
        try: proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait()
print(host_log)
print(guest_log)
