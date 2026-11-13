#!/usr/bin/env python3
"""Run real Lab4 PMM/page operations in a disposable kernel copy."""
from pathlib import Path
import argparse, tempfile, shutil, subprocess, hashlib, json

FIXTURE = r'''
#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <dtb.h>
#include <pmm.h>
#include <assert.h>
#include <sbi.h>

int kern_init(void) __attribute__((noreturn));
int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end-edata);
    dtb_init(); cons_init();
    cprintf("MEMBER-A: real PMM and Sv39 page operations\n");
    pmm_init();
    cprintf("PASS: original PMM and page-table checks returned\n");
    size_t baseline=nr_free_pages();
    uintptr_t va=0x40000000;
    assert(boot_pgdir_va[PDX1(va)]==0);
    struct Page *a=alloc_page(), *b=alloc_page();
    assert(a && b);
    *(volatile uint64_t *)page2kva(a)=0x11223344;
    *(volatile uint64_t *)page2kva(b)=0x55667788;
    assert(page_insert(boot_pgdir_va,a,va,PTE_R|PTE_W)==0);
    assert(*(volatile uint64_t *)va==0x11223344);
    assert(page_ref(a)==1);
    assert(page_insert(boot_pgdir_va,a,va+PGSIZE,PTE_R|PTE_W)==0);
    assert(page_ref(a)==2);
    *(volatile uint64_t *)(va+PGSIZE)=0x12345678;
    assert(*(volatile uint64_t *)va==0x12345678);
    cprintf("PASS: actual virtual access and shared physical page\n");
    assert(page_insert(boot_pgdir_va,a,va,PTE_R|PTE_W)==0);
    assert(page_ref(a)==2);
    assert(page_insert(boot_pgdir_va,b,va,PTE_R|PTE_W)==0);
    assert(page_ref(a)==1 && page_ref(b)==1);
    assert(*(volatile uint64_t *)va==0x55667788);
    assert(*(volatile uint64_t *)(va+PGSIZE)==0x12345678);
    cprintf("PASS: same-page reinsertion and replacement after cached access\n");
    page_remove(boot_pgdir_va,va);
    assert(get_page(boot_pgdir_va,va,NULL)==NULL);
    assert(page_ref(b)==0);
    size_t count=nr_free_pages();
    page_remove(boot_pgdir_va,va);
    assert(nr_free_pages()==count);
    page_remove(boot_pgdir_va,va+PGSIZE);
    assert(page_ref(a)==0);
    assert(nr_free_pages()==baseline-2);
    // Test owns this isolated subtree; page_remove deliberately retains it.
    pde_t *middle=page2kva(pde2page(boot_pgdir_va[PDX1(va)]));
    struct Page *leaf=pde2page(middle[PDX0(va)]);
    struct Page *mid=pde2page(boot_pgdir_va[PDX1(va)]);
    middle[PDX0(va)]=0; boot_pgdir_va[PDX1(va)]=0;
    flush_tlb();
    set_page_ref(leaf,0);free_page(leaf);
    set_page_ref(mid,0);free_page(mid);
    assert(nr_free_pages()==baseline);
    cprintf("PASS: removal, idempotence and full test-resource reclamation\n");
    cprintf("RESULT: MEMBER_A_PAGE_QEMU_PASS\n");
    sbi_shutdown();
    while(1) {}
}
'''

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--root',required=True);args=ap.parse_args()
    root=Path(args.root).resolve();code=root/'code'
    logs=root/'report/logs';logs.mkdir(parents=True,exist_ok=True)
    hashes={str(p.relative_to(code)):hashlib.sha256(p.read_bytes()).hexdigest() for p in code.rglob('*') if p.is_file() and p.suffix in ('.c','.h','.S') and 'obj' not in p.parts and 'bin' not in p.parts}
    (logs/'member-a-page-qemu-source-hashes.json').write_text(json.dumps(hashes,indent=2),encoding='utf-8')
    with tempfile.TemporaryDirectory(prefix='lab4-member-a-') as tmp:
        work=Path(tmp)/'code'
        shutil.copytree(code,work,ignore=shutil.ignore_patterns('obj','bin','.git','build'))
        (work/'kern/init/init.c').write_text(FIXTURE,encoding='utf-8')
        build=subprocess.run(['make','-C',str(work)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        (logs/'member-a-page-qemu-build.log').write_text(build.stdout+f'\nEXIT_CODE: {build.returncode}\n',encoding='utf-8')
        print(build.stdout)
        if build.returncode: raise SystemExit('BUILD FAILED; see build log. Original sources unchanged.')
        command=['qemu-system-riscv64','-machine','virt','-nographic','-bios','default','-device',f'loader,file={work}/bin/ucore.img,addr=0x80200000']
        timed_out=False
        try:
            run=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=30)
            output=run.stdout;rc=run.returncode
        except subprocess.TimeoutExpired as e:
            output=e.stdout or b''
            if isinstance(output,bytes):output=output.decode(errors='replace')
            rc=None;timed_out=True
        print(output)
        passed=not timed_out and rc==0 and 'RESULT: MEMBER_A_PAGE_QEMU_PASS' in output and 'panic' not in output.lower()
        (logs/'member-a-page-qemu.log').write_text(output,encoding='utf-8')
        (logs/'member-a-page-qemu-result.json').write_text(json.dumps({'passed':passed,'timeout':timed_out,'exit_code':rc,'scope':'real PMM and page operations; test-only init; no process/scheduler integration'},indent=2),encoding='utf-8')
        print('ISOLATED_QEMU:', 'PASS' if passed else 'FAIL')
        if not passed: raise SystemExit(1)
    print('Production files unchanged. Not full-kernel grading; allocation-failure injection remains host-only.')

if __name__=='__main__':main()
