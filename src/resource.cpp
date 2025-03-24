#include "resource.hpp"
#include "core_allocator.hpp"
#include "sm.hpp"
#include "ec.hpp"
#include "cell.hpp"


bool Cpu_resource::occupy(Cell *pd, Queue<Worker> *workers)
{
    bool rc = Resource::occupy(pd);
    if (rc)
    {
        _workers = workers;
        pd->cip->cores_new.set(_id);
    }
    return rc;
}

void Cpu_resource::release()
{
    Cell *curr = const_cast<Cell*>(_current);
    curr->cip->cores_current.clr(_id);
    Resource::release();

    curr->block_workers_on(_id);

}

void Cpu_resource::wake()
{
    _workers->for_each([&](auto &worker)
                       { Sm *sm = worker.sm;
        sm->up(); });
}

void Cpu_resource::reclaim()
{
    trace(0, "Reclaiming CPU core %u", _id);
    _owner->cip->cores_reclaimed.set(_id);
    current()->return_core(_id);
}

void Cpu_resource::return_core()
{
    Cell *borrower = const_cast<Cell*>(_current);
    borrower->cip->cores_current.clr(_id);
    borrower->cip->worker_info[_id].yield_flag = 0;

    __atomic_store_n(&_current, _owner, __ATOMIC_SEQ_CST);
    if (_owner)
    {
        _workers = &_owner->workers_for_core(_id);

        wake();
    }

    borrower->block_workers_on(_id);
}

//alignas(64) Cpu_resource cpu_resources[NUM_CPU];