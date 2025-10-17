#include "core_allocator.hpp"
#include "stdio.hpp"
#include "pd.hpp"
#include "hazards.hpp"
#include "lapic.hpp"

size_t Core_allocator::alloc(size_t quantity, Cell *cell)
{
    size_t cores_allocated = 0;

    cell->cip->cores_new.clear();

    /* Always allocate from the reserved CPU cores, first. 
       This reduces overlaps with other allocating cells and improves 
       locality of the CPU cores allocated, as the reserved CPU cores
       are in topological proximity to each other. */

    Cpuset reserved = cell->cip->cores_reserved;
    reserved.subtract(cell->cip->cores_current);

    Cpuset::for_each_until(
        reserved,
        [&](long cpu)
        {
            if (_resources[cpu].occupy(cell))
                cores_allocated++;
        },
        [&]()
        { return cores_allocated >= quantity; });

    if (cores_allocated == quantity)
        return cores_allocated;

    /* If we still need more CPU cores, now try to borrow
       free CPU cores from other cells. */
    trace(TRACE_CORE_ALLOC, "Need to borrow %lu cores", quantity - cores_allocated);

    Cpuset::for_each_until(
        _idle_cpus,
        [&](long cpu)
        {
            if (_resources[cpu].occupy(cell))
                cores_allocated++;
        },
        [&]()
        { return cores_allocated >= quantity; });

    if (cores_allocated == quantity)
        return cores_allocated;

    /* If we need even more CPU cores, or were unable to get some by 
       allocating, see if we have hired out CPU cores to other cells and
       reclaim some until we get the desired amount of CPU cores or 
       there are no more cores we could reclaim. */

    Cpuset::for_each_until(
        reserved,
        [&](long cpu)
        {
            if (_resources[cpu].borrowed())
            {
                if (_resources[cpu].reclaim())
                    cores_allocated++;
            }
        },
        [&]()
        { return cores_allocated >= quantity; });

    return cores_allocated;
}

void Core_allocator::init() {
    _resources = static_cast<Cpu_resource*>(Buddy::alloc(2, Pd::kern.quota, Buddy::FILL_0));
    Pd::root.Space_mem::insert(Pd::kern.quota, reinterpret_cast<mword>(_resources), 2, Hpt::HPT_P | Hpt::HPT_NX | Hpt::HPT_W, Buddy::ptr_to_phys(_resources));
}

void Core_allocator::release(Cell *cell, unsigned int cpu)
{
    Cpu_resource *cpu_resource = &_resources[cpu];

    /* When a new cell is created, it may create a pool of worker threads for all 
     * available CPUs in its habitat. However, this cannot be done by allocating the 
     * necessary CPUs first, because the allocation already assumes the worker threads exists.
     * Thus, we get into the situation where we have worker threads on a non-allocated CPU
     * after starting a cell. The cell is required to call this function, but as we
     * do not have the CPU allocated we must block the workers instead of using the 
     * release method of the CPU resource object, as it will hold the wrong information.
     */
    if (cpu_resource->current() != cell && cell) {
        cell->cip->worker_info[cpu].yield_flag = 0;
        cell->block_workers_on(cpu);
        return;
    }
    cpu_resource->release();
}

void Core_allocator::return_core([[maybe_unused]] unsigned int cpu)
{
    _resources[cpu].return_core();
}

void Core_allocator::confer(Cell *new_owner) {
    Cpuset::for_each(
        new_owner->cip->reserved_cores(),
        [&](long cpu)
        {
            _resources[cpu].confer(new_owner);
        });
}

void Core_allocator::transfer([[maybe_unused]] Cell *new_owner, unsigned cpu) {

	if (!_idle_cpus.chk(cpu))
		return;
	
	if (_resources[cpu].borrowed()) {
        _resources[cpu].reclaim();
    } else {
        trace(TRACE_CORE_ALLOC, "Occupying CPU %u ", cpu);
        if (!_resources[cpu].occupy(new_owner)) {
            if (!_resources[cpu].borrowed())
                return;
            trace(TRACE_ERROR, "Failed to transfer CPU %u", cpu);
        }
    }
    new_owner->cip->cores_new.clr(cpu);
    new_owner->cip->cores_current.set(cpu);
}

bool Core_allocator::handle_hazard(unsigned cpu)
{
    switch (_resources[cpu].hazards) {
        case HZD_YIELD: {
            /* An overlap of a yield request with a voluntary yield occurred */
            _resources[cpu].hazards &= ~HZD_YIELD;

            Cell *borrower = _resources[cpu].current();

            /* First, wait for the CPU core to be fully released */
            Lapic::pause_loop_until(5, [&]()
                                    { return borrower && _resources[cpu].current() == borrower; }, 1000);

            Cell *owner = _resources[cpu].owner();

            /* CPU has been released by borrower, try to occupy it */
            if (EXPECT_TRUE(_resources[cpu].occupy(owner))) {
                if (borrower) /* it might be that borrower is a nullptr, so check it here */
                    borrower->cip->cores_current.clr(cpu); /* if the borrower is not null clear the bit for this CPU */
            } else {
                /* The occupation failed, due to another cell being faster. 
                   Hence, clear the reclamation flag for it and cancel the reclamation. */
                owner->cip->cores_reclaimed.clr(cpu);
                return false;
            }
            return true;
        }
        case HZD_RECLAIM: {
            _resources[cpu].hazards &= ~HZD_RECLAIM;
            _resources[cpu].return_core();
            return true;
        }
        default:
            return false;
    }
}

Core_allocator _core_alloc;