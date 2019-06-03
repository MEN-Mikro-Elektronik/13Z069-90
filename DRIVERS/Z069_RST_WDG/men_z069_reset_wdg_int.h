/***********************  I n c l u d e  -  F i l e  ***********************/
/**
 *         \file men_z069_reset_wdg_int.h
 *
 *       \author aw
 *
 *  	 \brief  Internal header file of reset LINUX native driver
 *
 *     Switches: MAC_MEM_MAPPED / MAC_IO_MAPPED;
 *               MAC_BYTESWAP
 */
/*
 *---------------------------------------------------------------------------
 * Copyright (c) 2008-2019, MEN Mikro Elektronik GmbH
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

#ifndef MEN_Z069_RESET_WDG_INT_H
#define MEN_Z069_RESET_WDG_INT_H

#ifdef __cplusplus
	extern "C" {
#endif

#include <MEN/men_z069_reset_wdg.h>

/*--------------------------------------+
|   DEFINES                             |
+--------------------------------------*/

#if !defined(MAC_MEM_MAPPED) && !defined(MAC_IO_MAPPED)
#	define MAC_MEM_MAPPED
#endif

#define MEN_PROC_ROOT_DIR "men"		/**< root dir in /proc for LED controller */

#ifdef MAC_BYTESWAP
#	define RSWAP8(a)	(a)
#	define RSWAP16(a)	GALADR_Swap16(a)
#	define RSWAP32(a)	GALADR_Swap32(a)
#	define WSWAP8(a)	(a)
#	define WSWAP16(a)	GALADR_SWAP16(a)
#	define WSWAP32(a)	GALADR_SWAP32(a)
#else
#	define RSWAP8(a)	(a)
#	define RSWAP16(a)	(a)
#	define RSWAP32(a)	(a)
#	define WSWAP8(a)	(a)
#	define WSWAP16(a)	(a)
#	define WSWAP32(a)	(a)
#endif /* MAC_BYTESWAP */


#ifdef MAC_MEM_MAPPED
typedef volatile void* MACCESS; 	/* access pointer */

#define MREAD_D8(ma,offs)			readb((MACCESS)(ma)+(offs))
#define MREAD_D16(ma,offs)			RSWAP16(readw((MACCESS)(ma)+(offs)))
#define MREAD_D32(ma,offs)			RSWAP32(readl((MACCESS)(ma)+(offs)))

#define MWRITE_D8(ma,offs,val)		writeb(val,(MACCESS)(ma)+(offs))
#define MWRITE_D16(ma,offs,val)		writew(WSWAP16(val),(MACCESS)(ma)+(offs))
#define MWRITE_D32(ma,offs,val)		writel(WSWAP32(val),(MACCESS)(ma)+(offs))

#else
typedef volatile void* MACCESS; 	/* access pointer */

#define MREAD_D8(ma,offs)			inb((MACCESS)(ma)+(offs))
#define MREAD_D16(ma,offs)			RSWAP16(inw((MACCESS)(ma)+(offs)))
#define MREAD_D32(ma,offs)			RSWAP32(inl((MACCESS)(ma)+(offs)))

#define MWRITE_D8(ma,offs,val)		outb(val,(MACCESS)(ma)+(offs))
#define MWRITE_D16(ma,offs,val)		outw(WSWAP16(val),(MACCESS)(ma)+(offs))
#define MWRITE_D32(ma,offs,val)		outl(WSWAP32(val),(MACCESS)(ma)+(offs))

#endif

/*-----------------------------------------+
|  TYPEDEFS                                |
+-----------------------------------------*/

/** structure for holding /procfs/LED handles */
typedef struct {
	struct proc_dir_entry *led_dir;
	struct proc_dir_entry *led0_hdl;
	struct proc_dir_entry *led1_hdl;
	struct proc_dir_entry *led2_hdl;
	struct proc_dir_entry *led3_hdl;
} PROC_LED_HDL;

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
/* none */

#ifdef __cplusplus
	}
#endif

#endif	/* MEN_Z069_RESET_WDG_INT_H */

