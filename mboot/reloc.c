/*******************************************************************************
 * Copyright (c) 2008-2012,2014-2016,2018,2020 VMware, Inc. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * reloc.c -- Relocations handling
 *
 *-- Definitions ---------------------------------------------------------------
 *
 *   "run-time object"
 *      A run-time object is any data structure, code region, or whatever memory
 *      that will have to be relocated. There are four kinds of objects:
 *       'k' for kernel sections that must be relocated at fixed addresses
 *       'm' for kernel modules which are tried to be relocated above the kernel
 *       's' for system info structures that can be relocated anywhere
 *       't' for the trampoline objects that must be relocated into safe memory
 *
 *   "run-time"
 *      is the state of an object after it has been relocated. It is opposed to
 *      'boot-time' that is the state of an object before it is relocated.
 *      Basically, the bootloader manipulates boot-time objects, and the kernel
 *      manipulates run-time objects.
 *
 *   "trampoline"
 *      This is the part of the bootloader (code + data) which is processing the
 *      relocations. It is the trampoline that actually moves each run-time
 *      object from its boot-time source to its run-time destination.
 *      A critical property of the trampoline is that it must execute from a
 *      memory region that will not be a destination for any run-time object.
 *      Otherwise, the trampoline would overwrite itself while relocating.
 *
 *   "bootloader's memory"
 *      This is the memory where are located the bootloader's code and data, and
 *      loaded modules.
 *
 *   "system memory"
 *      - firmware memory, ACPI, SMBIOS, I/O memory...
 *      - regions that are not reported as 'available' in the memory map
 *        (including memory map holes)
 *
 *   "hidden memory"
 *      This is the memory that, for any reason, we don't want to use for
 *      relocating into. For instance, memory which is susceptible to contain
 *      system information, but which is not reported as reserved in the memory
 *      map. This is often the case with the very first pages of memory which
 *      may contain information such as an interrupt vectors table.
 *
 *   "run-time memory"
 *      Memory that is allocated for relocating into. Any region that is neither
 *      system memory nor hidden memory is suitable for being used as run-time
 *      memory.
 *
 *   "safe memory"
 *      Any memory area that is reported as 'available' in the system memory map
 *      and that is not one of the following:
 *       - system memory
 *       - hidden memory
 *       - bootloader's memory
 *       - run-time memory
 *      Data written into safe memory are guaranteed not to overwrite anything,
 *      and not to be overwritten by the relocations. That is why the trampoline
 *      must be installed into safe memory.
 *
 *
 *-- Relocation process --------------------------------------------------------
 *
 * add_runtime_object(), blacklist_runtime_mem()
 *
 *   1. Register run-time objects
 *      Each object that must be relocated is added to the relocation table with
 *      the function add_runtime_object().
 *
 *   2. Blacklist non run-time memory
 *      Non run-time memory is the memory that cannot be used to relocate
 *      run-time objects into. This includes system memory and hidden memory.
 *      Such memory must be blacklisted with blacklist_runtime_mem() to
 *      guarantee that it is not going to be allocated later with run_malloc().
 *
 * compute_relocations()
 *
 *   3. Allocate run-time memory
 *      - Sort the objects by type ('k', 'm', 's') and by order of registration.
 *      - Allocate fixed run-time memory for the 'k' objects (kernel sections)
 *      - Allocate contiguous run-time memory for the 'm' objects (modules)
 *      - Allocate whatever run-time memory for the 's' objects (system info)
 *
 *   3. Blacklist bootloader's memory
 *      At this point, system memory and hidden memory have been blacklisted.
 *      Moreover, we are done allocating run-time memory. Therefore, only two
 *      kinds of memory remain available in the allocator: bootloader's memory
 *      and safe memory. After blacklisting the bootloader's memory, we are sure
 *      that the allocator will only provide safe memory.
 *
 * runtime_addr()
 *
 *   4. Dynamically link run-time objects
 *      Run-time objects are often complex structures that contains internal
 *      pointers referring to other run-time objects. These pointers must be
 *      updated with their run-time value using the runtime_addr() function.
 *
 * install_trampoline()
 *
 *   5. Order relocations
 *      Ordering relocations with reloc_order() is necessary to make sure that
 *      no relocation would overwrite the source of a later one. Sometimes,
 *      cyclic dependencies prevent from finding a safe relocation order. In
 *      this case, the object that is causing the cyclic dependency is moved to
 *      a place it will never overwrite another object: into safe memory.
 *
 *   6. Install the trampoline
 *      The trampoline is relocated into safe memory with install_trampoline()
 *      to ensure it will not overwrite itself later when processing the runtime
 *      objects relocations.
 *----------------------------------------------------------------------------*/

#include <cpu.h>
#include <string.h>
#include <boot_services.h>

#include "mboot.h"

#define MAX_RELOCS_NR   512              /* Relocation table size, in entries */

static reloc_t relocs[MAX_RELOCS_NR];    /* The relocation table */
static size_t reloc_count = 0;           /* Number of reloc table entries */

#define add_runtime_object_delimiter()    \
   memset(&relocs[reloc_count++], 0, sizeof (reloc_t));

static ALWAYS_INLINE bool reloc_object_might_be_executable(reloc_t *obj)
{
   if (obj->type == 'k' || obj->type == 't') {
      return true;
   }

   return false;
}

static void reloc_sanity_check(reloc_t *objs, size_t count)
{
   uint64_t size, dest;
   const char *msg;
   char *src, *dst;
   bool error;
   size_t i;

   alloc_sanity_check(false);

   if (count < 2) {
      Log(LOG_ERR, "Relocation table is empty.\n");
      while (1);
   }

   count--;
   if (objs[count].src != NULL || objs[count].dest != 0 ||
       objs[count].size != 0 || objs[count].align != 0 ||
       objs[count].type != 0) {
      Log(LOG_ERR, "Bad relocation table delimiter.\n");
      while (1);
   }

   error = false;

   for (i = 0; i < count; i++) {
      msg = NULL;
      src = objs[i].src;
      dest = objs[i].dest;
      size = objs[i].size;
      dst = UINT64_TO_PTR(dest);

      if (objs[i].type != 'k' && objs[i].type != 'm' && objs[i].type != 's' &&
          objs[i].type != 't') {
         msg = "invalid relocation type";
      } else if (size == 0) {
         msg = "zero-length relocation";
      } else if ((objs[i].type == 'k') && (objs[i].align != 1)) {
         msg = "fixed relocation is not 1-byte aligned";
      } else if (arch_is_x86 && (objs[i].type == 'k') && (dest == 0)) {
         msg = "fixed relocation at NULL destination not supported on x86";
      } else if (dest != PTR_TO_UINT64(dst)) {
         msg = "pointer overflow";
      } else if ((dst + size <= dst) || (src + size <= src)) {
         msg = "uint64 overflow";
      }

      if (msg != NULL) {
         error = true;
         Log(LOG_ERR, "[%c] %"PRIx64" - %"PRIx64
             " -> %"PRIx64" - %"PRIx64" (%"PRIu64" bytes): %s.\n",
             objs[i].type, PTR_TO_UINT64(src), PTR_TO_UINT64(src) + size - 1,
             dest, dest + size - 1, size, msg);
      }
   }

   if (error) {
      Log(LOG_ERR, "Relocation table is corrupted.\n");
      while (1);
   }
}

/*-- add_runtime_object --------------------------------------------------------
 *
 *      Add an object to the relocation table.
 *
 * Parameters
 *      IN type:  object type ('k', 'm', 's', or 't')
 *      IN src:   pointer to the object
 *      IN size:  object size, in bytes
 *      IN dest:  fixed run-time address if any, 0 otherwise
 *      IN align: at run-time, the object must be aligned on this boundary
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int add_runtime_object(char type, void *src, uint64_t size, run_addr_t dest,
                       size_t align)
{
   reloc_t *reloc;

   if (size > 0) {
      if (reloc_count + 2 >= MAX_RELOCS_NR) {
         Log(LOG_ERR, "Relocation table is full.\n");
         return ERR_OUT_OF_RESOURCES;
      }

      reloc = &relocs[reloc_count];
      reloc->src = src;
      reloc->size = size;
      reloc->dest = dest;
      reloc->align = (align > 0) ? align : ALIGN_ANY;
      reloc->type = type;
      reloc_count++;
   }

   return ERR_SUCCESS;
}

/*-- do_reloc ------------------------------------------------------------------
 *
 *      Process the relocations by moving all the objects in the relocation
 *      table from their boot-time source to their run-time destination.
 *      This function assumes that the relocation table is NULL-terminated.
 *
 *   WARNING:
 *      In the case a run-time object had to be relocated where do_reloc() keeps
 *      its own code and/or data, this function would overwrite itself.
 *      For that reason, do_reloc() must be relocated into safe memory, and to
 *      do so, MUST BE POSITION-INDEPENDENT.
 *
 * Parameters
 *      IN reloc: the table of relocations
 *----------------------------------------------------------------------------*/
static void TRAMPOLINE do_reloc(reloc_t *reloc)
{
   char *src, *dest;
   size_t size;

   for ( ; reloc->type != 0; reloc++) {
      src = reloc->src;

      dest = UINT64_TO_PTR(reloc->dest);
      size = (size_t)reloc->size;

      if (src == NULL) {
         /* bzero */
         while (size > 0) {
            dest[--size] = 0;
         }
      } else if (src != dest) {
         /* memmove */
         if (src < dest) {
            while (size > 0) {
               size--;
               dest[size] = src[size];
            }
         } else {
            while (size > 0) {
               *dest++ = *src++;
               size--;
            }
         }
      }

      if (reloc_object_might_be_executable(reloc)) {
         cpu_code_update(reloc->dest, reloc->size);
      }
   }

   cpu_code_update_commit();
}

/*-- reloc_compare -------------------------------------------------------------
 *
 *      For sorting the relocation table by object type.
 *
 * Parameters
 *      IN a: pointer to a relocation table entry
 *      IN b: pointer to another relocation table entry
 *
 * Results
 *      -1, 0 or 1, depending on whether a is respectively lesser than, equal to
 *      or greater than b.
 *----------------------------------------------------------------------------*/
static int reloc_compare(const void *a, const void *b)
{
   if (((const reloc_t *)a)->type < ((const reloc_t *)b)->type) {
      return -1;
   }
   if (((const reloc_t *)a)->type > ((const reloc_t *)b)->type) {
      return 1;
   }
   return 0;
}

/*-- set_runtime_addr ----------------------------------------------------------
 *
 *      Compute the relocations for a group of objects. When possible, objects
 *      are relocated contiguously, at the specified preferred address.
 *
 * Parameters
 *      IN objs:           table of objects to relocate
 *      IN count:          number of objects to relocate
 *      IN preferred_addr: preferred fixed address
 *      IN alloc_option:   ALLOC_ANY or ALLOC_32BIT
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int set_runtime_addr(reloc_t *objs, size_t count,
                            run_addr_t prefered_addr, int alloc_option)
{
   run_addr_t contig_mem;
   uint64_t size;
   size_t i;
   int status;
   size_t max_align = 1;

   if (count == 0) {
      return ERR_SUCCESS;
   }

   size = 0;

   /*
    * The "sizing loop."
    */
   for (i = 0; i < count; i++) {
      if (objs[i].align > max_align) {
         /*
          * This loop is trying to compute the size of the group of objects,
          * taking into account any gaps between objects that will be
          * necessitated by the required alignment of objects.
          * However, it's not possible to compute that unless
          * you know what alignment the first object will be placed at!
          * The sizing loop effectively places the first object at address
          * zero, which of course is perfectly aligned no matter what
          * alignment is required.  So the size it computes is valid
          * if and only if the first object gets aligned at the
          * worst-case alignment needed for any of the objects.
          *
          * Otherwise we can be rounding by different amounts in
          * the destination "assignment loop" below.
          */
         max_align = objs[i].align;
      }
      size = roundup64(size, objs[i].align) + objs[i].size;
   }

   contig_mem = 0;

   if (prefered_addr > 0) {
      /* First try to relocate contiguously, at the preferred address. */
      contig_mem = roundup64(prefered_addr, max_align);
      if (runtime_alloc_fixed(&contig_mem, size) != ERR_SUCCESS) {
         contig_mem = 0;
      }
   }

   if (contig_mem == 0) {
      /* No preferred address, or not enough contiguous space there. */
      /* Try to relocate contiguously, anywhere else */
       if (runtime_alloc(&contig_mem, size, max_align, alloc_option)
           != ERR_SUCCESS) {
          contig_mem = 0;
       }
   }

   /*
    * The "assignment loop."
    */
   for (i = 0; i < count; i++) {
      reloc_t *o = &objs[i];

      if (contig_mem == 0) {
         /* Cannot relocate contiguously, relocate anywhere separately. */
         status = runtime_alloc(&o->dest, o->size, o->align, alloc_option);
         if (status != ERR_SUCCESS) {
            return status;
         }
      } else {
         contig_mem = roundup64(contig_mem, o->align);
         o->dest = contig_mem;
         contig_mem += o->size;
      }
      if (boot.debug) {
         Log(LOG_DEBUG, "[%c] %"PRIx64" - %"PRIx64
             " -> %"PRIx64" - %"PRIx64" (%"PRIu64" bytes)", o->type,
             PTR_TO_UINT64(o->src), PTR_TO_UINT64(o->src) + o->size - 1,
             o->dest, o->dest + o->size - 1, o->size);
      }
   }

   return ERR_SUCCESS;
}

/*-- find_reloc_dependency -------------------------------------------------
 *
 *      Check whether the i-th relocation in the given relocation table has at
 *      least one dependency. The first seek_offset entries are ignored.
 *
 *      Rule: a relocation depends on anoter one if moving the first relocation
 *            from its source to its destination would overwrite the source of
 *            the second relocation.
 *
 * Parameters
 *      IN rel:         pointer to the relocation table
 *      IN i:           index of the reference relocation
 *      IN seek_offset: table index at which to start searching for dependencies
 *
 * Results
 *      The index of the first dependency, or the i-parameter if no dependency
 *      is found.
 *----------------------------------------------------------------------------*/
static size_t find_reloc_dependency(reloc_t *rel, size_t i, size_t seek_offset)
{
   size_t j;

   for (j = seek_offset; rel[j].type != 0; j++) {
      if (j == i || rel[j].src == NULL) {
         continue;
      }

      if (is_overlap(rel[i].dest, rel[i].size,
                     PTR_TO_UINT64(rel[j].src), rel[j].size)) {
         return j;
      }
   }

   return i;
}

/*-- break_reloc_deadlock -----------------------------------------------------
 *
 *      Locate and break a circular dependency in the given relocation table.
 *
 *      Initialization:
 *        1. Set the visit counter of each relocation table entry to 0.
 *        2. Locate the largest entry in the relocation table, and make it the
 *           initial current entry. The largest relocation is the most likely to
 *           overlap and be overlapped by other relocations.
 *
 *      Main loop:
 *        3. Visit the current entry:
 *           - If this entry has not been visited yet, we just increment its
 *             visit counter.
 *           - If this entry has already been visited once, then we are entering
 *             into a cycle. We increment its visit counter.
 *           - If this entry has already been visited twice, then we are
 *             terminating a cycle. We can exit the main loop and jump to 5.
 *        4. The first dependency of the current entry becomes the current
 *           entry, and we loop back to 3.
 *
 *     Termination:
 *        5. At this stage, all of the relocation table entries visited twice
 *           are part of a cycle. We can break this cycle by picking up any of
 *           these relocations, and move its source into safe memory (where it
 *           will not be overwritten by the destination of any other
 *           relocation).
 *           For avoiding copying too much data, we just pick the smallest one.
 *
 * Parameters
 *      IN rel: pointer to the relocation table
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int break_reloc_deadlock(reloc_t *rel)
{
   run_addr_t addr;
   size_t biggest, smallest;
   size_t i, tmp;
   uint64_t size;
   int status;

   biggest = 0;
   size = 0;

   for (i = 0; rel[i].type != 0; i++) {
      rel[i].visited = 0;
      if (rel[i].size > size) {
         size = rel[i].size;
         biggest = i;
      }
   }

   smallest = biggest;
   i = biggest;

   do {
      rel[i].visited++;

      if (rel[i].visited == 2 && rel[i].size < size) {
         size = rel[i].size;
         smallest = i;
      }

      tmp = i;
      i = find_reloc_dependency(rel, i, 0);
      if (i == tmp) {
         Log(LOG_ERR, "Internal error while resolving relocations.\n");
         return ERR_INVALID_PARAMETER;
      }
   } while (rel[i].visited < 2);

   status = alloc(&addr, size, ALIGN_ANY, ALLOC_ANY);
   if (status != ERR_SUCCESS) {
      Log(LOG_DEBUG, "...unable to move %p (size 0x%"PRIx64")",
          rel[smallest].src, size);
      Log(LOG_ERR, "Error resolving relocations: %s", error_str[status]);
      return status;
   }
   if (boot.debug) {
      Log(LOG_DEBUG, "...moving %p (size 0x%"PRIx64") temporarily to %p\n",
          rel[smallest].src, size, UINT64_TO_PTR(addr));
   }

   /*
    * Here we have (rel[smallest].src != NULL) because a relocation with a NULL
    * source is meant to have its destination zero'ed. Such relocations have no
    * actual source, so they cannot be part of a cycle.
    */
   rel[smallest].src = memcpy(UINT64_TO_PTR(addr), rel[smallest].src,
                              (size_t)size);

   return ERR_SUCCESS;
}

/*-- reloc_resolve -------------------------------------------------------------
 *
 *      Reorder the relocations, so moving a relocation from its source to its
 *      destination will not overwrite the source of another relocation.
 *
 *      Initialization:
 *        1. Set the resolved offset to 0.
 *
 *      Main loop:
 *        2. For each unresolved relocation (i.e. any entry in the relocation
 *           table which is located at/after the resolved offset), if the entry
 *           does not depend on any other unresolved relocation, it is moved at
 *           the resolved offset and we increment the resolved offset.
 *        3. If at least one relocation has been resolved:
 *            - if the resolved offset has been incremented up to the total
 *              number of relocations, we can exit the main loop
 *            - otherwise, we loop back to 2.
 *        4. If no relocation has been resolved, then we have a circular
 *           dependency. We break it and loop back to 2.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int reloc_resolve(void)
{
   size_t i, n, resolved_count;
   reloc_t *unresolved;
   int status;

   reloc_sanity_check(relocs, reloc_count);

   resolved_count = 0;

   while (resolved_count < reloc_count - 1) {
      unresolved = &relocs[resolved_count];

      n = 0;
      for (i = 0; unresolved[i].type != 0; i++) {
         if (find_reloc_dependency(unresolved, i, n) == i) {
            if (i != n) {
               mem_swap(&unresolved[i], &unresolved[n], sizeof (reloc_t));
            }
            n++;
         }
      }

      if (n > 0) {
         resolved_count += n;
      } else {
         status = break_reloc_deadlock(unresolved);
         if (status != ERR_SUCCESS) {
            return status;
         }
      }
   }

   reloc_sanity_check(relocs, reloc_count);

   return ERR_SUCCESS;
}

/*-- install_trampoline --------------------------------------------------------
 *
 *      Install the trampoline in safe memory, i.e. in a place it will not be
 *      overwritten by the coming relocations.
 *
 *      This function first allocates contiguous safe memory for the trampoline
 *      data:
 *        - handoff structure
 *        - trampoline stack
 *        - relocations table
 *      and then allocates contiguous safe memory for the trampoline code.
 *
 * Parameters
 *      OUT run_trampo:  pointer to the relocated trampoline entry point
 *      OUT run_handoff: pointer to the relocated handoff structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int install_trampoline(trampoline_t *run_trampo, handoff_t **run_handoff)
{
   const uint64_t data_size = sizeof (handoff_t) + TRAMPOLINE_STACK_SIZE;
   reloc_t *code, *data;
   handoff_t *handoff;
   int status;

   Log(LOG_DEBUG, "Finalizing relocations validation...\n");

   /*
    * reloc_resolve may allocate and immediately write into safe memory or high
    * memory via break_reloc_deadlock, so it must be called after
    * blacklist_bootloader_mem.  Perhaps we should change break_reloc_deadlock
    * so that instead of moving the chosen relocation out of the way itself, it
    * inserts an extra table entry so that the trampoline will move it later.
    */
   status = reloc_resolve();
   if (status != ERR_SUCCESS) {
      return status;
   }

   Log(LOG_DEBUG, "Preparing a safe environment...\n");

   /* Register the trampoline to be relocated. */
   status = add_safe_object(relocs, data_size + reloc_count * sizeof (reloc_t),
                            ALIGN_PTR);
   if (status != ERR_SUCCESS) {
      return status;
   }
   status = add_safe_object(_trampoline_start, TRAMPOLINE_SIZE(), ALIGN_FUNC);
   if (status != ERR_SUCCESS) {
      return status;
   }

   data = &relocs[reloc_count - 2];
   code = &relocs[reloc_count - 1];
   add_runtime_object_delimiter();
   reloc_sanity_check(data, 3);

   /* Compute the trampoline run-time addresses. */
#if only_x86
   status = set_runtime_addr(data, 2, 0, ALLOC_32BIT);
#else
   status = set_runtime_addr(data, 2, 0, ALLOC_ANY);
#endif
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Trampoline relocation error: out of safe memory.\n");
      return status;
   }

   /* Setup handoff structure internal pointers with their run-time value */
   handoff = UINT64_TO_PTR(data->dest);
   memset(handoff, 0, data_size);
   handoff->stack    = data->dest + sizeof (handoff_t);
   handoff->relocs   = data->dest + data_size;
   handoff->relocate = code->dest + TRAMPOLINE_OFFSETOF(do_reloc);

   *run_handoff = handoff;
   *run_trampo  = UINT64_TO_PTR(code->dest + TRAMPOLINE_OFFSETOF(trampoline));

   /* Process the relocations for the trampoline itself */
   Log(LOG_DEBUG, "Installing a safe environment...\n");
   data->dest += data_size;
   data->size -= data_size;
   do_reloc(data);

#if defined(only_em64t) || defined(only_arm64)
   status = relocate_page_tables2();
   if (status != ERR_SUCCESS) {
      return status;
   }
#endif /* defined(only_em64t) || defined(only_arm64) */

   return ERR_SUCCESS;
}

/*-- blacklist_bootloader_mem --------------------------------------------------
 *
 *       Mark the boot-loader memory as no longer safe to allocate. The
 *       boot-loader memory includes:
 *         - memory map ranges marked as E820_TYPE_BOOTLOADER (UEFI-only)
 *         - boot-loader code and data segments (COM32-only)
 *         - run-time data to be relocated:
 *             - boot modules
 *             - system info structures
 *         - internal boot-loader structures which have been dynamically
 *           allocated, and which are still needed after the trampoline
 *           relocation such as the framebuffer font.
 *
 * Parameters
 *      IN mmap:  pointer to the E820 memory map
 *      IN count: number of entries in the memory map
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int blacklist_bootloader_mem(UNUSED_PARAM(e820_range_t *mmap),
                                    UNUSED_PARAM(size_t count))
{
   int status;
   size_t i;

#if defined(__COM32__)
   status = blacklist_runtime_mem(0, PTR_TO_UINT64(_end));
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Loader memory reservation error.\n");
      return status;
   }
#else /* UEFI-only */
   for (i = 0; i < count; i++) {
      if (mmap->type == E820_TYPE_BOOTLOADER && E820_LENGTH(mmap) > 0) {
         status = blacklist_runtime_mem(E820_BASE(mmap), E820_LENGTH(mmap));
         if (status != ERR_SUCCESS) {
            Log(LOG_ERR, "Loader memory reservation error.\n");
            return status;
         }
      }
      mmap++;
   }
#endif

   for (i = 0; relocs[i].type != 0; i++) {
      if (relocs[i].size > 0 && relocs[i].src != NULL) {
         status = blacklist_runtime_mem((run_addr_t)PTR_TO_UINT(relocs[i].src),
                                        relocs[i].size);
         if (status != ERR_SUCCESS) {
            Log(LOG_ERR, "Used memory reservation error.\n");
            return status;
         }
      }
   }

   if (fb_font.glyphs != NULL) {
      status = blacklist_runtime_mem((run_addr_t)PTR_TO_UINT(fb_font.glyphs),
                                     font_size());
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "Font memory reservation error.\n");
         return status;
      }
   }

   return ERR_SUCCESS;
}

/*-- compute_relocations -------------------------------------------------------
 *
 *      Compute the run-time addresses of the objects to be relocated.
 *
 * Parameters
 *      IN mmap:  pointer to the E820 memory map
 *      IN count: number of entries in the memory map
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int compute_relocations(e820_range_t *mmap, size_t count)
{
   int k = 0, m = 0, s = 0;
   run_addr_t kmem_end = 0;
   size_t i;
   int status;

   Log(LOG_DEBUG, "Calculating relocations...\n");

   /*
    * Sort the relocation table by object type and by insertion order.
    * Sorting algorithm must be stable.
    */
   bubble_sort(relocs, reloc_count, sizeof (reloc_t), reloc_compare);

   for (i = 0; i < reloc_count; i++) {
      if (relocs[i].type == 'k') {
         /*
          * 'k' object destination ranges have already been
          * allocated inside elf_register.
          */
         kmem_end = MAX(kmem_end, relocs[i].dest + relocs[i].size);
         k++;
      } else if (relocs[i].type == 'm') {
         m++;
      } else if (relocs[i].type == 's') {
         s++;
      } else {
         Log(LOG_ERR, "Invalid run-time object type.\n");
         return ERR_INCONSISTENT_DATA;
      }
   }

   /*
    * Next relocate the system information, preferring to put it right
    * after the 'k' object(s).  This is needed on x86 because
    * currently the x86 'k' object (vmkBoot) and system information
    * must be in low memory (below 4GB) even when booting in 64-bit
    * UEFI mode.  So we need to do this relocation before we possibly
    * run out of low memory.
    */
#if only_x86
   status = set_runtime_addr(&relocs[k + m], s, kmem_end, ALLOC_32BIT);
#else
   status = set_runtime_addr(&relocs[k + m], s, kmem_end, ALLOC_ANY);
#endif
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Boot info relocation error: %s", error_str[status]);
      return status;
   }

   /*
    * Finally relocate the modules sections.  These must be in low
    * memory if booting an old x86 multiboot kernel.  Of course they
    * must also be in low memory if booting in legacy BIOS or 32-bit
    * UEFI mode, but in that case there is no distinction between
    * ALLOC_ANY and ALLOC_32BIT.
    */
   status = set_runtime_addr(&relocs[k], m, 0,
                             boot.is_esxbootinfo ? ALLOC_ANY : ALLOC_32BIT);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Modules relocation error: %s", error_str[status]);
      return status;
   }

   /* Make sure the relocation table is NULL-terminated for do_reloc() */
   add_runtime_object_delimiter();

   status = blacklist_bootloader_mem(mmap, count);

   /*
    * Now that we have blacklisted the bootloader's memory, there is only one
    * kind of memory remaining available: safe memory.
    */

   return status;
}

/*-- runtime_addr --------------------------------------------------------------
 *
 *      Get the run-time address of a relocated object.
 *
 *      Do never call this function on objects that have been sorted with
 *      reloc_sort().
 *
 * Parameters
 *      IN  ptr:     boot-time pointer
 *      OUT runaddr: run-time address
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int runtime_addr(const void *ptr, run_addr_t *runaddr)
{
   size_t i;

   for (i = 0; relocs[i].type != 0; i++) {
      if (relocs[i].src == ptr) {
         *runaddr = relocs[i].dest;
         return ERR_SUCCESS;
      }
   }

   return ERR_NOT_FOUND;
}
