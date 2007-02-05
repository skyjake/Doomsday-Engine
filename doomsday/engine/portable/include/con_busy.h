/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * con_busy.h: Console Busy Mode
 */

#ifndef __DOOMSDAY_CONSOLE_BUSY_H__
#define __DOOMSDAY_CONSOLE_BUSY_H__

typedef int     (C_DECL *busyworkerfunc_t) (void* parm);

int         Con_Busy(int flags, busyworkerfunc_t worker, void *workerData);
void        Con_BusyWorkerEnd(void);
boolean     Con_IsBusy(void);

#endif
