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

class Pd;

class Core_allocator
{
    private:
        alignas(64) Cpu_resource *_resources{nullptr};

        bool try_alloc(Cell *cell, long cpu);

    public:
        Core_allocator() = default;

        size_t alloc(size_t quantity, Cell *cell);
        void release(unsigned int cpu);

        void return_core(unsigned int cpu);

        void init();

        void add_cpu([[maybe_unused]] unsigned int cpu) {
            new (&_resources[cpu]) Cpu_resource(static_cast<uint16>(cpu));
        }
};

extern Core_allocator _core_alloc;