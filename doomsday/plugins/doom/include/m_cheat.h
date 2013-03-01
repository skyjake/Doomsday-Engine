/**
 * @file m_cheat.h
 * Cheats - Doom (plus Chex and Hacx). @ingroup libdoom
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDOOM_M_CHEAT_H
#define LIBDOOM_M_CHEAT_H

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

/**
 * Register event sequence callbacks for all cheats.
 */
void G_RegisterCheats(void);

#endif // LIBDOOM_M_CHEAT_H
