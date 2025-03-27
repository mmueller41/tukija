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
    curr->cip->worker_info[_id].yield_flag = 0;
    Resource::release();

    /* As Hoitaja can also release CPU cores of arbitrary cells, 
        block current EC only if the caller is *not* Hoitaja.
        Otherwise, Hoitaja would block and make long-term cell managment
        unavailable. */
    if (Pd::current->cell == curr)
        curr->block_workers_on(_id);

}

void Cpu_resource::wake()
{
    _owner->cip->cores_current.set(_id);
    _workers->for_each([&](auto &worker)
                       { Sm *sm = worker.sm;
        sm->submit(); });
}

void Cpu_resource::reclaim()
{
    // trace(0, "Reclaiming CPU core %u", _id);
    Cell *borrower;
    _owner->cip->cores_reclaimed.set(_id);
    /* It might happen that the borrower from which to reclaim the CPU core
       has terminated in the meanwhile. If that's the case, we can just 
       transfer the CPU core to its owner and activate its workers. */
    if (EXPECT_FALSE(!(borrower = current()))) {
        __atomic_store_n(&_current, _owner, __ATOMIC_SEQ_CST);
        _workers = &_owner->workers_for_core(_id);
        wake();
        return;
    }
    if (EXPECT_TRUE(borrower != _owner))
        borrower->return_core(_id);
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

    if (Pd::current->cell == borrower)
        borrower->block_workers_on(_id);
}

//alignas(64) Cpu_resource cpu_resources[NUM_CPU];