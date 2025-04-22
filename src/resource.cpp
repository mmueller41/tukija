#include "resource.hpp"
#include "core_allocator.hpp"
#include "sm.hpp"
#include "ec.hpp"
#include "cell.hpp"


bool Cpu_resource::occupy(Cell *pd)
{
    bool rc = Resource::occupy(pd);
    if (rc)
    {
        _core_alloc._idle_cpus.clr(_id);
        pd->cip->cores_new.set(_id);
    }
    return rc;
}

void Cpu_resource::release()
{

    Cell *curr = const_cast<Cell*>(_current);

    /* It can happen that while we are releasing this CPU core, its owner has
       filed a yield request. If this is the case, we need to return the core
       to its owner instead of releasing it. Otherwise, the owner may indefinetely
       wait for the CPU core's return.
     */
    bool need_to_return_core = (curr->cip->worker_info[_id].yield_flag == 1);

    if (need_to_return_core) {
        return_core();
        return;
    }

    /* No yield requests received, clear yield flag and release the CPU core */
    curr->cip->worker_info[_id].yield_flag = 0;
    Resource::release();
    _core_alloc._idle_cpus.set(_id);

    /* As Hoitaja can also release CPU cores of arbitrary cells, 
        block current EC only if the caller is *not* Hoitaja.
        Otherwise, Hoitaja would block and make long-term cell managment
        unavailable. */
    if (Pd::current->cell == curr)
        curr->block_workers_on(_id);

}

bool Cpu_resource::reclaim()
{
    Cell *borrower = current();
    _owner->cip->cores_reclaimed.set(_id);
    
    /* It's possible that the borrower has just released the CPU 
       when we get here. So, first, try to occupy the CPU core. 
       If this fails either the borrower still holds this CPU or
       another third cell has snatched it away under or nose */
    if (occupy(_owner)) {
        current()->wake_core(_id);
        return true;
    } else {
        borrower = current();
    }
    if (EXPECT_TRUE(borrower && borrower != _owner)) {
        /* The CPU has been borrowed, file a yield request */
        return borrower->return_core(_id);
    }
    /* The CPU has been released here. But, since it may have
       already been occupied before we could get it, we just give
       up here to avoid a cascade of occupation retries and 
       yield requests that may lead to an infinite loop. */
    _owner->cip->cores_reclaimed.clr(_id);
    return false;
}

void Cpu_resource::return_core()
{
    Cell *borrower = const_cast<Cell*>(_current);
    borrower->cip->worker_info[_id].yield_flag = 0;

    assert(_owner);
    __atomic_store_n(&_current, _owner, __ATOMIC_SEQ_CST);

    current()->wake_core(_id);

    /* Ensure that the we only block, if the cell executing this code
       actually holds this CPU core. That's because, upon destructrion, 
       Hoitaja may return CPU cores of a dying cell too. But since Hoitaja
       is not the occupier, we *must not* block its threads. Otherwise
       we would end up blocking Hoitaja and stalling cell management infinitely. */
    if (Pd::current->cell == borrower)
        borrower->block_workers_on(_id);
}