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
        Cell *_current{}; // Current owner of this resource, if not the original owner.

    public:
        Resource(Type type, uint16 id) : _type(type), _id(id) {}
        inline bool occupy(Cell *pd) { return Atomic::cmp_swap(_current, static_cast<Cell*>(nullptr), pd); }
        inline void release() { Atomic::store(_current, static_cast<Cell *>(nullptr)); }
        inline void confer(Cell *new_owner) { _owner = new_owner; }
        inline Cell *owner() { return _owner; }
        inline bool borrowed() { return _owner != _current; }
        inline Cell *current() { return _current; }
};

class alignas(64) Cpu_resource : public Resource
{
    private:
        Queue<Worker> *_workers{nullptr}; // Used for pausing and waking the worker SC

    public:
        Cpu_resource(uint16 id) : Resource(Type::CPU, id) { trace(0, "Added CPU resource for CPU %u", _id); }

        bool occupy(Cell *pd, Queue<Worker> *workers);

        void release();

        void wake();

        void reclaim();

        void *operator new(size_t, void *p) { return p; }
};

extern Cpu_resource cpu_resources[NUM_CPU];