/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

/*
 * sys_master.h: Communication with the Master Server
 *
 * The master server maintains a list of running public servers.
 */

#ifndef __DOOMSDAY_SYSTEM_MASTER_H__
#define __DOOMSDAY_SYSTEM_MASTER_H__

#include "dd_share.h"

void            N_MasterInit(void);
void            N_MasterShutdown(void);
void            N_MasterAnnounceServer(boolean isOpen);
void            N_MasterRequestList(void);
int             N_MasterGet(int index, serverinfo_t *info);

#endif
