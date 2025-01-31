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

class Pd;
class Sm;

class alignas(64) Resource
{
    public:
        /* Types of resources, currently only CPUs (maybe contain accelerators in the future)*/
        enum Type
        {
            CPU = 0
        };

    private:
        Type _type;
        uint16 _id; // Identifier for this resource, e.g. a CPU ID.
        Pd *_pd {}; // Rightful owner of this resource, maybe null at first.
        Pd *_current{}; // Current owner of this resource, if not the original owner.

    public:
        Resource(Type type, uint16 id) : _type(type), _id(id) {}
        inline bool occupy(Pd *pd) { return Atomic::cmp_swap(_current, static_cast<Pd*>(nullptr), pd); }
        inline void release() { Atomic::store(_current, static_cast<Pd *>(nullptr)); }
        virtual void wake() = 0;
};

class alignas(64) Cpu_resource : public Resource
{
    private:
        Sm *_semaphore {}; // Used for pausing and waking the worker SC

    public:
        bool occupy(Pd *pd, Sm *sm);

        void release();

        void wake() override;
};