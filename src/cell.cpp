/* Cell
 * 
 * Copyright (C) 2023-2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 */
#include "cell.hpp"
#include "pd.hpp"
#include "sm.hpp"
#include "core_allocator.hpp"

bool Cell::wake_core(unsigned int core)
{
    bool woken = false;
    workers[core].for_each([&](auto &worker)
                           { 
                                    worker.sm->submit();
                                    woken = true; });
    return woken;
}

void Cell::wake_cores()
{
    add_cores(cip->cores_new);
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
            cip->cores_current.set(static_cast<unsigned>(cpu));
        }
        else
        {
            trace(TRACE_ERROR, "No worker on CPU %ld", cpu);
        } });
}

void Cell::return_core(unsigned int cpu)
{
    if (workers[cpu].head())
    {
        /* Check whether the yield flag has already been set, if not set it */
        unsigned long expect = 0;


        bool will_sleep = !__atomic_compare_exchange_n(&(cip->worker_info[cpu].yield_flag), &expect, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
        if (will_sleep) {
            _core_alloc.set_hazard(cpu, HZD_YIELD);
            _core_alloc.handle_hazard(cpu);
        }
    }
    else
    {
        /* TODO: directly return CPU core to core allocator */
        assert(workers[cpu].head());
    }
}

void Cell::block_workers_on(unsigned int core)
{
    workers[core].for_each([&](auto &worker)
                       {
        Sm *sm = worker.sm;
        Ec::current->cont = Ec::sys_finish<Sys_regs::SUCCESS, true>;
        sm->dn(true, 0, worker.sc->ec, true); });
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

void Cell::destroy(Cell *obj, Pd &pd)
{
    obj->~Cell();
    pd.cell_cache.free(obj, pd.quota);
}

Cell::~Cell()
{ 
    /* At last, free the CPU cores that were used by this cell */

    Cpuset::for_each(cip->cores_current,
                     [&](long cpu)
                     {
                         _core_alloc.release(static_cast<unsigned>(cpu));
                     });
}