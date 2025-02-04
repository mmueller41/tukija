/* Cell
 * 
 * Copyright (C) 2023-2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 */
#include "cell.hpp"
#include "pd.hpp"
#include "sm.hpp"

bool Cell::wake_core(unsigned int core)
{
    if (workers[core].sc && workers[core].sm) {
        workers[core].sm->up();
        return true;
    }
    return false;
}

void Cell::update(Cpuset &alloc)
{
    prefered_cores.merge(alloc);
}

unsigned Cell::yield_cores(Cpuset &cores, bool release)
{
    unsigned reclaimed = 0;
    cores.for_each([&](long cpu)
                   {
        if (workers[cpu].sc) {
            /* Check whether the yield flag has already been set, if not set it */
            short expect = 0;
            bool will_sleep = !__atomic_compare_exchange_n(&(cip->worker_info[cpu].yield_flag), &expect, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
            if (will_sleep)
                return;
            reclaimed++;
        } else {
            /* TODO: directly return CPU core to core allocator */
        }
        
        if (release) {
            /* TODO: free the core at the core allocator */
        }

        reclaimed++; });
    return reclaimed;
}

void *Cell::operator new(size_t, Pd &pd)
{
    return pd.cell_cache.alloc(pd.quota);
}
