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

#include "acpi_srat.hpp"
#include "tip.hpp"
#include "config.hpp"
#include "stdio.hpp"

void Acpi_table_srat::parse() const
{
    parse(Affinity::Type::LAPIC, &parse_lapic);
    parse(Affinity::Type::MAS, &parse_memory);
    parse(Affinity::Type::X2APIC, &parse_x2apic);
}

void Acpi_table_srat::parse(Affinity::Type type, void (*handler)(Affinity const *)) const
{
    for (Affinity const *ptr = srat; ptr < reinterpret_cast<Affinity *>(reinterpret_cast<mword>(this) + length); ptr = reinterpret_cast<Affinity*>(reinterpret_cast<mword>(ptr) + ptr->length))
        if (ptr->type == type)
            (*handler)(ptr);
}

void Acpi_table_srat::parse_lapic(Affinity const *ptr)
{
    Lapic const *p = static_cast<Lapic const *>(ptr);
    Tip *tip = Tip::tip();

    if (p->flags & 1) {
        uint32 numa_id = (p->domain_lo |
                          static_cast<uint32>(p->domain_hi[0]) << 8 |
                          static_cast<uint32>(p->domain_hi[1]) << 16 |
                          static_cast<uint32>(p->domain_hi[2]) << 24);


        Tip::check_and_add(*tip, numa_id);

        trace(TRACE_ACPI, "SRAT: CPU %u - Dom %u", p->apic_id, numa_id);
        Tip::add_cpu(*tip, p->apic_id, numa_id);
    }
}

void Acpi_table_srat::parse_memory(Affinity const *ptr)
{
    Memory const *mem = static_cast<Memory const *>(ptr);
    Tip *tip = Tip::tip();

    if (!mem->flag_enabled)
        return;

    Paddr base = mem->base;
    size_t size = mem->size;
    uint32 numa_id = mem->domain;

    if (mem->flag_nvm) {
        trace(TRACE_ACPI, "SRAT: Non-DRAM %#018lx-%018lx Dom %u", base, (base+size), numa_id);
        return;
    }

    trace(TRACE_ACPI, "SRAT: %#018lx-%018lx Dom %u", base, (base + size), numa_id);

    Tip::check_and_add(*tip, numa_id);

    Tip::add_mem(*tip, numa_id, base, size);
}

void Acpi_table_srat::parse_x2apic(Affinity const *ptr)
{
    X2apic const *apic = static_cast<X2apic const *>(ptr);

    uint32 numa_id = apic->domain;

    Tip *tip = Tip::tip();
    
    trace(TRACE_ACPI, "SRAT: CPU %u - Dom %u", apic->apic_id, numa_id);

    Tip::check_and_add(*tip, numa_id);

    Tip::add_cpu(*tip, apic->apic_id, numa_id);
}

void Acpi_table_srat::parse_gias(Affinity const *ptr)
{
    Generic_initiator const *gias = static_cast<Generic_initiator const *>(ptr);

    uint32 numa_id = gias->domain;
    Tip *tip = Tip::tip();

    Tip::check_and_add(*tip, numa_id);

    if (!gias->flag_enabled)
        return;

    switch (gias->dev_handle_type)
    {
    case Generic_initiator::dev_handle_type::ACPI /* constant-expression */:
        {
            trace(TRACE_ACPI, "SRAT: ACPI device %llx:%x Dom %u", gias->acpi_handle.hid, gias->acpi_handle.uid, numa_id);
            tip->add_acpi_dev(*tip, numa_id, gias->acpi_handle.hid, gias->acpi_handle.uid);
            break;
        }
    case Generic_initiator::dev_handle_type::PCI:
        {
            uint16 bus = gias->pci_handle.bdf >> 8;
            uint16 dev = gias->pci_handle.bdf >> 3 & 0xf;
            uint16 fun = gias->pci_handle.bdf & 0x7;
            uint16 seg = gias->pci_handle.segment;
            trace(TRACE_ACPI, "SRAT: PCI device %x:%x:%x.%x Dom %u", seg, bus, dev, fun, numa_id);
            tip->add_pci_dev(*tip, numa_id, gias->pci_handle.segment, gias->pci_handle.bdf);
            break;
        }

    default:
        break;
    }
}