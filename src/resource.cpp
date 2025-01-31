#include "resource.hpp"
#include "sm.hpp"
#include "ec.hpp"

bool Cpu_resource::occupy(Pd *pd, Sm *sm)
{
    bool rc = Resource::occupy(pd);
    if (rc)
    {
        if (!sm)
        {
            trace(TRACE_ERROR, "Occuping a CPU core without valid semaphore is illegal.");
            return false;
        }
        _semaphore = sm;
    }
    return rc;
}

void Cpu_resource::release()
{
    Sm *sm = _semaphore;
    Resource::release();
    sm->dn(false, 0, Ec::current, true);
}

void Cpu_resource::wake()
{
    _semaphore->up();
}