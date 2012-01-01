/**\file p_svtexarc.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_MATERIALARCHIVE_H
#define LIBCOMMON_MATERIALARCHIVE_H

typedef struct {
    int version;

    uint count;
    struct materialarchive_record_s* table;

    /// Used with older versions.
    uint numFlats;
} materialarchive_t;

typedef unsigned short materialarchive_serialid_t;

materialarchive_t* P_CreateMaterialArchive(void);
materialarchive_t* P_CreateEmptyMaterialArchive(void);
void P_DestroyMaterialArchive(materialarchive_t* materialArchive);

materialarchive_serialid_t MaterialArchive_FindUniqueSerialId(materialarchive_t* mArc, material_t* mat);

material_t* MaterialArchive_Find(materialarchive_t* mArc, materialarchive_serialid_t serialId, int group);

void MaterialArchive_Write(materialarchive_t* mArc);
void MaterialArchive_Read(materialarchive_t* mArc, int version);

#if _DEBUG
void MaterialArchive_Print(const materialarchive_t* mArc);
#endif

#endif /* LIBCOMMON_MATERIALARCHIVE_H */
