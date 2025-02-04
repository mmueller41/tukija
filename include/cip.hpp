/**
 * Cell Infopage 
 * 
 * Copyright (c) 2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 */

#pragma once 
#include "config.hpp"
#include "cpuset.hpp"

struct alignas(64) Cip_worker {
    volatile unsigned short yield_flag{0};
    unsigned short padding[3];
};

struct alignas(64) Cip
{
    alignas(64) struct Cip_worker worker_info[NUM_CPU];
    volatile unsigned short remainder{0};
    volatile unsigned short limit{0};
    
    /* Set of CPU cores currently allocated to this cell */
    Cpuset cores_current{0};
    
    /* Set of CPU cores recently added to this cell */
    Cpuset cores_new{0};

    Cip() = default;
};