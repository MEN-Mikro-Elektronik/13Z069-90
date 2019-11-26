/***********************  I n c l u d e  -  F i l e  ***********************/
/**
 *         \file men_z069_reset_wdg.h
 *
 *  	 \brief  Native header file of z069 LINUX native driver
 *
 *     Switches:
 *
 *---------------------------------------------------------------------------
 * Copyright 2018-2019, MEN Mikro Elektronik GmbH
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
#define Z069_REG_SIZE 			0x20
#define Z069_WDT_COUNTER_MAX 0x7fff             /**< max. value of watchdog counter */
#define Z069_WDT_COUNTER_MIN 1                  /**< min. value of watchdog counter */
#define Z069_WDT_TIMER_FREQUENZ (500)           /**< Timer frequency [Hz] of watchdog counter */

int z069_SetResetMask( u_int32 );
int z069_GetResetMask( u_int32 * );
int z069_SetResetCause( u_int32 );
int z069_GetResetCause( u_int32 * );
int z069_SetResetRequest( u_int32 );
int z069_GetResetRequest( u_int32 * );

#ifdef __cplusplus
	}
#endif

#endif	/* _MEN_Z069_RESET_WDG_H */

