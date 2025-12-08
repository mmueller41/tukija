/**
 * Habitat
 *
 * Copyright (c) 2025 Michael Müller <michael.mueller@uos.de>, Osnabrück University
 */

#include "compiler.hpp"
#include "cpuset.hpp"
#include "core_allocator.hpp"
#include "kobject.hpp"
#include "quota.hpp"
#include "rcu.hpp"
#include "refptr.hpp"
#include "resource.hpp"
#include "kobject.hpp"
#include "slab.hpp"
#include "space_obj.hpp"
#include "pd.hpp"

class Cell;

struct Habitat_info_page
{
		alignas(64) Cpuset reserved_cores{0};
		alignas(64) Cpuset current_cores{0};
		alignas(64) bool   resizeable{false};
		
		Paddr map(Pd *parent, Paddr parent_va);
		void *operator new(size_t, Pd &pd);
};

class Habitat : public Kobject, public Refcount
{
	private:

		Habitat_info_page *haip;        /* Page containing the resources this habitat controls */
		Cell *hoitaja; /* Pointer to the Hoitaja instance of this cell. This is used for allocating
		                  additional CPU cores from the parent habitat and to forward withdrawal
		                  requests to. */
		Habitat *parent{
			nullptr}; /* The enclosing parent habitat of this habitat. If this habitat requires more
		                 resources, allocation requests are forwarded to the parent habitat. */

		void init();

		bool _check_enclosure(Cpuset cpus)
		{
			bool enclosed = true;
			Cpuset::for_each(cpus, [&](long cpu) {
				enclosed &= haip->reserved_cores.chk(static_cast<unsigned int>(cpu));
			});
			return enclosed;
		}

		static void pre_free(Rcu_elem *) { }

		static void free(Rcu_elem *)
		{
			//Habitat *habitat = static_cast<Habitat *>(a);
			/*if (habitat->del_ref()) {
				assert(habitat != &Habitat::root);
				delete habitat;
		    }*/
		}

		static Slab_cache cache;

	public:

		Habitat(Pd *own, mword sel, struct Habitat_info_page *haip_hva, Cell *hoitaja_ptr,
		        Habitat *home);	
		
		/* Operations on cells */
        /**
         * @brief Create a cell object
         * 
         * @param prio - priority of the cell. This determines the share of resources the cell gets.
         * @param cip_hva - Hypervisor virtual address to the cell's info page
         * @return Cell* - the newly created cell
         */
		Cell *create_cell(Pd *pd, unsigned short prio, struct Cip *cip_hva);

		/**
		 * @brief Destroy a cell in this habitat
		 * 
		 * @param cell - the cell to destroy
		 */
		void  destroy_cell(Cell *cell, Pd *pd);

		/**
		 * @brief Allocate resources for a cell
		 * 
		 * @param type - the type of resource to allocate (currently only CPU cores are supported)
		 * @param quantity - how many units of the resource are requested
		 * @param cell - the cell to allocate resources for
		 * @return size_t - number of actually allocated resources (maybe less then requested)
		 */
		size_t alloc(Resource::Type type, unsigned int quantity, Cell *cell);

		/**
		 * @brief Release a resource from a cell
		 * 
		 * @param type - type of the Resource (currently only CPU cores)
		 * @param cell - the cell that releases the resource
		 */
		void release(Resource::Type type, Cell *cell, unsigned short id);

		void return_resource(Resource::Type type, unsigned short id);

		/**
		 * @brief Set the CPU core affinity of a cell
		 *
		 * @param cell - cell to set the affinity of
         * @return true, if reserved cores are inside this habitat; false, if not
		 */
        bool   set_affinity(Cell *cell);

		Cpuset get_affinity()
		{
			return haip->reserved_cores;
		}

		bool resizeable()
		{
			return haip->resizeable;
		}
		
		ALWAYS_INLINE
		static inline void *operator new(size_t, Quota &quota) { return cache.alloc(quota); }

		ALWAYS_INLINE
		static inline void operator delete(void *ptr)
		{
			cache.free(ptr, Pd::current->quota);
		}
		
        static Habitat root;
};