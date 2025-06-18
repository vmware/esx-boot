/*******************************************************************************
 * Copyright (c) 2008-2015 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 *****************************************************************************/

/*
 * ns16550.h -- NS 16550-type UART definitions.
 */

#ifndef NS16550_H
#define NS16550_H

#define NS16550_RX               0         /* In:  Receive buffer (DLAB=0) */
#define NS16550_TX               0         /* Out: Transmit buffer (DLAB=0) */
#define NS16550_DLL              0         /* Out: Divisor Latch Low (DLAB=1) */
#define NS16550_DLM              1         /* Out: Divisor Latch High (DLAB=1) */
#define NS16550_IER              1         /* Out: Interrupt Enable Register */
#define NS16550_FCR              2         /* Out: FIFO Control Register */
#define NS16550_LCR              3         /* Out: Line Control Register */
#define NS16550_MCR              4         /* Out: Modem Control Register */
#define NS16550_LSR              5         /* In:  Line Status Register */
#define NS16550_MSR              6         /* In:  Modem Status Register */

#define NS16550_FCR_ENABLE_FIFO  (1 << 0)  /* Enable the FIFO */
#define NS16550_FCR_CLEAR_RCVR   (1 << 1)  /* Clear the RCVR FIFO */
#define NS16550_FCR_CLEAR_XMIT   (1 << 2)  /* Clear the XMIT FIFO */
#define NS16550_FCR_TRIGGER_1    0x00      /* Mask for trigger set at 1 */

#define NS16550_LCR_WLEN8        0x03      /* Word length: 8 bits */
#define NS16550_LCR_SBC          (1 << 6)  /* Set break control */
#define NS16550_LCR_DLAB         (1 << 7)  /* Divisor latch access bit */

#define NS16550_MCR_DTR          (1 << 0)  /* DTR complement */
#define NS16550_MCR_RTS          (1 << 1)  /* RTS complement */
#define NS16550_MCR_OUT2         (1 << 3)  /* Out2 complement */

#define NS16550_MSR_CTS          (1 << 4)  /* Clear to Send */
#define NS16550_MSR_DCD          (1 << 7)  /* Data Carrier Detect */

#define NS16550_LSR_THRE         (1 << 5)  /* Transmit-hold-register empty */

#endif /* NS16550_H */
