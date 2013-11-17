/** @file colorpalettes.h  Color palette resource collection.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RESOURCE_COLORPALETTES_H
#define DENG_RESOURCE_COLORPALETTES_H

#include <de/libdeng1.h>

// Maximum number of palette translation tables.
#define NUM_TRANSLATION_CLASSES         3
#define NUM_TRANSLATION_MAPS_PER_CLASS  7
#define NUM_TRANSLATION_TABLES          (NUM_TRANSLATION_CLASSES * NUM_TRANSLATION_MAPS_PER_CLASS)

DENG_EXTERN_C byte *translationTables;

void R_InitTranslationTables();
void R_UpdateTranslationTables();

byte const *R_TranslationTable(int tclass, int tmap);

#endif // DENG_RESOURCE_COLORPALETTES_H
