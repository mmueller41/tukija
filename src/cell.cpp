/* Cell
 * 
 * Copyright (C) 2023-2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 */
#include "cell.hpp"
#include "pd.hpp"
#include "sm.hpp"
#include "core_allocator.hpp"

void Cell::wake_core(unsigned int core)
{
    workers[core].for_each([&](auto &worker)
                           { 
                                    this->cip->cores_current.set(core);
                                    worker.sm->up(); });
}

void Cell::wake_cores()
{
    add_cores(cip->cores_new);
}

void Cell::update(Cpuset &alloc)
{
    prefered_cores.merge(alloc);
}


void Cell::add_cores(Cpuset &cores)
{
    Cpuset::for_each(cores, [&](long cpu)
                     { wake_core(static_cast<unsigned>(cpu)); });
}

bool Cell::return_core(unsigned int cpu)
{
    if (workers[cpu].head())
    {
        unsigned long expect = 0;

        /* Try to set the yield flag in the borrower's CIP. If this fails, the borrower has already indicated the voluntary yield of 
           the CPU core. */
        bool will_sleep = !__atomic_compare_exchange_n(&(cip->worker_info[cpu].yield_flag), &expect, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
        if (will_sleep) {
            /* */
            _core_alloc.set_hazard(cpu, HZD_YIELD);
            return _core_alloc.handle_hazard(cpu);
        }
        return true;
    }
    else
    {
        _core_alloc.return_core(cpu);
        return true;
    }
}

void Cell::block_workers_on(unsigned int core)
{
    if (EXPECT_FALSE(to_be_destroyed))
        return;

    cip->cores_current.clr(core);
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
    Buddy::free(reinterpret_cast<Paddr>(obj->cip), pd.quota);
    obj->~Cell();
    pd.cell_cache.free(obj, pd.quota);
}

Cell::~Cell()
{ 
    /* At last, free the CPU cores that were used by this cell */

    to_be_destroyed = true;

    Cpuset::for_each(cip->cores_current,
                     [&](long cpu)
                     {
                         if (cip->worker_info[cpu].yield_flag == 1)
                         {
                             trace(TRACE_CELL, "Found pending yield request for CPU %lu.", cpu);
                             _core_alloc.return_core(static_cast<unsigned>(cpu));
                         }
                         else
                             _core_alloc.release(this, static_cast<unsigned>(cpu));
                         //assert(cip->worker_info[cpu].yield_flag != 1);
                     });
}