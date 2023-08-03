/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * List of system registers definitions, using the following format:
 * _SYSREG( name, op0, op1, crn, crm, op2 )
 */

_SYSREG( CurrentEL,         3, 0,  4,  2, 2 )

/* HCR_EL2.MIDR */
_SYSREG( MIDR_EL1,          3, 0,  0,  0, 0 )

/* HCR_EL2.MPIDR */
_SYSREG( MPIDR_EL1,         3, 0,  0,  0, 5 )

/* HCR_EL2.REVIDR */
_SYSREG( REVIDR_EL1,        3, 0,  0,  0, 6 )
_SYSREG( ID_AA64PFR0_EL1,   3, 0,  0,  4, 0 )
_SYSREG( ID_AA64PFR1_EL1,   3, 0,  0,  4, 1 )
_SYSREG( ID_AA64ZFR0_EL1,   3, 0,  0,  4, 4 )
_SYSREG( ID_AA64DFR0_EL1,   3, 0,  0,  5, 0 )
_SYSREG( ID_AA64DFR1_EL1,   3, 0,  0,  5, 1 )
_SYSREG( ID_AA64AFR0_EL1,   3, 0,  0,  5, 4 )
_SYSREG( ID_AA64AFR1_EL1,   3, 0,  0,  5, 5 )
_SYSREG( ID_AA64ISAR0_EL1,  3, 0,  0,  6, 0 )
_SYSREG( ID_AA64ISAR1_EL1,  3, 0,  0,  6, 1 )
_SYSREG( ID_AA64ISAR2_EL1,  3, 0,  0,  6, 2 )
_SYSREG( ID_AA64MMFR0_EL1,  3, 0,  0,  7, 0 )
_SYSREG( ID_AA64MMFR1_EL1,  3, 0,  0,  7, 1 )
_SYSREG( ID_AA64MMFR2_EL1,  3, 0,  0,  7, 2 )
