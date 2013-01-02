/**
 * @file dd_wad.h
 * Wrapper API for accessing data stored in DOOM WAD files.
 *
 * All data is read through Doomsday's virtual file system (see @ref fs).
 *
 * @ingroup resource
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 1993-1996 by id Software, Inc.
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

#ifndef LIBDENG_API_WAD_H
#define LIBDENG_API_WAD_H

#include "de_api.h"
#include "dd_share.h"

typedef struct de_api_wad_s // v1
{
    de_api_t api;

    size_t (*LumpLength)(lumpnum_t lumpNum);

    /// @return  Name of the lump associated with @a lumpNum.
    AutoStr* (*LumpName)(lumpnum_t lumpNum);

    /// @return  "Last modified" timestamp of the zip entry.
    uint (*LumpLastModified)(lumpnum_t lumpNum);

    /// @return  Name of the WAD file where the data associated with @a lumpNum resides.
    ///     Always returns a valid filename (or an empty string).
    AutoStr* (*LumpSourceFile)(lumpnum_t lumpNum);

    /// @return  @c true iff the data associated with @a lumpNum does not originate from the current game.
    boolean (*LumpIsCustom)(lumpnum_t lumpNum);

    /**
     * @param name      Name of the lump to search for.
     * @param silent    Do not print results to the console.
     *
     * @return  Unique index of the found lump in the primary lump directory else @c -1 if not found.
     */
    lumpnum_t (*CheckLumpNumForName2)(char const* name, boolean silent);

    lumpnum_t (*CheckLumpNumForName)(char const* name);

    /// @note As per W_CheckLumpNumForName but results in a fatal error if not found.
    lumpnum_t (*GetLumpNumForName)(char const* name);

    /**
     * Read the data associated with @a lumpNum into @a buffer.
     *
     * @param lumpNum   Logical lump index associated with the data being read.
     * @param buffer    Buffer to read into. Must be at least W_LumpLength() bytes.
     *
     * @return  Number of bytes read.
     */
    size_t (*ReadLump)(lumpnum_t lumpNum, uint8_t* buffer);

    /**
     * Read a subsection of the data associated with @a lumpNum into @a buffer.
     *
     * @param lumpNum       Logical lump index associated with the data being read.
     * @param buffer        Buffer to read into. Must be at least W_LumpLength() bytes.
     * @param startOffset   Offset from the beginning of the lump to start reading.
     * @param length        Number of bytes to be read.
     *
     * @return  Number of bytes read.
     */
    size_t (*ReadLumpSection)(lumpnum_t lumpNum, uint8_t* buffer, size_t startOffset, size_t length);

    /**
     * Read the data associated with @a lumpNum into the cache.
     *
     * @param lumpNum  Logical lump index associated with the data being read.
     *
     * @return  Ptr to the cached copy of the associated data.
     */
    uint8_t const* (*CacheLump)(lumpnum_t lumpNum);

    /**
     * Remove a lock on a cached data lump associated with @a lumpNum.
     *
     * @param lumpNum  Logical lump index associated with the data.
     */
    void (*UnlockLump)(lumpnum_t lumpNum);

} de_api_wad_t;

// Macros for accessing exported functions.
#ifndef DENG_NO_API_MACROS_WAD
#define W_LumpLength                _api_W.LumpLength
#define W_LumpName                  _api_W.LumpName
#define W_LumpLastModified          _api_W.LumpLastModified
#define W_LumpSourceFile            _api_W.LumpSourceFile
#define W_LumpIsCustom              _api_W.LumpIsCustom
#define W_CheckLumpNumForName2      _api_W.CheckLumpNumForName2
#define W_CheckLumpNumForName       _api_W.CheckLumpNumForName
#define W_GetLumpNumForName         _api_W.GetLumpNumForName
#define W_ReadLump                  _api_W.ReadLump
#define W_ReadLumpSection           _api_W.ReadLumpSection
#define W_CacheLump                 _api_W.CacheLump
#define W_UnlockLump                _api_W.UnlockLump
#endif

// Internal access.
#ifdef __DOOMSDAY__
DENG_USING_API(wad, W);
#endif

#endif /* LIBDENG_API_WAD_H */
