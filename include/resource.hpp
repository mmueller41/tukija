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

class Pd;
class Worker;

class Resource
{
    public:
        /* Types of resources, currently only CPUs (maybe contain accelerators in the future)*/
        enum Type
        {
            CPU = 0
        };

    protected:
        Type _type;
        uint16 _id; // Identifier for this resource, e.g. a CPU ID.
        Cell *_owner {}; // Rightful owner of this resource, maybe null at first.
        Cell volatile *_current{}; // Current owner of this resource, if not the original owner.

    public:
        Resource(Type type, uint16 id) : _type(type), _id(id) {}
        inline bool occupy(Cell *pd) {
            Cell *expect{nullptr};
            return __atomic_compare_exchange_n(&_current, &expect, pd, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED);
        }
        inline void release() { __atomic_store_n(&_current, nullptr, __ATOMIC_SEQ_CST); }
        inline void confer(Cell *new_owner) { _owner = new_owner; }
        inline Cell *owner() { return _owner; }
        inline bool borrowed() { return _owner != _current && _current != nullptr; }
        inline Cell *current() { return const_cast<Cell*>(_current); }
};

class alignas(64) Cpu_resource : public Resource
{
    private:
        Queue<Worker> *_workers{nullptr}; // Used for pausing and waking the worker SC

    public:
        Cpu_resource(uint16 id) : Resource(Type::CPU, id) { }

        bool occupy(Cell *pd, Queue<Worker> *workers);

        void release();

        void wake();

        void reclaim();

        void return_core();

        void *operator new(size_t, void *p) { return p; }
};

extern Cpu_resource cpu_resources[NUM_CPU];