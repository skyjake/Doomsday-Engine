/**
 * @file genericfile.h
 *
 * Generic implementation of AbstractFile, for generic/unknown files.
 *
 * @ingroup fs
 *
 * @see abstractfile.h, AbstractFile
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_GENERICFILE_H
#define LIBDENG_FILESYS_GENERICFILE_H

#include "lumpinfo.h"
#include "abstractfile.h"

#ifdef __cplusplus
namespace de {

class LumpDirectory;

/**
 * GenericFile. Runtime representation of a generic/unknown file for use with LumpDirectory
 */
class GenericFile : public AbstractFile
{
public:
    GenericFile(DFile& file, char const* path, LumpInfo const& info);
    ~GenericFile();

    /// @return Number of lumps (always @c =1).
    int lumpCount();

    /**
     * Lookup the lump info descriptor for this lump.
     *
     * @param lumpIdx       Ignored. Required argument.
     *
     * @return Found lump info.
     */
    LumpInfo const* lumpInfo(int lumpIdx);

    /**
     * Publish this lump to the end of the specified @a directory.
     *
     * @param directory Directory to publish to.
     *
     * @return Number of lumps published to the directory. Always @c =1
     */
    int publishLumpsToDirectory(LumpDirectory* directory);

private:
    struct Instance;
    Instance* d;
};

} // namespace de

extern "C" {
#endif // __cplusplus

/**
 * C wrapper API:
 */

struct genericfile_s; // The genericfile instance (opaque)
typedef struct genericfile_s GenericFile;

/**
 * Constructs a new GenericFile instance which must be destroyed with GenericFile_Delete()
 * once it is no longer needed.
 *
 * @param hndl      Virtual file handle to the underlying file resource.
 * @param path      Virtual file system path to associate with the resultant GenericFile.
 * @param info      File info descriptor for the resultant GenericFile. A copy is made.
 */
GenericFile* GenericFile_New(DFile* hndl, char const* path, LumpInfo const* info);

/**
 * Destroy GenericFile instance @a file.
 */
void GenericFile_Delete(GenericFile* file);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_GENERICFILE_H */
