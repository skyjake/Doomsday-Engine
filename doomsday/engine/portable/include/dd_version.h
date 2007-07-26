/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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
 * dd_version.h: Version Information
 */

#ifndef __DOOMSDAY_VERSION_INFO_H__
#define __DOOMSDAY_VERSION_INFO_H__

/*
 * Version number rules: (major).(minor).(revision)-(release)
 *
 * Major version will be 1 for now (few things short of a complete
 * rewrite will increase the major version).
 *
 * Minor version increases with important feature releases.
 * NOTE: No extra zeros. Numbering goes from 1 to 9 and continues from
 * 10 like 'normal' numbers.
 *
 * Revision number increases with each small (maintenance) release.
 */

/*
 * Version constants.  The Game module can use DOOMSDAY_VERSION to
 * verify that the engine is new enough.  Don't change
 * DOOMSDAY_VERSION unless you wish to break compatibility.
 */
#define DOOMSDAY_VERSION		10900 // Don't touch; see above.
#define DOOMSDAY_RELEASE_NAME	"svn-trunk-devel"
#define DOOMSDAY_VERSION_TEXT	"1.9."DOOMSDAY_RELEASE_NAME
#define DOOMSDAY_PROJECTURL     "http://sourceforge.net/projects/deng/"

#endif
