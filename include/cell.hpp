/*
* Cell - Elastic Resource Container
*
* Copyright (c) 2023-2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
*/

#pragma once

#include "cip.hpp"

class Sc;
class Sm;
class Pd;

struct Worker
{
    Sc *sc;
    Sm *sm;
};

class Cell
{
    private:
        struct Worker workers[NUM_CPU];
        Cpuset prefered_cores{0};
        unsigned prio;

    public:
        struct Cip *cip{nullptr};

        /*** Constructors ***/
        
        /**
         * Creates a new cell
         * @param _prio - the priority of the cell
         * @param pre_alloc - the set of prefered CPU cores for allocation
         * @param new_cip - pointer to the CIP for this cell
        */
        Cell(unsigned _prio, struct Cip *new_cip) : prio(_prio) , cip(new_cip)
        {
        }

        /*** CPU Resource functions ***/
        
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
         * @return true, if a worker was registered on the core; otherwise return false
         */
        bool wake_core(unsigned int core);

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

        static void *operator new(size_t, Pd &pd);
};