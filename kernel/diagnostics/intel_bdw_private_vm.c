#include <srvros/intel_bdw_private_vm.h>
#include <stddef.h>

_Static_assert(sizeof(struct bdw_private_vm_image)==BDW_PRIVATE_PAGES*4096, "PPGTT arena size");
_Static_assert(offsetof(struct bdw_private_vm_image,tables)==BDW_PRIVATE_PT*4096, "page table offset");

enum { PAGE_BYTES=4096, QUEUE_WORDS=64, BATCH_END=0x05000000 };
_Static_assert(BDW_GATE_WORDS_MAX+1<=QUEUE_WORDS && BDW_BARRIER_ENGINES*QUEUE_WORDS<=1024,
               "engine batches must fit in the command page");

/* The PTE for virtual page 1 names backing page 0. Both levels select
 * private PAT index 3 (PWT|PCD), and only completion pages permit GPU writes.
 * A global indirect-context pointer still requires a GGTT binding. */
static uint64_t table_entry(uint64_t base,unsigned level,unsigned i)
{
    const unsigned present=1, writable=2, pat3=24;
    if (level>1) return 0;
    if (level==1)
        return i==0 ? (base+BDW_PRIVATE_PT*PAGE_BYTES)|pat3|writable|present : 0;
    if (i==0 || i>BDW_PRIVATE_GATE+1) return 0;
    bool result=i>=2 && i<BDW_PRIVATE_GATE+1;
    return (base+(i-1)*PAGE_BYTES)|pat3|present|(result ? writable : 0);
}

static bool valid(const volatile struct bdw_private_vm_image *image,uint64_t base,uint64_t epoch)
{
    return image && !((uintptr_t)image & 4095) && base>=0x100000 && !(base & 4095) &&
           base<=(1ull<<32)-sizeof(*image) && epoch && epoch<=(UINT64_MAX>>8);
}

static unsigned engine_batch(unsigned engine,uint64_t epoch,uint32_t *packet)
{
    uint64_t result=(2+engine)*PAGE_BYTES, gate=(BDW_PRIVATE_GATE+1)*PAGE_BYTES;
    uint64_t marker=(epoch<<8)|(engine+1);
    unsigned words=intel_bdw_ppgtt_drain_and_hold(engine,result,gate,marker,packet);
    if (!words) return 0;
    packet[words]=BATCH_END;
    return words+1;
}

bool intel_bdw_private_vm_build(volatile struct bdw_private_vm_image *image,uint64_t base,uint64_t epoch)
{
    if (!valid(image,base,epoch)) return false;
    for (unsigned i=0;i<1024;i++) {
        image->commands[i]=0;
        image->gate[i]=0;
        for (unsigned e=0;e<BDW_BARRIER_ENGINES;e++) image->result[e][i]=0;
    }
    for (unsigned e=0;e<BDW_BARRIER_ENGINES;e++) {
        uint32_t packet[BDW_GATE_WORDS_MAX+1];
        unsigned n=engine_batch(e,epoch,packet);
        if (!n) return false;
        for (unsigned i=0;i<n;i++) image->commands[e*QUEUE_WORDS+i]=packet[i];
    }
    for (unsigned level=0;level<5;level++)
        for (unsigned i=0;i<512;i++) image->tables[level][i]=table_entry(base,level,i);
    return true;
}

bool intel_bdw_private_vm_verify(volatile const struct bdw_private_vm_image *image,uint64_t base,uint64_t epoch)
{
    if (!valid(image,base,epoch)) return false;
    for (unsigned e=0;e<BDW_BARRIER_ENGINES;e++) {
        uint32_t packet[BDW_GATE_WORDS_MAX+1];
        unsigned n=engine_batch(e,epoch,packet);
        if (!n) return false;
        for (unsigned i=0;i<QUEUE_WORDS;i++)
            if (image->commands[e*QUEUE_WORDS+i]!=(i<n ? packet[i] : 0)) return false;
        for (unsigned i=0;i<1024;i++) if (image->result[e][i]) return false;
    }
    for (unsigned i=BDW_BARRIER_ENGINES*QUEUE_WORDS;i<1024;i++) if (image->commands[i]) return false;
    for (unsigned i=0;i<1024;i++) if (image->gate[i]) return false;
    for (unsigned level=0;level<5;level++)
        for (unsigned i=0;i<512;i++) if (image->tables[level][i]!=table_entry(base,level,i)) return false;
    return true;
}
