/* Habitat
 *
 * Copyright (C) 2023-2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 */

#include "habitat.hpp"
#include "bits.hpp"
#include "buddy.hpp"
#include "cell.hpp"
#include "compiler.hpp"
#include "initprio.hpp"
#include "memory.hpp"
#include "pd.hpp"
#include "resource.hpp"
#include "slab.hpp"
#include "stdio.hpp"

INIT_PRIORITY(PRIO_SLAB)
Slab_cache Habitat::cache (sizeof(Habitat), 64);
ALIGNED(32) Habitat Habitat::root(&Pd::root, 0, nullptr, Pd::root.cell, &Habitat::root);

Habitat::Habitat(Pd *own, mword sel, struct Habitat_info_page *haip_hva, Cell *hoitaja_ptr,
        Habitat *home)
	: Kobject(HABITAT, static_cast<Space_obj *>(own), sel, 0x6, free, pre_free), haip(haip_hva),
	  hoitaja(hoitaja_ptr), parent(home)
{}

Cell *Habitat::create_cell(Pd *pd, unsigned short prio, struct Cip *cip_hva)
{
    return new (*pd) Cell(this, prio, cip_hva);
}

void Habitat::destroy_cell(Cell *cell, Pd *pd)
{
	if (pd->cell != cell) return;

	Cell::destroy(cell, *pd);
}

size_t Habitat::alloc(Resource::Type type, unsigned int quantity, Cell *cell)
{
	switch (type) {
	case Resource::CPU:
		{
			size_t cores = _core_alloc.alloc(quantity, cell);

			if (!cores) return 0;

			cell->update_channel_params(cores);
			cell->wake_cores();
			return cores;
		}
	default:
		{
			trace(TRACE_ERROR, "%s: Resource type %u not supported, yet.", __func__, type);
			return 0;
		}
	}
}

void Habitat::release(Resource::Type type, Cell *cell, unsigned short id)
{
	switch (type) {
	case Resource::CPU:
		{
			_core_alloc.release(cell, id);
		}
	default:
		return;
	}
}

void Habitat::return_resource(Resource::Type type, unsigned short id)
{
	switch (type) {
	case Resource::CPU:
		{
			_core_alloc.return_core(id);
		}
	default:
		{   
            return;
		}
    }
}

bool Habitat::set_affinity(Cell *cell)
{
	Cip *cip = cell->cip;

	if (!_check_enclosure(cip->cores_reserved)) {
		trace(TRACE_ERROR, "Containment breached for cell %p in habitat %p", cell, this);
        return false;
	}

	_core_alloc.confer(cell);
	if (!cell->initialized) {
		trace(TRACE_CELL, "Initalizeing cell");
		_core_alloc.transfer(cell, cip->cores_reserved.first_cpu());
		cell->initialized = true;
	}

	return true;
}

void *Habitat_info_page::operator new(size_t, Pd &pd)
{
	size_t size = align_up(sizeof(Habitat_info_page), PAGE_SIZE);
	return Buddy::alloc(static_cast<unsigned short>(size / PAGE_SIZE), pd.quota, Buddy::NOFILL);
}

Paddr Habitat_info_page::map(Pd *parent, Paddr parent_va)
{
	mword haip_hva = reinterpret_cast<mword>(this);

	parent->Space_mem::insert(parent->quota, parent_va, 1, Hpt::HPT_U | Hpt::HPT_P | Hpt::HPT_W,
	                          Buddy::ptr_to_phys(reinterpret_cast<void *>(haip_hva)));

	return haip_hva;
}
