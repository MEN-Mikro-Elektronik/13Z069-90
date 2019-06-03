/***********************  I n c l u d e  -  F i l e  ***********************/
/**
 *         \file 16z069_rst.h
 *
 *       \author rt
 *
 *        \brief Header file for reset controller in Chameleon FPGA
 *
 * Covers IP core 16Z069_RST
 *
 *     Switches: -
 */
/*
 *---------------------------------------------------------------------------
 * Copyright (c) 2007-2019, MEN Mikro Elektronik GmbH
 ****************************************************************************/
/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _Z069_RST_H
#define _Z069_RST_H

#ifdef __cplusplus
	extern "C" {
#endif

/*-----------------------------------------+
|  TYPEDEFS                                |
+-----------------------------------------*/
#ifdef MENMON
/* watchdog object */
typedef struct {
	MM_WDOG std;
	MACCESS ma;
	u_int16 trigValue;			/**< value to write next into WDVR */
} MMCHAM_16Z069WDOG;

/* reset controller object */
struct MMCHAM_16Z069RST_S;
typedef struct MMCHAM_16Z069RST_S MMCHAM_16Z069RST;

struct MMCHAM_16Z069RST_S
{
	MM_OBJECT mmObj;

	/*---------------+
	|  METHODS       |
	+---------------*/
    /**********************************************************************/
    /** destroy object
	 */
	void (*destroy)( MMCHAM_16Z069RST** thisP );
    /**********************************************************************/
    /** request reset
	 */
	void (*reset)( MMCHAM_16Z069RST* this, u_int16 mask );
    /**********************************************************************/
    /** get reset cause (bitmask)
	 */
	u_int16 (*getResetCause)( MMCHAM_16Z069RST* this, int clearRstCause );
	/**********************************************************************/
	/**	attach system unit's watchdog to MENMON	watchdog dispatcher
	 */
	int (*attachWdog)( MMCHAM_16Z069RST* this );
	/**********************************************************************/
	/**	detach system unit's watchdog
	 */
	void (*detachWdog)( MMCHAM_16Z069RST* this );

	/*---------------+
	|  PRIVATE DATA  |
	+---------------*/
	CHAMELEON_HANDLE* _chah;		/**< chameleon handle  */
	MACCESS _ma;					/**< fpga unit base address */
	MMCHAM_16Z069WDOG* _wdog;		/**< watchdog object */
};

/*-----------------------------------------+
|  PROTOTYPS                               |
+-----------------------------------------*/
/**********************************************************************/
/** Create reset controller object
*/
MMCHAM_16Z069RST* MMCHAM_16z069RstCreate( u_int32 busNo, u_int32 devNo );
#endif /* MENMON */

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/

/* register offsets */

#define Z069_RST_RCR		0x00	/**< Reset Cause Register (rwc) */
#define Z069_RST_RMR		0x04	/**< Reset Mask Register (rw) */
#define Z069_RST_RRR		0x08	/**< Reset Request Register (rw) */
#define Z069_RST_WTR		0x10	/**< Watchdog Timer Register (rw) */
#define Z069_RST_WVR		0x14	/**< Watchdog Value Register (rw) */

/* bits in WTR */
#define Z069_RST_WTR_WDEN		0x8000	/**< Watchdog enable */
#define Z069_RST_WTR_WDET_MASK	0x7FFF	/**< Watchdog expiration time mask */

/* bits in WVR */
#define Z069_RST_WVR_TRIG_VAL	0x5555	/**< Init watchdog trigger value */

/* watchdog timing */
#define Z069_WDOG_FREQ		500	/**< timer counts at this frequency (HZ) */
#define Z069WDOG_SHORT_TOUT 20	/**< timeout in WDOG_STATE_SHORT_TOUT (1/10s)*/

/* MEN specific IOCTL codes */
#define	Z069_WDT_IOCTL_BASE	'M'

#define RSTIOC_SET_RESET_MASK       	_IOW(Z069_WDT_IOCTL_BASE, 1, int)
#define RSTIOC_GET_RESET_MASK       	_IOR(Z069_WDT_IOCTL_BASE, 2, int)
#define RSTIOC_SET_RESET_CAUSE      	_IOW(Z069_WDT_IOCTL_BASE, 3, int)
#define RSTIOC_GET_RESET_CAUSE      	_IOR(Z069_WDT_IOCTL_BASE, 4, int)
#define RSTIOC_SET_RESET_REQUEST    	_IOW(Z069_WDT_IOCTL_BASE, 5, int)
#define RSTIOC_GET_RESET_REQUEST    	_IOR(Z069_WDT_IOCTL_BASE, 6, int)

#ifdef __cplusplus
	}
#endif

#endif	/* _Z069_RST_H */


