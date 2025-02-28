#include "core_allocator.hpp"
#include "stdio.hpp"
#include "pd.hpp"

size_t Core_allocator::alloc(size_t quantity, Cell *cell)
{
    size_t cores_allocated = 0;
    Cpuset free_affiliated_cores{0};

    free_affiliated_cores.merge(cell->cip->cores_reserved);
    free_affiliated_cores.subtract(cell->cip->cores_current);

    Cpuset::for_each(free_affiliated_cores,
        [&](long cpu)
        {
            if (quantity == 0)
                return;
            if (_resources[cpu].occupy(cell, &cell->workers_for_core(static_cast<unsigned>(cpu))))
            {
                cores_allocated++;
                quantity--;
            }
        });

    Console::print("Allocating %lu cores for Cell %p", quantity, cell);
    return cores_allocated;
}

void Core_allocator::init() {
    _resources = static_cast<Cpu_resource*>(Buddy::alloc(2, Pd::kern.quota, Buddy::FILL_0));
    Pd::root.Space_mem::insert(Pd::kern.quota, reinterpret_cast<mword>(_resources), 2, Hpt::HPT_P | Hpt::HPT_NX | Hpt::HPT_W, Buddy::ptr_to_phys(_resources));
}

void Core_allocator::release(Cell *cell, unsigned int cpu)
{
    Cpu_resource *cpu_resource = &_resources[cpu];

    cpu_resource->release();
}

Core_allocator _core_alloc;