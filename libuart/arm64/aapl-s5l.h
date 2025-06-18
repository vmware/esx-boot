/*******************************************************************************
 * Copyright (c) 2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * aapl-s5l.h -- Definitions for the UART found in Apple Silicon hardware.
 */

#ifndef AAPL_S5L_H
#define AAPL_S5L_H

#define AAPL_S5L_ULCON    0x000 /* Line control */
#define AAPL_S5L_UCON     0x004 /* Control */
#define AAPL_S5L_UFCON    0x008 /* FIFO control */
#define AAPL_S5L_UTRSTAT  0x010 /* TX/RX status */
#define AAPL_S5L_UTXH     0x020 /* Transmit buffer */
#define AAPL_S5L_URXH     0x024 /* Receive buffer */
#define AAPL_S5L_UBRDIV   0x028 /* Baud rate divisor */
#define AAPL_S5L_UFRACVAL 0x02c /* Divisor fractional */

#define AAPL_S5L_UTRSTAT_RX_TIMEOUT        0x8
#define AAPL_S5L_UTRSTAT_TRANSMITTER_EMPTY 0x4
#define AAPL_S5L_UTRSTAT_TX_FIFO_EMPTY     0x2
#define APPL_S5L_UTRSTAT_RX_READY          0x1

#endif /* AAPL_S5L_H */
