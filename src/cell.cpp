/* Cell
 * 
 * Copyright (C) 2023-2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 */
#include "cell.hpp"
#include "pd.hpp"
#include "sm.hpp"

bool Cell::wake_core(unsigned int core)
{
    bool woken = false;
    workers[core].for_each([&](auto &worker)
                           { worker.sm->up(); woken = true; });
    return woken;
}

void Cell::update(Cpuset &alloc)
{
    prefered_cores.merge(alloc);
}

unsigned Cell::yield_cores(Cpuset &cores, bool release)
{
    unsigned reclaimed = 0;
    Cpuset::for_each(cores, [&](long cpu)
                   {
        if (workers[cpu].head()) {
            /* Check whether the yield flag has already been set, if not set it */
            unsigned long expect = 0;
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

void Cell::add_cores(Cpuset &cores)
{
    Cpuset::for_each(cores, [&](long cpu)
                   {
        if (wake_core(static_cast<unsigned>(cpu))) {
            cip->cores_new.set(static_cast<unsigned>(cpu));
        } });
}

void Cell::return_core(unsigned int cpu)
{
    if (workers[cpu].head())
    {
        /* Check whether the yield flag has already been set, if not set it */
        unsigned long expect = 0;
        bool will_sleep = !__atomic_compare_exchange_n(&(cip->worker_info[cpu].yield_flag), &expect, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
        if (will_sleep)
            return;
    }
    else
    {
        /* TODO: directly return CPU core to core allocator */
    }
}

void Cell::block_workers_on(unsigned int core)
{
    workers[core].for_each([&](auto &worker)
                       {
        Sm *sm = worker.sm;
        sm->dn(false, 0, Ec::current, true); });
}

void *Cell::operator new(size_t, Pd &pd)
{
    /*size_t cell_size = align_up(sizeof(Cell), PAGE_SIZE);
    void *ptr = Buddy::alloc(static_cast<unsigned short>(cell_size / PAGE_SIZE), pd.quota, Buddy::NOFILL);
    pd.Space_mem::insert(pd.quota, reinterpret_cast<mword>(ptr), 1, Hpt::HPT_P | Hpt::HPT_W, Buddy::ptr_to_phys(ptr));*/
    return pd.cell_cache.alloc(pd.quota);
}

void *Worker::operator new(size_t, Pd &pd)
{
    return pd.worker_cache.alloc(pd.quota);
}