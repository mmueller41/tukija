#include "tip.hpp"
#include "pd.hpp"


void Tip_node::print()
{
    Console::print("TIP: Dom %u - CPUs [ ", id);
    Cpuset::for_each(cpus, [&](long cpu)
                    { Console::print("%2ld ", cpu); });
    Console::print("] ");
    for_each_mem([&](Tip_mem &mem)
                      { Console::print("| %#018lx - %018lx", mem.start, mem.end); });
}

void *Tip_node::alloc_mem()
{
    if (num_mem_descr > 32) {
        return nullptr;
    } else
        return &memory[num_mem_descr++];
}

void *Tip_node::alloc_dev()
{
    if (num_devs > 32) {
        return nullptr;
    }
    return &devices[num_devs++];
}

void Tip::delegate_to_userspace(Pd &pd)
{
    mword tip_addr = USER_ADDR - PAGE_H_SIZE - PAGE_SIZE - PAGE_T_SIZE;
    mword phys = align_dn(reinterpret_cast<Paddr>(&FRAME_T), PAGE_SIZE);
    mword virt = align_dn (tip_addr, PAGE_SIZE);
    mword size = align_up (PAGE_T_SIZE, PAGE_SIZE);

    for (unsigned long o; size; size -= 1UL << o, phys += 1UL << o, virt += 1UL << o) {
        pd.delegate<Space_mem>(&Pd::kern, phys >> PAGE_BITS, virt >> PAGE_BITS, (o = min (max_order (phys, size), max_order (virt, size))) - PAGE_BITS, 1);
        trace(TRACE_TIP, "Mapping TIP frame to VA %lx", virt);
    }

    trace(TRACE_TIP, "Mapped TIP to userspace GVA: %lx", tip_addr);
}

void *Tip_mem::operator new(size_t, Tip_node &node) { return node.alloc_mem(); }
void *Tip_dev::operator new(size_t, Tip_node &node) { return node.alloc_dev(); }