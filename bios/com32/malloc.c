/*------------------------------------------------------------------------------
 *   Copyright 2004-2009 H. Peter Anvin - All Rights Reserved
 *   Portions Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *----------------------------------------------------------------------------*/

/*
 * malloc.c -- Memory allocation
 */

#include <sys/types.h>
#include <string.h>
#include <cpu.h>

#include "com32_private.h"

typedef uint64_t addr_t;

/* This structure should be a power of two. This becomes the alignment unit. */
struct free_arena_header;

struct arena_header {
   size_t type;
   size_t size;     /* Also gives the location of the next entry */
   struct free_arena_header *next, *prev;
};

struct free_arena_header {
   struct arena_header a;
   struct free_arena_header *next_free, *prev_free;
};

#define ONE_MB           0x100000ULL
#define FOUR_GB          0x100000000ULL
#define E820_MEM_MAX     (FOUR_GB - ONE_MB)

#define ARENA_TYPE_USED  0
#define ARENA_TYPE_FREE  1
#define ARENA_TYPE_HEAD  2

#define ARENA_SIZE_MASK  (~(uintptr_t)(sizeof(struct arena_header)-1))

#define ARENA_ALIGN_UP(p)  \
   (((uintptr_t)(p) + ~ARENA_SIZE_MASK) & ARENA_SIZE_MASK)

static struct free_arena_header __malloc_head = {
   {
      ARENA_TYPE_HEAD,
      0,
      &__malloc_head,
      &__malloc_head,
   },
   &__malloc_head,
   &__malloc_head
};

static INLINE uintptr_t sp(void)
{
   uintptr_t sp;

   __asm__ __volatile__ ("movl %%esp,%0" : "=rm" (sp));

   return sp;
}

static struct free_arena_header *__free_block(struct free_arena_header *ah)
{
   struct free_arena_header *pah, *nah;

   pah = ah->a.prev;
   nah = ah->a.next;

   if (pah->a.type == ARENA_TYPE_FREE &&
       (char *)pah + pah->a.size == (char *)ah) {
      /* Coalesce into the previous block */
      pah->a.size += ah->a.size;
      pah->a.next = nah;
      nah->a.prev = pah;

      ah = pah;
      pah = ah->a.prev;
   } else {
      /* Need to add this block to the free chain */
      ah->a.type = ARENA_TYPE_FREE;

      ah->next_free = __malloc_head.next_free;
      ah->prev_free = &__malloc_head;
      __malloc_head.next_free = ah;
      ah->next_free->prev_free = ah;
   }

   /* In either of the previous cases, we might be able to merge
      with the subsequent block... */
   if (nah->a.type == ARENA_TYPE_FREE &&
       (char *)ah + ah->a.size == (char *)nah) {
      ah->a.size += nah->a.size;

      /* Remove the old block from the chains */
      nah->next_free->prev_free = nah->prev_free;
      nah->prev_free->next_free = nah->next_free;
      ah->a.next = nah->a.next;
      nah->a.next->a.prev = ah;
   }

   /* Return the block that contains the called block */
   return ah;
}

/*
 * This is used to insert a block which is not previously on the
 * free list.  Only the a.size field of the arena header is assumed
 * to be valid.
 */
static void __inject_free_block(addr_t start, addr_t length)
{
   struct free_arena_header *ah, *nah;

   for (nah = __malloc_head.a.next; nah->a.type != ARENA_TYPE_HEAD;
        nah = nah->a.next) {

      if ((uintptr_t)nah >= start + length) {
         /* nah is entirely beyond this block? */
         break;
      }

      /* Is this block entirely beyond nah? */
      if (start < (uintptr_t)nah + nah->a.size) {
         /* We have some sort of overlap - reject this block */
         return;
      }
   }

   /* Now, nah should point to the successor block */
   ah = (struct free_arena_header *)(uintptr_t)start;
   ah->a.size = length;
   ah->a.next = nah;
   ah->a.prev = nah->a.prev;
   nah->a.prev = ah;
   ah->a.prev->a.next = ah;

   __free_block(ah);
}

static void *__malloc_from_block(struct free_arena_header *fp, size_t size)
{
   size_t fsize;
   struct free_arena_header *nfp, *na;

   fsize = fp->a.size;

   /* We need the 2* to account for the larger requirements of a free block */
   if (fsize >= size + 2 * sizeof(struct arena_header)) {
      /* Bigger block than required -- split block */
      nfp = (struct free_arena_header *)((char *)fp + size);
      na = fp->a.next;

      nfp->a.type = ARENA_TYPE_FREE;
      nfp->a.size = fsize - size;
      fp->a.type = ARENA_TYPE_USED;
      fp->a.size = size;

      /* Insert into all-block chain */
      nfp->a.prev = fp;
      nfp->a.next = na;
      na->a.prev = nfp;
      fp->a.next = nfp;

      /* Replace current block on free chain */
      nfp->next_free = fp->next_free;
      nfp->prev_free = fp->prev_free;
      fp->next_free->prev_free = nfp;
      fp->prev_free->next_free = nfp;
   } else {
      /* Allocate the whole block */
      fp->a.type = ARENA_TYPE_USED;

      /* Remove from free chain */
      fp->next_free->prev_free = fp->prev_free;
      fp->prev_free->next_free = fp->next_free;
   }

   return (void *)(&fp->a + 1);
}

void *sys_malloc(size_t size)
{
   struct free_arena_header *fp;

   if (size == 0) {
      return NULL;
   }

   /* Add the obligatory arena header, and round up */
   size = (size + 2 * sizeof(struct arena_header) - 1) & ARENA_SIZE_MASK;

   for (fp = __malloc_head.next_free; fp->a.type != ARENA_TYPE_HEAD;
        fp = fp->next_free) {
      if (fp->a.size >= size) {
         /* Found fit -- allocate out of this block */
         return __malloc_from_block(fp, size);
      }
   }

   return NULL;
}

void sys_free(void *ptr)
{
   struct free_arena_header *ah;

   if (ptr == NULL) {
      return;
   }

   ah = (struct free_arena_header *)((struct arena_header *)ptr - 1);

   __free_block(ah);
}

void *realloc(void *ptr, size_t size)
{
   struct free_arena_header *ah, *nah;
   void *newptr;
   size_t newsize, oldsize, xsize;

   if (!ptr)
      return sys_malloc(size);

   if (size == 0) {
      sys_free(ptr);
      return NULL;
   }

   ah = (struct free_arena_header *)
      ((struct arena_header *)ptr - 1);

   /* Actual size of the old block */
   oldsize = ah->a.size;

   /* Add the obligatory arena header, and round up */
   newsize = (size + 2 * sizeof(struct arena_header) - 1) & ARENA_SIZE_MASK;

   if (oldsize >= newsize && newsize >= (oldsize >> 2) &&
       oldsize - newsize < 4096) {
      /* This allocation is close enough already. */
      return ptr;
   } else {
      xsize = oldsize;

      nah = ah->a.next;
      if ((char *)nah == (char *)ah + ah->a.size &&
       nah->a.type == ARENA_TYPE_FREE &&
          oldsize + nah->a.size >= newsize) {
         /* Merge in subsequent free block */
         ah->a.next = nah->a.next;
         ah->a.next->a.prev = ah;
         nah->next_free->prev_free = nah->prev_free;
         nah->prev_free->next_free = nah->next_free;
         xsize = (ah->a.size += nah->a.size);
      }

      if (xsize >= newsize) {
         /* We can reallocate in place */
         if (xsize >= newsize + 2 * sizeof(struct arena_header)) {
            /* Residual free block at end */
            nah = (struct free_arena_header *)((char *)ah + newsize);
            nah->a.type = ARENA_TYPE_FREE;
            nah->a.size = xsize - newsize;
            ah->a.size = newsize;

            /* Insert into block list */
            nah->a.next = ah->a.next;
            ah->a.next = nah;
            nah->a.next->a.prev = nah;
            nah->a.prev = ah;

            /* Insert into free list */
            if (newsize > oldsize) {
               /* Hack: this free block is in the path of a memory object
             which has already been grown at least once.  As such, put
             it at the *end* of the freelist instead of the beginning;
             trying to save it for future realloc()s of the same block. */
               nah->prev_free = __malloc_head.prev_free;
               nah->next_free = &__malloc_head;
               __malloc_head.prev_free = nah;
               nah->prev_free->next_free = nah;
            } else {
               nah->next_free = __malloc_head.next_free;
               nah->prev_free = &__malloc_head;
               __malloc_head.next_free = nah;
               nah->next_free->prev_free = nah;
            }
         }
         /* otherwise, use up the whole block */
         return ptr;
      } else {
         /* Last resort: need to allocate a new block and copy */
         oldsize -= sizeof(struct arena_header);
         newptr = sys_malloc(size);
         if (newptr) {
            memcpy(newptr, ptr, MIN(size, oldsize));
            sys_free(ptr);
         }
         return newptr;
      }
   }
}

static void consider_memory_area(addr_t start, size_t len)
{
   addr_t end;

   if (start >= E820_MEM_MAX || len == 0) {
      return;
   }

   if (len > E820_MEM_MAX - start) {
      len = E820_MEM_MAX - start;
   }

   end = start + len;

   if (end <= __com32.cs_memsize) {
      return;
   }

   if (start <= __com32.cs_memsize) {
      start = __com32.cs_memsize;
      len = end - start;
   }

   if (len >= 2 * sizeof(struct arena_header)) {
      __inject_free_block(start, len);
   }
}

static int com32_scan_memory(void)
{
   e820_range_t e820buf;
   uint64_t start, len, maxlen;
   uint32_t desc_size, next;
   bool memfound;
   int status;
   size_t dosmem, s1, s2;
   const addr_t bios_data = 0x510; /* Amount to reserve for BIOS data */

   if (__com32.cs_sysargs < 7 || __com32.cs_memsize == 0) {
      return ERR_UNSUPPORTED;
   }

   /* Use INT 12h to get DOS memory */
   if (int12_get_memory_size(&dosmem) != ERR_SUCCESS) {
      dosmem = bios_get_ebda();
      if (dosmem == 0) {
         dosmem = LOWMEM_LIMIT;   /* Hope for the best... */
      }
   }

   consider_memory_area(bios_data, dosmem - bios_data);

   memfound = false;
   next = 0;

   do {
      status = int15_e820(&e820buf, &next, &desc_size);
      if (status != ERR_SUCCESS) {
         break;
      }

      start = E820_BASE(&e820buf);
      len = E820_LENGTH(&e820buf);

      if (start < FOUR_GB) {
         /* Don't rely on E820 being valid for low memory.  Doing so
          could mean stuff like overwriting the PXE stack even when
          using "keeppxe", etc. */
         if (start < BIOS_UPPER_MEM_START) {
            if (len > BIOS_UPPER_MEM_START - start) {
               len -= BIOS_UPPER_MEM_START - start;
            } else {
               len = 0;
            }
            start = BIOS_UPPER_MEM_START;
         }

         maxlen = FOUR_GB - start;
         if (len > maxlen) {
            len = maxlen;
         }

         if (len > 0 && e820buf.type == E820_TYPE_AVAILABLE) {
            consider_memory_area(start, len);
            memfound = true;
         }
      }
   } while (next != 0);

   if (memfound) {
      return ERR_SUCCESS;
   }

   /* Next try INT 15h AX=E801h */
   if (int15_e801(&s1, &s2) == ERR_SUCCESS) {
      if (s1 > 0) {
         consider_memory_area(BIOS_UPPER_MEM_START, s1);
         if (s2 > 0) {
            consider_memory_area(BIOS_UPPER_MEM_START, s2);
         }
      }
      return ERR_SUCCESS;
   }

   /* Finally try INT 15h AH=88h */
   if (int15_88(&s1) == ERR_SUCCESS) {
      consider_memory_area(BIOS_UPPER_MEM_START, s1);
   }

   return ERR_SUCCESS;
}

/* FIXME: ASSUME THE STACK IS SETUP HIGH ENOUGH! */
static void CONSTRUCTOR init_memory_arena(void)
{
   size_t stack_size, total_space;
   addr_t start;

   start = ARENA_ALIGN_UP(&_end);
   total_space = sp() - start;

   stack_size = MIN(STACK_SIZE, total_space / 2);
   if (total_space < stack_size + 4 * sizeof (struct arena_header)) {
      stack_size = total_space - 4 * sizeof (struct arena_header);
   }

   __inject_free_block(start, total_space - stack_size);

   /* Scan the memory map to look for other suitable regions */
   com32_scan_memory();
}
