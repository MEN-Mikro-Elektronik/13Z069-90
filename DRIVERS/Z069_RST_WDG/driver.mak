#**************************  M a k e f i l e ********************************
#  
#         Author: aw/ts
#          $Date: 2010/12/03 19:23:03 $
#      $Revision: 1.3 $
#  
#    Description: makefile descriptor for z069 reset/wdg kernel module
#                      
#---------------------------------[ History ]---------------------------------
#
# $Log: driver.mak,v $
# Revision 1.3  2010/12/03 19:23:03  ts
# R: cosmetics, log footer was missing
# M: added log macro
#
# Revision 1.2  25.07.2008 10:15:09 by aw
# R: driver supports not only reset functionality
# M: rename driver to z069_reset_wdg
#
# Revision 1.1  14.07.2008 10:13:04 by aw
# Initial Revision
#-----------------------------------------------------------------------------
#   (c) Copyright 2008 by MEN mikro elektronik GmbH, Nuernberg, Germany 
#*****************************************************************************

MAK_NAME=lx_z69

MAK_LIBS=

MAK_SWITCH=$(SW_PREFIX)Z069_RST_WDG_BIT=0 \
		   $(SW_PREFIX)MAC_MEM_MAPPED

MAK_INCL=

MAK_INP1=men_z069_reset_wdg$(INP_SUFFIX)

MAK_INP=$(MAK_INP1)

