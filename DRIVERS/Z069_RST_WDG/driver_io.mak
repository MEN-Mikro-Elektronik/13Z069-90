#**************************  M a k e f i l e ********************************
#  
#         Author: ts
#          $Date: 2010/12/03 19:23:38 $
#      $Revision: 1.1 $
#  
#    Description: makefile descriptor for z069 reset/wdg kernel module,
#                 IO mapped as in F11S and other X86 cards
#                      
#---------------------------------[ History ]---------------------------------
#
#
#-----------------------------------------------------------------------------
#   (c) Copyright 2008 by MEN mikro elektronik GmbH, Nuernberg, Germany 
#*****************************************************************************

MAK_NAME=lx_z069

MAK_LIBS=

MAK_SWITCH=$(SW_PREFIX)Z069_RST_WDG_BIT=0 \
		   $(SW_PREFIX)MAC_IO_MAPPED 

MAK_INCL=

MAK_INP1=men_z069_reset_wdg$(INP_SUFFIX)

MAK_INP=$(MAK_INP1)
