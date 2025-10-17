/*
 * Resource Allocator
 *
 * Copyright (C) 2023-2025 Michael Müller <michael.mueller@uos.org>, Osnabrück University
 *
 * This file is part of the Tukija microhypervisor, based on NOVA.
 *
 * Tukija and NOVA are free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * Tukija is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#include "config.hpp"
#include "cell.hpp"
#include "resource.hpp"
#include "cpuset.hpp"
#include "buddy.hpp"
#include "stdio.hpp"
#include "bit_alloc.hpp"
#include "spinlock.hpp"

class Pd;

class Core_allocator
{
    private:
        alignas(64) Cpu_resource *_resources{nullptr}; /* array of CPU resources */
        alignas(64) unsigned _cpu_count{0}; /* number of CPU resources that are available */
        alignas(64) Cpuset _idle_cpus{0};

        /**
         * @brief try to allocate a single CPU for a cell
         * 
         * @param cell - the cell that tries to allocate 
         * @param cpu - the CPU core that shall be allocated
         * @return true - the allocation has been successful
         * @return false - the allocation failed due to another cell holding the CPU core
         */
        bool try_alloc(Cell *cell, long cpu);


    public:
        friend class Cpu_resource;
        Core_allocator() = default;

        /**
         * @brief Allocate a number of CPU cores for a cell
         * 
         * @param quantity - the number of CPU cores to allocate
         * @param cell - pointer to the allocating cell
         * @return size_t - number of actually allocated cores, may differ from
         *         quantity due to availability of CPU cores
         */
        size_t alloc(size_t quantity, Cell *cell);

        /**
         * @brief Voluntarily yield a CPU core, putting all workers of 
         *        the given cell on it to sleep.
         * 
         * @param cell - the cell that yields the CPU
         * @param cpu  - the CPU that shall be yielded
         */
        void release(Cell *cell, unsigned int cpu);

        /**
         * @brief Return a CPU core to its owner
         * 
         * @param cpu - the CPU core to return
         */
        void return_core(unsigned int cpu);

        /**
         * @brief Allocate the management data structures for core allocation
         * 
         */
        void init();

        /**
         * @brief Add a CPU to the allocator (only called by startup code)
         * 
         * @param cpu - logical ID of the CPU to add
         */
        void add_cpu([[maybe_unused]] unsigned int cpu) {
            new (&_resources[cpu]) Cpu_resource(static_cast<uint16>(cpu));

			Atomic::add<unsigned>(_cpu_count, 1);
            _idle_cpus.set(cpu);
        }

        /**
         * @brief Confer ownership of a set of cpus to a new owner that are
         * specified in the new owner's CIP
         * 
         * @param new_owner - the cell the CPU cores shall be confered to
         */
        void confer(Cell *new_owner);

        /**
         * @brief Transfer ownership and possession of a CPU core to a
         *        cell. This will automatically make new_owner not just
         *        the owner of cpu but allocate the CPU for new_owner too.
         * 
         * @param new_owner - the cell that the CPU core shall be transfered to
         * @param cpu - the CPU core to transfer
         */
        void transfer(Cell *new_owner, unsigned cpu);

        /**
         * @brief Notifiy the allocator that a hazardous situation has occurred
         * 
         * @param cpu - CPU core ID for which the hazard occurred
         * @param hazard  - the type of hazard
         */
        void set_hazard(unsigned cpu, unsigned hazard) {
            _resources[cpu].hazards |= hazard;
        }

        /**
         * @brief Handles overlaps between the withdrawal of a CPU core and 
         * the withdrawee voluntarily yielding it
         * 
         * @param cpu - CPU ID which its owner reclaims
         * @return true - the yield request was successfully filed
         * @return false - another cell has allocated the core before the yield request could be filed, thus,
         *                 no reclamation was filed
         */
        bool handle_hazard(unsigned cpu);
};

extern Core_allocator _core_alloc;