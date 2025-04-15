/**
 * Representation of a (hardware) resource. 
 * 
 * \author Michael Müller, Osnabrück University
 * 
 * Copyright (c) 2025 Michael Müller, Osnabrück University
 */

#pragma once

#include "atomic.hpp"
#include "types.hpp"
#include "cell.hpp"
#include "slab.hpp"
#include "stdio.hpp"
#include "queue.hpp"
#include "spinlock.hpp"

class Pd;
class Worker;

/**
 * @brief Representation of an arbitrary atomically allocatable resource
 * 
 */
class Resource
{
    public:
        /* Types of resources, currently only CPUs (maybe contain accelerators in the future)*/
        enum Type
        {
            CPU = 0
        };
        unsigned int hazards{0};


    protected:
        Type _type; /* type of this resource */
        uint16 _id; // Identifier for this resource, e.g. a CPU ID.
        Cell *_owner {}; // Rightful owner of this resource, maybe null at first.
        alignas(64) Cell volatile *_current{}; // Current owner of this resource, if not the original owner.

    public:
        /**
         * @brief Construct a new Resource object with given type and id
         * 
         * @param type - type of the new resource
         * @param id - the new resource's ID (used for allocating, withdrawal and yielding)
         */
        Resource(Type type, uint16 id) : _type(type), _id(id) {}

        /**
         * @brief Occupy this resource
         * 
         * @param pd - cell that claims this resource
         * @return true - the resource was handed over to the claiming cell
         * @return false - the resource has already been occupied by another cell
         */
        inline bool occupy(Cell *pd) {
            Cell *expect{nullptr};
            return __atomic_compare_exchange_n(&_current, &expect, pd, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
        }

        /**
         * @brief Release this resource
         * 
         */
        ALWAYS_INLINE
        inline void release() { __atomic_store_n(&_current, nullptr, __ATOMIC_SEQ_CST); }
        
        /**
         * @brief Confer ownership of this resource to a cell
         * 
         * @param new_owner - new owner of this resource
         */
        ALWAYS_INLINE
        inline void confer(Cell *new_owner) { __atomic_store_n(&_owner, new_owner, __ATOMIC_SEQ_CST); }

        /**
         * @brief Get the owner of this resource
         * 
         * @return pointer to the cell that owns this resource
         */
        ALWAYS_INLINE
        inline Cell *owner() { return _owner; }
        
        /**
         * @brief Check whether this resource has been lent to another cell
         * 
         * @return true - if the resource was barrrowed by a cell other than the owner
         * @return false - the cell is occupied by its owner
         */
        ALWAYS_INLINE
        inline bool borrowed() { return _owner != _current && _current != nullptr; }
        
        /**
         * @brief Return the current occupier of this resource
         * 
         * @return pointer to the cell that currently occupies (uses) this resource
         */
        ALWAYS_INLINE
        inline Cell *current() { return const_cast<Cell*>(_current); }
};

/**
 * @brief Representation of allocatable CPU cores
 * 
 */
class alignas(64) Cpu_resource : public Resource
{
    public:
        /**
         * @brief Construct a new Cpu_resource with given ID
         * 
         * @param id - the logical ID of the CPU core represented by this CPU resource object
         */
        Cpu_resource(uint16 id) : Resource(Type::CPU, id) { }

        /**
         * @brief Occupy a CPU resource for usage
         * 
         * @param pd - the cell demanding this CPU core
         * @return true - the claimant has received this CPU core
         * @return false - another cell has already occupied this CPU core
         */
        bool occupy(Cell *pd);

        /**
         * @brief Release the CPU resource blocking all workers of the current holder running on it
         * 
         */
        void release();

        /**
         * @brief Reclaim the CPU resource for its owner
         * 
         * @return true - the reclamation has been successfully filed to a borrower or 
         *                this resource could be occupied by the owner (because it had been released before)
         * @return false - the resource could not be reclaimed as the borrower changed while trying 
         *                 to send yield request
         */
        bool reclaim();

        /**
         * @brief Return this CPU core to its owner and block all
         *        workers of its borrower that run on this CPU core.
         * 
         */
        void return_core();

        /**
         * @brief Placement new operator to create a new resource object in the core allocator's
         *        resource management array.
         *        IMPORTANT: this does *not* allocate any memory, the memory for this resource
         *                   must be allocated before and a pointer to it passed to this operator
         * 
         * @param p - Pointer to the slot in the resource array
         * @return void* - the very same pointer
         */
        void *operator new(size_t, void *p) { return p; }
};