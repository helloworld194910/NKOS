#!/usr/bin/env python3
"""Contract tests of actual page_insert/remove bodies with simulated dependencies."""
from pathlib import Path
import argparse, subprocess, hashlib, json

PRE = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uintptr_t pte_t;
typedef uintptr_t pde_t;
struct Page { int ref; int freed; };
static struct Page pages[16];
static pte_t entries[4], root[512];
static int mode, flushes, frees, creates;
#define PTE_V 1
#define PTE_R 2
#define PTE_W 4
#define E_NO_MEM 4
#define PDX1(a) (((a)>>30)&511)
static int page_ref_inc(struct Page *p) { return ++p->ref; }
static int page_ref_dec(struct Page *p) { assert(p->ref>0); return --p->ref; }
static void set_page_ref(struct Page *p,int n) { p->ref=n; }
static uintptr_t page2ppn(struct Page *p) { return p-pages; }
static pte_t pte_create(uintptr_t n,int perm) { return (n<<10)|perm; }
static struct Page *pte2page(pte_t p) { return &pages[p>>10]; }
static struct Page *pde2page(pde_t p) { return pte2page(p); }
static void free_page(struct Page *p) { assert(p->ref==0 && !p->freed); p->freed=1; ++frees; }
static void tlb_invalidate(pde_t *p,uintptr_t a) { (void)p;(void)a;++flushes; }
static pte_t *get_pte(pde_t *p,uintptr_t a,int create) {
 if(create) ++creates;
 if(mode==1) return NULL;
 if(mode==2 && create) { p[PDX1(a)]=pte_create(10,PTE_V);pages[10].ref=1;return NULL; }
 return &entries[(a>>12)&3];
}
void page_remove(pde_t *,uintptr_t);
'''
POST = r'''
static void reset(void) { memset(pages,0,sizeof(pages)); memset(entries,0,sizeof(entries)); memset(root,0,sizeof(root));mode=flushes=frees=creates=0; }
int main(void) {
 reset();assert(page_insert(root,&pages[1],0x123,PTE_R)==0);assert(pages[1].ref==1);assert(entries[0]==pte_create(1,PTE_R|PTE_V));assert(flushes==1);
 puts("PASS 1: new mapping and non-aligned address");
 assert(page_insert(root,&pages[1],0,PTE_R|PTE_W)==0);assert(pages[1].ref==1 && frees==0);assert(entries[0]==pte_create(1,PTE_R|PTE_W|PTE_V));assert(flushes==2);
 puts("PASS 2: same-page permission update");
 assert(page_insert(root,&pages[1],4096,PTE_R)==0);assert(pages[1].ref==2);assert(page_insert(root,&pages[2],0,PTE_R)==0);assert(pages[1].ref==1 && !pages[1].freed && pages[2].ref==1);assert(pte2page(entries[0])==&pages[2]);
 puts("PASS 3: replace shared page without premature free");
 page_remove(root,4096);assert(pages[1].ref==0 && pages[1].freed && entries[1]==0);int f=frees,t=flushes,c=creates;page_remove(root,4096);assert(frees==f && flushes==t && creates==c);
 puts("PASS 4: last reference and repeated removal");
 assert(page_insert(root,&pages[3],0,PTE_R)==0);assert(pages[2].freed && pages[2].ref==0 && pages[3].ref==1);
 puts("PASS 5: replacement releases old last reference");
 reset();mode=1;page_remove(root,0);assert(frees==0 && flushes==0 && creates==0);assert(page_insert(root,&pages[1],0,PTE_R)==-E_NO_MEM);assert(pages[1].ref==0 && root[0]==0 && frees==0);
 puts("PASS 6: absent table and first allocation failure");
 reset();mode=2;root[0]=0x100;assert(page_insert(root,&pages[1],0,PTE_R)==-E_NO_MEM);assert(root[0]==0x100 && pages[10].freed && pages[10].ref==0 && pages[1].ref==0 && frees==1 && flushes==1);
 puts("PASS 7: partial table allocation rollback");
 reset();mode=1;root[0]=pte_create(10,PTE_V);pages[10].ref=1;assert(page_insert(root,&pages[1],0,PTE_R)==-E_NO_MEM);assert(root[0]==pte_create(10,PTE_V) && pages[10].ref==1 && frees==0 && pages[1].ref==0);
 puts("PASS 8: existing intermediate table preserved on failure");
 puts("RESULT: 8/8 host contract scenarios passed"); return 0;
}
'''

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--root',required=True);ap.add_argument('--cc',default='gcc');args=ap.parse_args()
    root=Path(args.root).resolve();src=root/'code/kern/mm/pmm.c'
    s=src.read_text(encoding='utf-8')
    start=s.index('int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)')
    end=s.index('// invalidate a TLB entry',start)
    body=s[start:end]
    build=root/'code/obj/member-a-page-host';build.mkdir(parents=True,exist_ok=True)
    unit=build/'actual_page_ops_test.c';unit.write_text(PRE+body+POST,encoding='utf-8')
    binary=build/('page-ops.exe' if __import__('os').name=='nt' else 'page-ops')
    cmd=[args.cc,'-std=c99','-Wall','-Wextra','-Werror','-O0',str(unit),'-o',str(binary)]
    logs=[]
    for command in [cmd,[str(binary)]]:
        r=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        logs.append('COMMAND: '+' '.join(command)+'\n'+r.stdout+f'EXIT_CODE: {r.returncode}\n');print(logs[-1])
        if r.returncode: break
    dest=root/'report/logs/member-a-page-ops-host.log';dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text('SOURCE_SHA256: '+hashlib.sha256(src.read_bytes()).hexdigest()+'\n'+''.join(logs),encoding='utf-8')
    print('LIMIT: real page-operation bodies; mocked get_pte/allocator/TLB. Not QEMU or real page-table traversal.')
    if r.returncode: raise SystemExit(r.returncode)

if __name__=='__main__': main()
