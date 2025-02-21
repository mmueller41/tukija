#include "tip.hpp"
#include "pd.hpp"


void Tip_node::print()
{
    Console::print("TIP: Dom %u - CPUs [ ", id);
    cpus.for_each([&](long cpu)
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

void *Tip_mem::operator new(size_t, Tip_node &node) { return node.alloc_mem(); }
void *Tip_dev::operator new(size_t, Tip_node &node) { return node.alloc_dev(); }