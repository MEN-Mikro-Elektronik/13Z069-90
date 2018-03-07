/***********************  I n c l u d e  -  F i l e  ***********************/
/**
 *         \file men_z069_reset_wdg.h
 *
 *       \author aw
 *        $Date: 2008/08/27 17:59:46 $
 *    $Revision: 2.2 $
 *
 *  	 \brief  Native header file of z069 LINUX native driver
 *
 *     Switches:
 */
/*-------------------------------[ History ]---------------------------------
 *
 * $Log: men_z069_reset_wdg.h,v $
 * Revision 2.2  2008/08/27 17:59:46  aw
 * R: Watchdog timer frequenz was wrong
 * M: set frequenz to 500 Hz
 *
 * Revision 2.1  2008/07/24 13:06:18  aw
 * Initial Revision
 *
 *
 *---------------------------------------------------------------------------
 * (c) Copyright 2008 by MEN Mikro Elektronik GmbH, Nuremberg, Germany
 ****************************************************************************/

#ifndef _MEN_Z069_RESET_WDG_H
#define _MEN_Z069_RESET_WDG_H

#ifdef __cplusplus
	extern "C" {
#endif

#include <MEN/16z069_rst.h>

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/
#define Z069_RESET_MINOR 200

/* register size used by this driver */
#define Z069_WDG_REGS_SIZE 0x8
#define Z069_REGS_SIZE 0x18
#define Z069_WDT_COUNTER_MAX 0x7fff             /**< max. value of watchdog counter */
#define Z069_WDT_COUNTER_MIN 1                  /**< min. value of watchdog counter */
#define Z069_WDT_TIMER_FREQUENZ (500)           /**< Timer frequency [Hz] of watchdog counter */


#define RSTIOC_SET_RESET_MASK       0x1
#define RSTIOC_GET_RESET_MASK       0x2
#define RSTIOC_SET_RESET_CAUSE      0x3
#define RSTIOC_GET_RESET_CAUSE      0x4
#define RSTIOC_SET_RESET_REQUEST    0x5
#define RSTIOC_GET_RESET_REQUEST    0x6
		
/*-----------------------------------------+
|  TYPEDEFS                                |
+-----------------------------------------*/
/* none */

/*--------------------------------------+
|   EXTERNALS                           |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   GLOBALS                             |
+--------------------------------------*/
/* none */

/*--------------------------------------+
|   PROTOTYPES                          |
+--------------------------------------*/
int z069_SetResetMask( u_int32 * );
int z069_GetResetMask( u_int32 * );
int z069_SetResetCause( u_int32 * );
int z069_GetResetCause( u_int32 * );
int z069_SetResetRequest( u_int32 * );
int z069_GetResetRequest( u_int32 * );


#ifdef __cplusplus
	}
#endif

#endif	/* _MEN_Z069_RESET_WDG_H */

