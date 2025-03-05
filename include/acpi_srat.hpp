/*
 * ACPI System Resource Affinity Table
 *
 * Copyright (C) 2022-2025 Michael Müller, Osnabrück University
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#include "acpi_table.hpp"

#pragma pack(1)

class Acpi_table_srat : public Acpi_table
{
    private:
        struct Affinity {
            uint8 type;
            uint8 length;

            enum Type
            {
                LAPIC = 0,
                MAS = 1,
                X2APIC = 2,
                GICCAS = 3,
                GICITS = 4,
                GIAS = 5
            };
        };

        struct Lapic : public Affinity {
            uint8 domain_lo;
            uint8 apic_id;
            uint32 enabled : 1,
                reserved: 31;
            uint8 local_sapic_eid;
            uint8 domain_hi[3];
            uint32 clock_domain;
        };

        struct Memory : public Affinity {
            uint32 domain;
            uint16 reserved1;
            uint64 base;
            uint64 size;
            uint32 reserved2;
            uint32 flag_enabled : 1,
                flag_hotplug : 1,
                flag_nvm : 1, : 29;
            uint64 reserved;
        };

        struct X2apic : public Affinity {
            uint16 reserved;
            uint32 domain;
            uint32 apic_id;
            uint32 enabled : 1,
                flags : 31;
            uint32 clock_domain;
            uint32 reserved2;
        };

        struct Generic_initiator : public Affinity {
            uint8 reserved;
            uint8 dev_handle_type;
            uint32 domain;

            union {
                struct {
                    uint64 hid;
                    uint32 uid;
                    uint32 reserved;
                } acpi_handle;
                struct {
                    uint16 segment;
                    uint16 bdf;
                    uint32 reserved[3];
                } pci_handle;
            };
            uint32 flag_enabled : 1,
                flag_arch_transact : 1, : 30;

            enum dev_handle_type
            {
                ACPI = 0,
                PCI = 1
            };
        };

        uint32 reserved;
        uint64 reserved2;
        Affinity srat[];

        INIT static void parse_lapic(Affinity const *);
        INIT static void parse_memory(Affinity const *);
        INIT static void parse_x2apic(Affinity const *);
        INIT static void parse_gias(Affinity const *);
        INIT void parse(Affinity::Type, void (*)(Affinity const *)) const;

    public:
        INIT void parse() const;
};

#pragma pack()