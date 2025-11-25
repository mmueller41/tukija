/* Habitat
 *
 * Copyright (C) 2023-2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 */

#include "habitat.hpp"
#include "cell.hpp"
#include "compiler.hpp"
#include "initprio.hpp"
#include "pd.hpp"
#include "resource.hpp"
#include "slab.hpp"
#include "stdio.hpp"

INIT_PRIORITY(PRIO_SLAB)
ALIGNED(32) Habitat Habitat::root(&Pd::root, 0, nullptr, Pd::root.cell, &Habitat::root);

Habitat::Habitat(Pd *own, mword sel, struct Habitat_info_page *haip_hva, Cell *hoitaja_ptr,
        Habitat *home)
	: Kobject(HABITAT, static_cast<Space_obj *>(own), sel, 0x6, free, pre_free), haip(haip_hva),
	  hoitaja(hoitaja_ptr), parent(home)
{
	if (this != &Habitat::root) init();
}

void Habitat::init()
{
	core_alloc.init();
	Cpuset::for_each(haip->reserved_cores, [&](long cpu) {
		core_alloc.add_cpu(static_cast<unsigned int>(cpu));
	});
}

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
			size_t cores = this->core_alloc.alloc(quantity, cell);

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
			core_alloc.release(cell, id);
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
			core_alloc.return_core(id);
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

	core_alloc.confer(cell);
	if (!cell->initialized) {
		trace(TRACE_CELL, "Initalizeing cell");
		core_alloc.transfer(cell, cip->cores_reserved.first_cpu());
		cell->initialized = true;
	}

	return true;
}
