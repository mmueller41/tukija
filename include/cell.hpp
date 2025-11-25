/*
* Cell - Elastic Resource Container
*
* Copyright (c) 2023-2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
*/

#pragma once

#include "cip.hpp"
#include "queue.hpp"

class Habitat;
class Sc;
class Sm;
class Pd;

struct Worker
{
    Sc *sc;
    Sm *sm;
    Worker *prev;
    Worker *next;

    void *operator new(size_t, Pd & pd);
};

class Cell
{
    private:
        Spinlock workers_locks[NUM_CPU];
        Queue<Worker> workers[NUM_CPU];
        Cpuset prefered_cores{0};
		unsigned      prio;

        /* Prohibit copying (for -Weffc++) */
        Cell(const Cell &);
        Cell &operator=(const Cell &);

    public:
        bool to_be_destroyed{false};
        struct Cip *cip{nullptr};
		Habitat      *home; /* The habitat this cell resides in. */
        bool initialized{false};

        /*** Constructors ***/
        
        /**
         * Creates a new cell
         * @param _prio - the priority of the cell
         * @param pre_alloc - the set of prefered CPU cores for allocation
         * @param new_cip - pointer to the CIP for this cell
        */
		Cell(Habitat *habitat, unsigned _prio, struct Cip *new_cip)
			: prio(_prio), cip(new_cip), home(habitat)
        {
        }

        /*** Destructor ***/
        ~Cell();

        /*** CPU Resource functions ***/

        void lock_workers_queue(unsigned int core) {
            workers_locks[core].lock();
        }

        void unlock_workers_queue(unsigned int core) {
            workers_locks[core].unlock();
        }

        Queue<Worker> &workers_for_core(unsigned int core) {
            return workers[core];
        }

        /**
         * Add CPU cores to this cell
         * @param cores - the CPU core IDs to add
         */
        void add_cores(Cpuset &cores);

        /**
         * Yield a specific CPU 
         * @param core - the Id of the core to yield
         * @param clear_flag - whether or not to clear a pending yield flag in the CIP
         */
        void yield_core(unsigned int core, bool clear_flag = true)
        {
            cip->cores_current.clr(core);
            if (clear_flag)
                cip->worker_info[core].yield_flag = 0;
        }

        /**
         * Wake a cpu core
         * @param core - the CPU core to wake up
         */
        void wake_core(unsigned int core);

        /**
         * @brief Wake all CPU cores that were recently allocated but not woken up yet
         * 
         */
        void wake_cores();

        /**
         * Update prefered resource allocation
         * @param alloc - the new prefered allocation
         * @param offset - offset inside the CPU set 
         */
        void update(Cpuset &alloc);

        /** Reclaim a set of CPU cores from this cell
         * @param cores - the IDs of the CPU cores to yield
         * @param release - whether the yielded CPU cores shall be marked as free CPU cores
         * @return the number of reclaimed CPU cores
         */
        unsigned yield_cores(Cpuset &cores, bool release = false);

        /**
         * @brief Request the return of a CPU core to its owner
         * 
         * @param core - the ID of the CPU core to return to its owner
         */
        bool return_core(unsigned int core);

        /**
         * @brief Block all workers registered for a given CPU core
         * 
         * @param core - to block all workers of this cell on
         */
        void block_workers_on(unsigned int core);

        static void *operator new(size_t, Pd &pd);

        static void destroy(Cell *obj, Pd &pd);

		void update_channel_params(size_t cores)
		{
			if (!cores || !cip->channel_info.count)
                return;
			cip->channel_info.limit = static_cast<unsigned short>(cip->channel_info.count / cores);
			cip->channel_info.remainder = static_cast<unsigned short>(cip->channel_info.count % cores);
		}
};
