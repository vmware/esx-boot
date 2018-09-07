/*******************************************************************************
 * Copyright (c) 2008-2013 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * bios.h -- BIOS definitions
 *
 *  BIOS 1st MB memory map:
 *
 *  <--------------- Low memory (640 Kb) -------------------->
 *
 *  +-----------+-----+--------+--------+--------------+------+---------------+
 *  |           |     |        |        |              |      |               |
 *  | Real mode | BDA |  Free  |  Boot  |     Free     | EBDA |      BIOS     |
 *  |    IVT    |     | memory | sector |    memory    |      |      ROM      |
 *  |           |     |        |        |              |      |               |
 *  +-----------+-----+--------+--------+--------------+------+---------------+
 *  0          400h 501h      7C00     7E00            ?    A0000h           1Mb
 *
 */

#ifndef BIOS_H_
#define BIOS_H_

#include <stdbool.h>
#include <compat.h>

#define LOWMEM_LIMIT             0xa0000ULL   /* 640 Kb */
#define BIOS_ROM_START           0xa0000ULL   /* 640 Kb */
#define BIOS_UPPER_MEM_START     0x100000ULL  /* 1Mb */

#define BIOS_ROM_SIZE            (BIOS_UPPER_MEM_START - BIOS_ROM_START)

#define PIT8254_DIVIDER          65536    /* Per Spec */
#define PC_PIT_FREQ              1193182
#define PIT8254_MAX_TICK_VALUE \
   (((uint64_t)(PC_PIT_FREQ) * (SECS_PER_DAY)) / (PIT8254_DIVIDER))

#define SECONDS_TO_BIOS_TICKS(s) \
   (((uint64_t)(s) * PC_PIT_FREQ) / (PIT8254_DIVIDER))
#define BIOS_TICKS_TO_MILLISEC(tc) \
   ((uint64_t)(tc) * (((PIT8254_DIVIDER) * (MILLISECS_IN_ONE_SEC))/(PC_PIT_FREQ)))

#pragma pack(1)
typedef union {
   uint32_t ptr;
   struct {
      uint16_t offset;
      uint16_t segment;
   } real;
} farptr_t;
#pragma pack()

static INLINE farptr_t linear_to_real(uint32_t linear)
{
   farptr_t ptr;

   ptr.real.segment = (uint16_t)(linear >> 4);
   ptr.real.offset = (uint16_t)(linear & 0xf);

   return ptr;
}

static INLINE uint32_t real_to_linear(farptr_t ptr)
{
   return ((uint32_t)ptr.real.segment << 4) + ptr.real.offset;
}

static INLINE void *linear_to_virtual(uint32_t linear)
{
   return (void *)(uintptr_t)(linear);
}

static INLINE uint32_t virtual_to_linear(const volatile void *virtual)
{
   return (uint32_t)(uintptr_t)virtual;
}

static INLINE void *real_to_virtual(farptr_t ptr)
{
   return linear_to_virtual(real_to_linear(ptr));
}

static INLINE farptr_t virtual_to_real(const volatile void *virtual)
{
   return linear_to_real(virtual_to_linear(virtual));
}

/*-- ptr_real_offset -----------------------------------------------------------
 *
 *      Calculate the real-mode offset of a physical linear address relatively
 *      to the given real-mode segment.
 *
 * Parameters
 *      IN linear:  the physical linear address
 *      IN segment: the real-mode segment
 *
 * Results
 *      The real-mode offset.
 *----------------------------------------------------------------------------*/
static INLINE uint16_t ptr_real_offset(const volatile void *virtual,
                                       uint16_t segment)
{
   return (uint16_t)(virtual_to_linear(virtual) - ((uint32_t)segment << 4));
}

/*
 * BIOS Data Area (BDA) map
 */
#define BDA_SEGMENT             0x40
#define BDA_OFFSET              0
#define EBDA_MAX_SIZE           (128 * 1024)

#pragma pack(1)
typedef struct {
   volatile uint16_t com[4];            /* 00h - COM ports base I/O address */
   volatile uint16_t lpt[3];            /* 08h - LPT ports base I/O address */

   volatile uint16_t ebda;              /* 0Eh - EBDA segment */
   volatile uint16_t equipment;         /* 10h - Installed hardware */
   volatile uint8_t post_status;        /* 12h - POST status */
   volatile uint16_t low_mem_size;      /* 13h - Low mem size in kb (0-640) */
   volatile uint16_t reserved1;         /* 15h - Reserved */

   volatile uint8_t kbd_status_1;       /* 17h - Keyboard status flags 1 */
   volatile uint8_t kbd_status_2;       /* 18h - Keyboard status flags 2 */
   volatile uint8_t kbd_alt_keypad;     /* 19h - Alt Numpad work area */
   volatile uint16_t kbd_head;          /* 1Ah - Next char addr in kbd buffer */
   volatile uint16_t kbd_tail;          /* 1Ch - Last char addr in kbd buffer */
   volatile uint16_t kbd_buffer[16];    /* 1Eh - Keyboard buffer */

   volatile uint8_t floppy_recalibrate; /* 3Eh - Floppy calibration status */
   volatile uint8_t floppy_motor;       /* 3Fh - Floppy drive motor status */
   volatile uint8_t floppy_timeout;     /* 40h - Floppy motor time-out */
   volatile uint8_t floppy_status;      /* 41h - Floppy last command status */
   volatile uint8_t floppy_command[7];  /* 42h - Floppy/HD status/command */

   volatile uint8_t video_mode;         /* 49h - Video current mode */
   volatile uint16_t video_columns;     /* 4Ah - Text columns per row */
   volatile uint16_t video_page_size;   /* 4Ch - Video page size in bytes */
   volatile uint16_t video_page_addr;   /* 4Eh - Video current page address */
   volatile uint8_t video_cursor[16];   /* 50h - Video cursor pos (pages 0-7) */
   volatile uint16_t video_cursor_type; /* 60h - Video cursor type */
   volatile uint8_t video_page;         /* 62h - Video current page number */
   volatile uint16_t video_crt_addr;    /* 63h - Video CRT controller port */
   volatile uint8_t video_mode_select;  /* 65h - Video mode select register */
   volatile uint8_t video_cga_palette;  /* 66h - CGA color palette */

   volatile uint32_t post_rm_entry;     /* 67h - Real mode re-entry point */
   volatile uint8_t last_spurious_int;  /* 6Bh - Last unexpected interrupt */

   volatile uint32_t timer_ticks;       /* 6Ch - Timer ticks since midnight */
   volatile uint8_t timer_overflow;     /* 70h - Timer 24 hour flag */

   volatile uint8_t kbd_ctrl_break;     /* 71h - Keyboard Ctrl-Break flag */
   volatile uint16_t post_reset_flags;  /* 72h - Soft reset flag */

   volatile uint8_t disk_status;        /* 74h - Last HD operation status */
   volatile uint8_t disk_count;         /* 75h - Number of HD drives */
   volatile uint8_t disk_ctrl;          /* 76h - Hard disk control byte */
   volatile uint8_t disk_io_port;       /* 77h - Hard disk I/O port offset */

   volatile uint8_t lpt_timeout[3];     /* 78h - LPT port timeouts (1 to 3) */
   volatile uint8_t virtual_dma;        /* 7Bh - Virtual DMA support */
   volatile uint8_t com_timeout[4];     /* 7Ch - COM port timeouts (1 to 4) */

   volatile uint16_t kbd_buffer_start;  /* 80h - Keyboard buf starting offset */
   volatile uint16_t kbd_buffer_end;    /* 82h - Keyboard buf ending offset */

   volatile uint8_t video_rows;         /* 84h - rows on the screen minus 1 */
   volatile uint16_t bytes_per_char;    /* 85h - Bytes per character */
   volatile uint8_t video_options;      /* 87h - EGA/VGA control */
   volatile uint8_t video_switches;     /* 88h - Video EGA/VGA switches */
   volatile uint8_t video_control;      /* 89h - MCGA/VGA control flags */
   volatile uint8_t video_dcc_idx;      /* 8Ah - MCGA/VGA DCC table index */

   volatile uint8_t floppy_data_rate;   /* 8Bh - Last floppy data rate */
   volatile uint8_t disk_ctrlr_status;  /* 8Ch - HD controller status */
   volatile uint8_t disk_ctrlr_error;   /* 8Dh - HD controller error */
   volatile uint8_t disk_complete;      /* 8Eh - HD Interrupt Control */
   volatile uint8_t floppy_info;        /* 8Fh - Diskette controller info */
   volatile uint8_t drive_state[4];     /* 90h - Drives 0-3 media states */
   volatile uint8_t floppy_track[2];    /* 94h - Drive 0, 1 current track */

   volatile uint8_t kbd_mode;           /* 96h - Keyboard mode/type */
   volatile uint8_t kbd_led_status;     /* 97h - Keyboard LED status */

   volatile uint32_t timer2_ptr;        /* 98h - Pointer to user wait flag */
   volatile uint32_t timer2_timeout;    /* 9Ch - Timeout in microseconds */
   volatile uint8_t timer2_wait_active; /* A0h - Wait active flag */

   volatile uint8_t lan_a_channel;      /* A1h - LAN A DMA channel flags */
   volatile uint8_t lan_a_status[2];    /* A2h - status LAN A 0, 1 */
   volatile uint32_t disk_ivt;          /* A4h - Saved HD interrupt vector */
   volatile uint32_t video_ptr;         /* A8h - Video save pointer table */
   volatile uint8_t reserved2[8];       /* ACh - Reserved */
   volatile uint8_t kbd_nmi;            /* B4h - Keyboard NMI control flags */
   volatile uint32_t kbd_break_pending; /* B5h - Keyboard break pending flags */
   volatile uint8_t port_60_queue;      /* B9h - Port 60 single byte queue */
   volatile uint8_t scancode;           /* BAh - Scancode of last key */
   volatile uint8_t nmi_head;           /* BBh - pointer to NMI buffer head */
   volatile uint8_t nmi_tail;           /* BCh - pointer to NMI buffer tail */
   volatile uint8_t nmi_buffer[16];     /* BDh - NMI scan code buffer */
   volatile uint8_t reserved3;          /* CDh - Reserved */
   volatile uint16_t day;               /* CEh - Day counter */
   volatile uint8_t reserved4[32];      /* D0h - Reserved */
   volatile uint8_t userspace[16];      /* F0h - Reserved for user */
   volatile uint8_t print_screen;       /* 100h - Print Screen Status byte */
} bios_data_area_t;
#pragma pack()

/*-- bios_get_bda --------------------------------------------------------------
 *
 *      Return a pointer to the Bios Data Area.
 *
 * Results
 *      A pointer to the BDA.
 *----------------------------------------------------------------------------*/
static INLINE bios_data_area_t * bios_get_bda(void)
{
   farptr_t ptr;

   ptr.real.segment = BDA_SEGMENT;
   ptr.real.offset = BDA_OFFSET;

   return real_to_virtual(ptr);
}

/*-- bios_get_ebda -------------------------------------------------------------
 *
 *      Get the EBDA base address.
 *
 * Results
 *      The EBDA base address, or 0 if EBDA was not found.
 *----------------------------------------------------------------------------*/
static INLINE uint32_t bios_get_ebda(void)
{
   bios_data_area_t * const bda = bios_get_bda();
   uint32_t ebda;
   farptr_t ptr;

   ptr.real.segment = bda->ebda;
   ptr.real.offset = 0;

   ebda = real_to_linear(ptr);
   if (ebda < (LOWMEM_LIMIT - EBDA_MAX_SIZE) || ebda >= LOWMEM_LIMIT) {
      return 0;
   }

   return ebda;
}

/*-- bios_get_com_port ---------------------------------------------------------
 *
 *      Scan the BDA for the I/O base address of a given serial COM port.
 *
 * Parameters
 *      IN com: 1=COM1, 2=COM2, 3=COM3, 4=COM4
 *
 * Results
 *      The COM port I/O base address, or 0 if not supported.
 *----------------------------------------------------------------------------*/
static INLINE uint16_t bios_get_com_port(uint8_t com)
{
   bios_data_area_t * const bda = bios_get_bda();

   if (com == 0 || com > 4) {
      return 0;
   }

#ifdef BIOS_STRESS_TEST
   /*
    * Bits 9-11 of the BDA equipment field indicate the number of serial ports
    * installed. Not sure if this is properly populated by all BIOSes.
    */
   if (com > ((bda->equipment >> 9) & 0x7)) {
      return 0;
   }
#endif

   return bda->com[com - 1];
}

/*-- bios_get_current_tick -----------------------------------------------------
 *
 *      Return the number of timer ticks since midnight.
 *
 * Results
 *      Number of ticks.
 *----------------------------------------------------------------------------*/
static INLINE uint32_t bios_get_current_tick(void)
{
   bios_data_area_t * const bda = bios_get_bda();

   return bda->timer_ticks;
}

/*-- bios_has_timer_overflowed ------------------------------------------------
 *
 *      Indicate if the BIOS timer has overflowed. On some newer machines
 *      the flag "timer_overflow" of the bios data area could indicate the
 *      number of times the timer has actually overflown.
 *      However to keep the implementation generic, this function should only
 *      be used for an indication of overflow and not as a means to get the
 *      number of times the timer has overflowed.
 *
 * Results
 *      Flag indicating whether the flag has overflowed.
 *
 *----------------------------------------------------------------------------*/
static INLINE bool bios_has_timer_overflowed(void)
{
   bios_data_area_t * const bda = bios_get_bda();

   return (bda->timer_overflow != 0);
}

#endif /* !BIOS_H_ */
