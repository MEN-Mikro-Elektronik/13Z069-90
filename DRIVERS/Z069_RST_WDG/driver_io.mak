#**************************  M a k e f i l e ********************************
#  
#         Author: ts
#  
#    Description: makefile descriptor for z069 reset/wdg kernel module,
#                 IO mapped as in F11S and other X86 cards
#                      
#-----------------------------------------------------------------------------
#   Copyright 2008-2019, MEN Mikro Elektronik GmbH
#*****************************************************************************
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

MAK_NAME=lx_z069
# the next line is updated during the MDIS installation
STAMPED_REVISION="13Z069-90_02_02-2-g8656ad2-dirty_2019-05-30"

DEF_REVISION=MAK_REVISION=$(STAMPED_REVISION)

MAK_LIBS=

MAK_SWITCH=$(SW_PREFIX)Z069_RST_WDG_BIT=0 \
		$(SW_PREFIX)$(DEF_REVISION) \
		   $(SW_PREFIX)MAC_IO_MAPPED 

MAK_INCL=

MAK_INP1=men_z069_reset_wdg$(INP_SUFFIX)

MAK_INP=$(MAK_INP1)
