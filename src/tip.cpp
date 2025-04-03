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
    for (int f = 0, v=0; f < PAGE_T_SIZE/PAGE_SIZE; f++, v++) {
        pd.delegate<Space_mem>(&Pd::kern, (reinterpret_cast<Paddr>(&FRAME_T) >> PAGE_BITS) + f, (USER_ADDR - PAGE_H_SIZE - PAGE_SIZE - v * PAGE_SIZE) >> PAGE_BITS, 0, 1);
    }
}

void *Tip_mem::operator new(size_t, Tip_node &node) { return node.alloc_mem(); }
void *Tip_dev::operator new(size_t, Tip_node &node) { return node.alloc_dev(); }