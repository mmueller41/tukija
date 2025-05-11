/**
 * Cell Infopage 
 * 
 * Copyright (c) 2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 */

#pragma once 
#include "config.hpp"
#include "cpuset.hpp"
#include "console.hpp"
#include "spinlock.hpp"

class Pd;
struct Cip_worker
{
    unsigned long yield_flag{0};
    unsigned long padding[3];
};

struct Channel_info {
    volatile unsigned short remainder{0};
    volatile unsigned short limit{0};
    unsigned int count{0};
};

struct alignas(64) Cip
{
    alignas(64) struct Cip_worker worker_info[NUM_CPU];
    struct Channel_info channel_info {};

    Spinlock lock{};

    /* Set of CPU cores currently allocated to this cell */
    alignas(64) Cpuset cores_current{0};

    /* Set of CPU cores reserved for this cell*/
    Cpuset cores_reserved{0};

    /* Set of CPU cores recently added to this cell */
    Cpuset cores_new{0};

    /* Set of CPU cores for which a return request was filed */
    Cpuset cores_reclaimed{0};

    /* Padding for CIP area solely used by user-space */
    unsigned char padding[1048];

    Cip() = default;

    Cpuset &reserved_cores() { return cores_reserved; }

    Paddr map(Pd * parent, Pd *self, Paddr parent_va);

    void print()
    {
        Console::print("------<CPU resource info>------\n");
        Console::print("# reserved CPU cores: %u\n", cores_reserved.count());
        Console::print("Reserved CPU cores: ");
        Cpuset::for_each(cores_reserved, [&](long cpu)
                                { Console::print("%ld ", cpu); });
        Console::print("\n");
        Console::print("# currently allocated CPU cores: %u\n", cores_current.count());
        Console::print("Allocated CPU cores: ");
        Cpuset::for_each(cores_current, [&](long cpu)
                                { Console::print("%ld ", cpu); });
        Console::print("\n");
        Console::print("------<Worker information>-------\n");
        Console::print("# channels available: %u\n", channel_info.count);
        Console::print("Pending yield requests for workers: ");
        for (int i = 0; i < NUM_CPU; i++) {
            if (worker_info[i].yield_flag)
                Console::print("%u ", i);
        }
        Console::print("\n");
    }

    void *operator new(size_t, Pd &pd);
};