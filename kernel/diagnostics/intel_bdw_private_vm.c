#include <srvros/intel_bdw_private_vm.h>
#include <stddef.h>

_Static_assert(sizeof(struct bdw_private_vm_image)==BDW_PRIVATE_PAGES*4096, "PPGTT arena size");
_Static_assert(offsetof(struct bdw_private_vm_image,tables)==BDW_PRIVATE_PT*4096, "page table offset");

enum { PAGE_BYTES=4096, QUEUE_WORDS=BDW_PRIVATE_QUEUE_BYTES/4, BATCH_END=0x05000000 };
_Static_assert(BDW_GATE_WORDS_MAX+1<=QUEUE_WORDS && BDW_BARRIER_ENGINES*QUEUE_WORDS<=1024,
               "engine batches must fit in the command page");

/* The PTE for virtual page 1 names backing page 0. Both levels select
 * private PAT index 3 (PWT|PCD), and only completion pages permit GPU writes.
 * A global indirect-context pointer still requires a GGTT binding. */
static uint64_t table_entry(uint64_t base,unsigned level,unsigned i,bool marker_only)
{
    const unsigned present=1, writable=2, pat3=24;
    if (level>1) return 0;
    if (level==1)
        return i==0 ? (base+BDW_PRIVATE_PT*PAGE_BYTES)|pat3|writable|present : 0;
    if (i==0 || i>BDW_PRIVATE_GATE+1) return 0;
    if (marker_only && i!=1 && i!=2+BDW_BARRIER_BCS) return 0;
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

/* RCS entry for a shared private VM. The CPU opens RENDER_START after
 * observing all five fresh drain markers. Copy the sibling markers from
 * their actual GPU addresses before issuing the final post-sync flush. */
/* Three barriers, two four-word waits, two five-word copies per peer, and END. */
enum { GATHER_WORDS = 3*BCS_FLUSH_DWORDS + 2*4 + 2*5*(BDW_BARRIER_ENGINES-1) + 1 };
_Static_assert(GATHER_WORDS*4 <= BDW_PRIVATE_GATHER_BYTES &&
               BDW_PRIVATE_GATHER_BATCH + BDW_PRIVATE_GATHER_BYTES <= PAGE_BYTES,
               "gather batch must fit the command page");

enum { MARKER_WORDS = BDW_PRIVATE_MARKER_BYTES / 4 };
_Static_assert(MARKER_WORDS == 5 + BCS_FLUSH_DWORDS + 1 &&
               BDW_PRIVATE_MARKER_BATCH + BDW_PRIVATE_MARKER_BYTES <= PAGE_BYTES,
               "standalone BCS request must fit the command page");

static bool first_marker(uint64_t epoch, uint32_t packet[MARKER_WORDS])
{
    const uint32_t result = (2 + BDW_BARRIER_BCS) * PAGE_BYTES;
    const uint64_t marker = (epoch << 8) | (BDW_BARRIER_BCS + 1);
    /* MI_STORE_DATA_IMM QWord in the private address space. Write the
     * complement on a different cache line before the post-sync marker. */
    packet[0] = (0x20u << 23) | (1u << 21) | 3u;
    packet[1] = result;
    packet[2] = 0;
    packet[3] = (uint32_t)~marker;
    packet[4] = (uint32_t)(~marker >> 32);
    packet[MARKER_WORDS - 1] = BATCH_END;
    return intel_bdw_ppgtt_barrier(BDW_BARRIER_BCS, result + BDW_QUEUE_DRAIN, marker, packet + 5);
}

static bool render_gather(uint64_t epoch, uint32_t packet[GATHER_WORDS])
{
    const uint32_t render = 2*PAGE_BYTES, gate = (BDW_PRIVATE_GATE+1)*PAGE_BYTES;
    const uint64_t marker = (epoch<<8) | (BDW_BARRIER_RCS+1);
    if (!intel_bdw_ppgtt_barrier(BDW_BARRIER_RCS,render+BDW_QUEUE_DRAIN,marker,packet) ||
        !intel_bdw_ppgtt_wait_equal(gate+BDW_GATE_RENDER_START,1,packet+BCS_FLUSH_DWORDS))
        return false;
    unsigned used = BCS_FLUSH_DWORDS + 4;
    for (unsigned e=1; e<BDW_BARRIER_ENGINES; e++) {
        uint32_t dst = render+BDW_PRIVATE_GATHER_RESULT+(e-1)*8;
        uint32_t src = (2+e)*PAGE_BYTES+BDW_QUEUE_DRAIN;
        for (unsigned offset=0; offset<8; offset+=4) {
            /* Broadwell PRM 2a pp. 817–818, MI_COPY_MEM_MEM, both addresses PPGTT. */
            packet[used++] = (0x2eu<<23) | 3u;
            packet[used++] = dst+offset;
            packet[used++] = 0;
            packet[used++] = src+offset;
            packet[used++] = 0;
        }
    }
    if (!intel_bdw_ppgtt_barrier(BDW_BARRIER_RCS,render+BDW_QUEUE_FINAL_FLUSH,marker,packet+used) ||
        !intel_bdw_ppgtt_wait_equal(gate,1,packet+used+BCS_FLUSH_DWORDS) ||
        !intel_bdw_ppgtt_barrier(BDW_BARRIER_RCS,render+BDW_QUEUE_RESUMED,~marker,
                                 packet+used+BCS_FLUSH_DWORDS+4))
        return false;
    packet[GATHER_WORDS-1] = BATCH_END;
    return true;
}

static void clear_pages(volatile struct bdw_private_vm_image *image)
{
    for (unsigned i=0; i<1024; i++) {
        image->commands[i]=0;
        image->gate[i]=0;
        for (unsigned e=0; e<BDW_BARRIER_ENGINES; e++) image->result[e][i]=0;
    }
}

static void build_tables(volatile struct bdw_private_vm_image *image,uint64_t base,bool marker_only)
{
    for (unsigned level=0; level<5; level++)
        for (unsigned i=0; i<512; i++) image->tables[level][i]=table_entry(base,level,i,marker_only);
}

static bool verify_pages(const volatile struct bdw_private_vm_image *image,uint64_t base,bool marker_only)
{
    for (unsigned i=0; i<1024; i++) {
        if (image->gate[i]) return false;
        for (unsigned e=0; e<BDW_BARRIER_ENGINES; e++) if (image->result[e][i]) return false;
    }
    for (unsigned level=0; level<5; level++)
        for (unsigned i=0; i<512; i++)
            if (image->tables[level][i]!=table_entry(base,level,i,marker_only)) return false;
    return true;
}

bool intel_bdw_marker_vm_build(volatile struct bdw_private_vm_image *image,uint64_t base,uint64_t epoch)
{
    uint32_t marker[MARKER_WORDS];
    if (!valid(image,base,epoch) || !first_marker(epoch,marker)) return false;
    clear_pages(image);
    for (unsigned i=0; i<MARKER_WORDS; i++) image->commands[BDW_PRIVATE_MARKER_BATCH/4+i]=marker[i];
    build_tables(image,base,true);
    return true;
}

bool intel_bdw_marker_vm_verify(const volatile struct bdw_private_vm_image *image,uint64_t base,uint64_t epoch)
{
    uint32_t marker[MARKER_WORDS];
    if (!valid(image,base,epoch) || !first_marker(epoch,marker)) return false;
    for (unsigned i=0; i<1024; i++) {
        unsigned index=i-BDW_PRIVATE_MARKER_BATCH/4;
        if (image->commands[i]!=(index<MARKER_WORDS ? marker[index] : 0)) return false;
    }
    return verify_pages(image,base,true);
}

bool intel_bdw_private_vm_build(volatile struct bdw_private_vm_image *image,uint64_t base,uint64_t epoch)
{
    if (!valid(image,base,epoch)) return false;
    clear_pages(image);
    for (unsigned e=0;e<BDW_BARRIER_ENGINES;e++) {
        uint32_t packet[BDW_GATE_WORDS_MAX+1];
        unsigned n=engine_batch(e,epoch,packet);
        if (!n) return false;
        for (unsigned i=0;i<n;i++) image->commands[e*QUEUE_WORDS+i]=packet[i];
    }
    uint32_t gather[GATHER_WORDS];
    if (!render_gather(epoch,gather)) return false;
    for (unsigned i=0;i<GATHER_WORDS;i++) image->commands[BDW_PRIVATE_GATHER_BATCH/4+i]=gather[i];
    build_tables(image,base,false);
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
    }
    uint32_t gather[GATHER_WORDS];
    if (!render_gather(epoch,gather)) return false;
    for (unsigned i=0;i<1024-BDW_PRIVATE_GATHER_BATCH/4;i++)
        if (image->commands[BDW_PRIVATE_GATHER_BATCH/4+i] != (i<GATHER_WORDS ? gather[i] : 0)) return false;
    return verify_pages(image,base,false);
}
