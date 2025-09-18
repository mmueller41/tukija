#include "cip.hpp"
#include "pd.hpp"
#include "space_mem.hpp"

void *Cip::operator new(size_t, Pd &pd)
{
    size_t size = align_up(sizeof(Cip), PAGE_SIZE);
    return Buddy::alloc(static_cast<unsigned short>(size / PAGE_SIZE), pd.quota, Buddy::NOFILL);
}

Paddr Cip::map(Pd *parent, Pd *self, Paddr parent_va)
{
    unsigned long *cip_hva = reinterpret_cast<unsigned long*>(this);
    
    self->Space_mem::insert(self->quota, (USER_ADDR - PAGE_H_SIZE - PAGE_T_SIZE - PAGE_SIZE - 8*PAGE_SIZE), 3, Hpt::HPT_U | Hpt::HPT_W | Hpt::HPT_P, Buddy::ptr_to_phys(reinterpret_cast<void *>(cip_hva)));
    
    parent->Space_mem::insert(parent->quota, reinterpret_cast<mword>(parent_va), 3, Hpt::HPT_U | Hpt::HPT_W | Hpt::HPT_P, Buddy::ptr_to_phys(reinterpret_cast<void*>(cip_hva)));

    mword phys = align_dn(reinterpret_cast<Paddr>(Buddy::ptr_to_phys(cip_hva)), PAGE_SIZE);
    mword virt = align_dn(USER_ADDR - PAGE_H_SIZE - PAGE_T_SIZE - PAGE_SIZE - 8*PAGE_SIZE, PAGE_SIZE);
    mword size = align_up(sizeof(Cip), PAGE_SIZE);

    for (unsigned long o; size; size -= 1UL << o, phys += 1UL << o, virt += 1UL << o) {
        parent->delegate<Space_mem>(self, phys >> PAGE_BITS, virt >> PAGE_BITS, (o = min(max_order(phys, size), max_order(virt, size))) - PAGE_BITS, 3);
		trace(TRACE_CELL, "Mapping CIP frame to VA %lx", virt);
    }

    trace(TRACE_CELL, "Mapped CIP at %lx", (USER_ADDR - PAGE_H_SIZE - PAGE_T_SIZE - PAGE_SIZE - 8 * PAGE_SIZE));
    return reinterpret_cast<Paddr>(cip_hva);
}