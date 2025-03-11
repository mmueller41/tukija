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
        /*if (!sm)
        {
            trace(TRACE_ERROR, "Occuping a CPU core without valid semaphore is illegal.");
            return false;
        }*/
        _workers = workers;
        pd->cip->cores_new.set(_id);
    }
    return rc;
}

void Cpu_resource::release()
{

    _current->cip->cores_current.clr(_id);

    const_cast<Cell*>(_current)->block_workers_on(_id);

    Resource::release();
}

void Cpu_resource::wake()
{
    _workers->for_each([&](auto &worker)
                       { Sm *sm = worker.sm;
                         sm->up(); });
}

void Cpu_resource::reclaim()
{
    //trace(0, "Reclaiming CPU core %u", _id);
    current()->return_core(_id);
}

void Cpu_resource::return_core()
{
    _current->cip->cores_current.clr(_id);

    current()->block_workers_on(_id);

    if (__atomic_exchange_n(&_current, _owner, __ATOMIC_SEQ_CST) != Pd::current->cell) {
    }
    if (_owner) {
        _workers = &current()->workers_for_core(_id);

        wake();
    }
}

//alignas(64) Cpu_resource cpu_resources[NUM_CPU];