#**************************  M a k e f i l e ********************************
#  
#         Author: aw/ts
#          $Date: 2010/12/03 19:23:03 $
#      $Revision: 1.3 $
#  
#    Description: makefile descriptor for z069 reset/wdg kernel module
#                      
#-----------------------------------------------------------------------------
#   Copyright (c) 2008-2019, MEN Mikro Elektronik GmbH
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

MAK_NAME=lx_z69

MAK_LIBS=

MAK_SWITCH=$(SW_PREFIX)Z069_RST_WDG_BIT=0 \
		   $(SW_PREFIX)MAC_MEM_MAPPED

MAK_INCL=

MAK_INP1=men_z069_reset_wdg$(INP_SUFFIX)

MAK_INP=$(MAK_INP1)

