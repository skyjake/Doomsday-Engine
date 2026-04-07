/**
 * @file de/unittest.h
 * Macros for unit testing.
 *
 * @authors Copyright &copy; 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_UNIT_TEST_H
#define DE_UNIT_TEST_H

/// @addtogroup legacy
/// @{

#ifdef _DEBUG
#  define DE_RUN_UNITTEST(Name)    int testResult_##Name = UNITTEST_##Name();
#else
#  define DE_RUN_UNITTEST(Name)
#endif

#define DE_DEFINE_UNITTEST(Name)   static int UNITTEST_##Name(void)

/// @}

#endif // DE_UNIT_TEST_H
