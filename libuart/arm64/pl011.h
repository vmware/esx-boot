/*******************************************************************************
 * Copyright (c) 2008-2015 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * pl011.h -- PL011 UART definitions.
 */

#ifndef PL011_H
#define PL011_H

#define PL011_DR                   0x00 /*  Data read or written from the interface. */
#define PL011_RSR                  0x04 /*  Receive status register (Read). */
#define PL011_ECR                  0x04 /*  Error clear register (Write). */
#define PL011_FR                   0x18 /*  Flag register (Read only). */
#define PL011_IBRD                 0x24
#define PL011_FBRD                 0x28
#define PL011_LCRH                 0x2C
#define PL011_CR                   0x30
#define PL011_IFLS                 0x34
#define PL011_IMSC                 0x38
#define PL011_RI                   0x3C
#define PL011_MI                   0x40
#define PL011_PERIPH_ID0           0xFE0

#define PL011_RSR_OE               0x08
#define PL011_RSR_BE               0x04
#define PL011_RSR_PE               0x02
#define PL011_RSR_FE               0x01

#define PL011_FR_TXFE              0x80
#define PL011_FR_RXFF              0x40
#define PL011_FR_TXFF              0x20
#define PL011_FR_RXFE              0x10
#define PL011_FR_BUSY              0x08
#define PL011_FR_TMSK              (PL011_FR_TXFF + PL011_FR_BUSY)

#define PL011_IS_OE                (1 << 10)
#define PL011_IS_BE                (1 << 9)
#define PL011_IS_PE                (1 << 8)
#define PL011_IS_FE                (1 << 7)
#define PL011_IS_RT                (1 << 6)
#define PL011_IS_TX                (1 << 5)
#define PL011_IS_RX                (1 << 4)
#define PL011_IS_DSR               (1 << 3)
#define PL011_IS_DCD               (1 << 2)
#define PL011_IS_CTS               (1 << 1)
#define PL011_IS_RI                (1 << 0)

#define PL011_LCRH_SPS             (1 << 7)
#define PL011_LCRH_WLEN_8          (3 << 5)
#define PL011_LCRH_WLEN_7          (2 << 5)
#define PL011_LCRH_WLEN_6          (1 << 5)
#define PL011_LCRH_WLEN_5          (0 << 5)
#define PL011_LCRH_FEN             (1 << 4)
#define PL011_LCRH_STP2            (1 << 3)
#define PL011_LCRH_EPS             (1 << 2)
#define PL011_LCRH_PEN             (1 << 1)
#define PL011_LCRH_BRK             (1 << 0)

#define PL011_CR_CTSEN             (1 << 15)
#define PL011_CR_RTSEN             (1 << 14)
#define PL011_CR_OUT2              (1 << 13)
#define PL011_CR_OUT1              (1 << 12)
#define PL011_CR_RTS               (1 << 11)
#define PL011_CR_DTR               (1 << 10)
#define PL011_CR_RXE               (1 << 9)
#define PL011_CR_TXE               (1 << 8)
#define PL011_CR_LPE               (1 << 7)
#define PL011_CR_IIRLP             (1 << 2)
#define PL011_CR_SIREN             (1 << 1)
#define PL011_CR_UARTEN            (1 << 0)

#define PL011_IMSC_OEIM            (1 << 10)
#define PL011_IMSC_BEIM            (1 << 9)
#define PL011_IMSC_PEIM            (1 << 8)
#define PL011_IMSC_FEIM            (1 << 7)
#define PL011_IMSC_RTIM            (1 << 6)
#define PL011_IMSC_TXIM            (1 << 5)
#define PL011_IMSC_RXIM            (1 << 4)
#define PL011_IMSC_DSRMIM          (1 << 3)
#define PL011_IMSC_DCDMIM          (1 << 2)
#define PL011_IMSC_CTSMIM          (1 << 1)
#define PL011_IMSC_RIMIM           (1 << 0)

#endif /* PL011_H */
