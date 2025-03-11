#include "core_allocator.hpp"
#include "stdio.hpp"
#include "pd.hpp"

size_t Core_allocator::alloc(size_t quantity, Cell *cell)
{
    size_t cores_allocated = 0;
    Cpuset free_affiliated_cores{0};

    free_affiliated_cores.merge(cell->cip->cores_reserved);
    free_affiliated_cores.subtract(cell->cip->cores_current);

    Cpuset::for_each_until(
        free_affiliated_cores,
        [&](long cpu)
        {
            if (_resources[cpu].occupy(cell, &cell->workers_for_core(static_cast<unsigned>(cpu))))
            {
                cores_allocated++;
            }
        },
        [&]() -> bool
        { return cores_allocated == quantity; });

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
    if (cpu_resource->current() != Pd::current->cell) {
        Pd::current->cell->block_workers_on(cpu);
        return;
    }

    cpu_resource->release();
}

void Core_allocator::return_core(unsigned int cpu)
{
    _resources[cpu].return_core();
}

Core_allocator _core_alloc;