/*
 * Topology Information Pages (TIP)
 *
 * Copyright (C) 2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 * 
 * This file is part of the Tukija microhypervisor.
 *
 * Tukija is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Tukija is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#include "extern.hpp"
#include "types.hpp"
#include "config.hpp"
#include "cpuset.hpp"
#include "stdio.hpp"

struct Tip_node;

class Pd;
struct Tip_mem
{
    Paddr start;
    Paddr end;

    Tip_mem() = default;

    Tip_mem(Paddr mstart, Paddr mend) :  start(mstart), end(mend) {}

    void *operator new(size_t, Tip_node &);
};

struct Tip_dev 
{
    enum dev_type
    {
        ACPI = 0,
        PCI = 1
    };

    dev_type type;

    union
    {
        struct {
            uint64 hid;
            uint32 uid;
        } acpi_handle;
        struct {
            uint16 segment;
            uint16 bdf;
        } pci_handle;
    };

    Tip_dev() = default;

    Tip_dev(uint64 hid, uint32 uid) : type(dev_type::ACPI) {
        this->acpi_handle.hid = hid;
        this->acpi_handle.uid = uid;
    }

    Tip_dev(uint16 seg, uint16 pbdf) :  type(dev_type::PCI) {
        this->pci_handle.segment = seg;
        this->pci_handle.bdf = pbdf;
    }

    static void *operator new(size_t, Tip_node &);
};

struct Tip_node
{
    uint32 id;
    uint8 num_mem_descr{0};
    uint8 num_devs{0};
    Tip_mem memory[32];
    Tip_dev devices[32];
    Cpuset cpus{0};
    void print();
    void *alloc_mem();
    void *alloc_dev();

    template <typename T>
    void for_each_mem(T const fn) {
        for (uint8 i = 0; i < num_mem_descr; i++)
            fn(memory[i]);
    }
};

class Tip
{
    
    public:
        uint16_t length{8};
        uint32 cpu_to_node[NUM_CPU];
        Tip_node nodes[];

        ALWAYS_INLINE
        static inline Tip *tip()
        {
            return reinterpret_cast<Tip *>(&PAGE_T);
        }

        static void add_node(Tip_node *node, uint32 id) {
            node->id = id;
            tip()->length += sizeof(Tip_node);
        }

        template <typename T>
        static void on_node(Tip &tip, T const fn, uint32 id)
        {
            mword const node_cnt = (reinterpret_cast<mword>(&tip.nodes) + tip.length - reinterpret_cast<mword>(tip.nodes)) / sizeof(Tip_node);

            Tip_node *n = tip.nodes;

            for (unsigned i = 0; i < node_cnt; i++) {
                n = tip.nodes + i;

                if (n->id == id)
                    break;
            }
            
            if (n->id != id)
                return;

            fn(*n);
        }
        
        template <typename T>
        static void for_each_node(Tip &tip, T const fn)
        {
            mword const node_cnt = (reinterpret_cast<mword>(&tip.nodes) + tip.length - reinterpret_cast<mword>(tip.nodes)) / sizeof(Tip_node);

            Tip_node *n = tip.nodes;

            for (unsigned i = 0; i < node_cnt; i++) {
                n = tip.nodes + i;

                fn(*n);
            }
        }

        static void add_cpu(Tip &tip, unsigned cpu_id, uint32 node_id)
        {
            Tip::on_node(
                tip, [&](Tip_node &node)
                { node.cpus.set(cpu_id); },
                node_id);
            tip.cpu_to_node[cpu_id] = node_id;
        }

        static void add_mem(Tip &tip, uint32 node_id, Paddr base, Paddr size)
        {
            Tip::on_node(
                tip, [&](Tip_node &node)
                { new (node) Tip_mem(base, base + size-1); },
                node_id);
        }

        static void add_pci_dev(Tip &tip, uint32 node_id, uint16 segment, uint16 bdf){
            Tip::on_node(
                tip, [&](Tip_node &node)
                { new (node) Tip_dev(segment, bdf); }, node_id);
        }

        static void add_acpi_dev(Tip &tip, uint32 node_id, uint64 hid, uint32 uid) {
            Tip::on_node(
                tip, [&](Tip_node &node)
                { new (node) Tip_dev(hid, uid); },
                node_id);
        }

        ALWAYS_INLINE
        inline static bool has_node(Tip &tip, uint32 node_id)
        {
            bool has = false;
            if (tip.length == 0)
                return has;
            
            mword const node_cnt = (reinterpret_cast<mword>(&tip.nodes) + tip.length - reinterpret_cast<mword>(tip.nodes)) / sizeof(Tip_node);

            Tip_node *n = tip.nodes;

            for (unsigned i = 0; i < node_cnt; i++)
            {
                n = &tip.nodes[i];

                if (n->id == node_id) {
                    has=true;
                    break;
                }
            }

            return has;
        }

        ALWAYS_INLINE
        inline static void check_and_add(Tip &tip, uint32 node_id)
        {
            if (!has_node(tip, node_id)) {
                trace(TRACE_ACPI, "SRAT: New Domain %u ", node_id);
                Tip_node *node = reinterpret_cast<Tip_node *>(reinterpret_cast<mword>(&tip.nodes) + tip.length);
                Tip::add_node(node, node_id);
            }
        }

        ALWAYS_INLINE
        inline void print()
        {
            Console::print("Size of CPUset structure: %ld \n", sizeof(Cpuset));
            mword const node_cnt = (reinterpret_cast<mword>(&nodes) + length - reinterpret_cast<mword>(nodes)) / sizeof(Tip_node);

            Tip_node *n = nodes;

            for (unsigned i = 0; i < node_cnt; i++)
            {
                n = &nodes[i];

                n->print();
                Console::print("\n");
            }
        }

        ALWAYS_INLINE
        inline static Tip_node &lookup(cpu_t cpu)
        {
            uint32 node_id = Tip::tip()->cpu_to_node[cpu];
            return Tip::tip()->nodes[node_id];
        }

        void delegate_to_userspace(Pd &pd);
};
