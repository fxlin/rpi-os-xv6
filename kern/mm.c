#include "mm.h"

#include "types.h"
#include "string.h"
#include "mmu.h"
#include "memlayout.h"
#include "spinlock.h"
#include "console.h"
#include "peripherals/mbox.h"

#ifdef DEBUG

#define MAX_PAGES 1000
static void *alloc_ptr[MAX_PAGES];

#endif

extern char end[];

struct freelist {
    void *next;
    void *start, *end;
} freelist;

static struct spinlock memlock;

void mm_test();

/*
 * Allocate one 4096-byte page of physical memory.
 * Returns a pointer that the kernel can use.
 * Returns 0 if the memory cannot be allocated.
 */ 
static void *
freelist_alloc(struct freelist *f)
{
    void *p = f->next;
    if (p)
        f->next = *(void **)p;
    return p;
}

/*
 * Free the page of physical memory pointed at by v.
 */
static void
freelist_free(struct freelist *f, void *v)
{
    *(void **)v = f->next;
    f->next = v;
}

void
free_range(void *start, void *end)
{
    acquire(&memlock);
    int cnt = 0;
    for (void *p = start; p + PGSIZE <= end; p += PGSIZE, cnt ++) 
        freelist_free(&freelist, p);
    cprintf("- free_range: 0x%p ~ 0x%p, %d pages\n", start, end, cnt);
    release(&memlock);

    // mm_test();
    // mm_test();
}

void
mm_init()
{
    int phystop = mbox_get_arm_memory();
    free_range(ROUNDUP((void *)end, PGSIZE), P2V(phystop));
}

/*
 * Allocate a page of physical memory.
 * Returns 0 if failed else a pointer.
 * Corrupt the page by filling non-zero value in it for debugging.
 */
void *
kalloc()
{
    acquire(&memlock);
    void *p = freelist_alloc(&freelist);
    if (p) {
        memset(p, 0xAC, PGSIZE);
#ifdef DEBUG
        int i;
        for (i = 0; i < MAX_PAGES; i++) {
            if (!alloc_ptr[i]) {
                alloc_ptr[i] = p;
                break;
            }
        }
        if (i == MAX_PAGES) {
            panic("mm: no more space for debug. ");
        }
#endif
    }
    else cprintf("- kalloc null\n");
    release(&memlock);
    return p;
}

/*
 * Free the physical memory pointed at by v.
 */
void
kfree(void *va)
{
    acquire(&memlock);
    memset(va, 0xAF, PGSIZE); // For debug.
    freelist_free(&freelist, va);
#ifdef DEBUG
    int i;
    for (i = 0; i < MAX_PAGES; i++) {
        if (alloc_ptr[i] == va) {
            alloc_ptr[i] = 0;
            break;
        }
    }
    if (i == MAX_PAGES) {
        panic("kfree: not allocated. ");
    }
#endif
    release(&memlock);
}


void
mm_dump()
{
#ifdef DEBUG
    int cnt = 0;
    for (int i = 0; i < MAX_PAGES; i++) {
        if (alloc_ptr[i]) cnt++;
    }
    cprintf("allocated: %d pages\n", cnt);
#endif
}

void
mm_test()
{
#ifdef DEBUG
    static void *p[0x100000000/PGSIZE];
    int i;
    for (i = 0; (p[i] = kalloc()); i++) {
        memset(p[i], 0xFF, PGSIZE);
        if (i % 10000 == 0) cprintf("0x%p\n", p[i]);
    }
    while (i--)
        kfree(p[i]);
#endif
}
