#include "core_allocator.hpp"
#include "stdio.hpp"
#include "pd.hpp"
#include "hazards.hpp"

bool Core_allocator::try_alloc(Cell *cell, long cpu)
{
    return _resources[cpu].occupy(cell, &cell->workers_for_core(static_cast<unsigned>(cpu)));
}

size_t Core_allocator::alloc(size_t quantity, Cell *cell)
{
    size_t cores_allocated = 0;
    //Cpuset free_affiliated_cores{0};


    cell->cip->cores_new.clear();

    for (unsigned cpu = 0; cpu < _cpu_count && cores_allocated < quantity; cpu++) {
        if (_resources[cpu].owner() == cell) {
            if (try_alloc(cell, cpu))
                cores_allocated++;
        }
    }
        /*free_affiliated_cores.merge(pre_reserved);
        free_affiliated_cores.subtract(cell->cip->cores_current);

        //trace(0, "Allocating");
        Cpuset::for_each_until(
            free_affiliated_cores,
            [&](long cpu)
            {
                if (try_alloc(cell, cpu))
                {
                    cores_allocated++;
                }
            },
            [&]() -> bool
            { return cores_allocated == quantity; });*/

    if (cores_allocated == quantity)
        return cores_allocated;

    //trace(0, "Need to borrow %lu cores", quantity - cores_allocated);
    for (unsigned cpu = 0; cpu < _cpu_count; cpu++)
    {
        //trace(0, "Trying to allocate CPU %u ", cpu);
        if (cores_allocated == quantity)
            break;
        if (try_alloc(cell, cpu))
        {
            cores_allocated++;
        }
    }

    if (cores_allocated == quantity)
        return cores_allocated;
    
    for (unsigned cpu = 0; cpu < _cpu_count && cores_allocated < quantity; cpu++)
    {
        Cell *owner = _resources[cpu].owner();
        if (owner == cell)
        {
            if (_resources[cpu].borrowed()) {
                if (owner == _resources[cpu].owner()) {
                    _resources[cpu].reclaim();
                    cores_allocated++;
                }
            }
        }
    }

    // trace(0, "Need to reclaim %lu cores", quantity - cores_allocated);
    /*Cpuset::for_each_until(
        pre_reserved,
        [&](long cpu)
        {
            if (_resources[cpu].borrowed())
            {
                _resources[cpu].reclaim();
                cores_allocated++;
            }
        },
        [&]() -> bool
        { return cores_allocated == quantity; });*/

    return cores_allocated;
}

void Core_allocator::init() {
    _resources = static_cast<Cpu_resource*>(Buddy::alloc(2, Pd::kern.quota, Buddy::FILL_0));
    Pd::root.Space_mem::insert(Pd::kern.quota, reinterpret_cast<mword>(_resources), 2, Hpt::HPT_P | Hpt::HPT_NX | Hpt::HPT_W, Buddy::ptr_to_phys(_resources));
}

void Core_allocator::release(unsigned int cpu)
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
    if (cpu_resource->current() != Pd::current->cell && Pd::current->cell) {
        Pd::current->cell->cip->worker_info[cpu].yield_flag = 0;
        Pd::current->cell->block_workers_on(cpu);
        return;
    }

    cpu_resource->release();
}

void Core_allocator::return_core(unsigned int cpu)
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
    if (_resources[cpu].borrowed()) {
        _resources[cpu].reclaim();
    } else {
        trace(0, "Occupying CPU %u ", cpu);
        if (!_resources[cpu].occupy(new_owner, &new_owner->workers_for_core(cpu))) {
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
            _resources[cpu].hazards &= ~HZD_YIELD;
            bool need_alloc = false;

            while (true) {
                if (_resources[cpu].current() == _resources[cpu].owner())
                    break;
                
                if (_resources[cpu].current() == nullptr) {
                    need_alloc = true;
                    break;
                }

                __builtin_ia32_pause();
            }


            Cell *owner = _resources[cpu].owner();

            if (need_alloc && !_resources[cpu].occupy(owner, &owner->workers_for_core(cpu))) {
                if (_resources[cpu].owner() == owner)
                    _resources[cpu].reclaim();
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