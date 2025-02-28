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
    Resource::release();

    _current->cip->cores_current.clr(_id);

    _workers->for_each([&](auto &worker)
                       {
        Sm *sm = worker.sm;
        sm->dn(false, 0, Ec::current, true); });
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
    _current->return_core(_id);
}


//alignas(64) Cpu_resource cpu_resources[NUM_CPU];