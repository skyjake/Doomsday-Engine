/**
 * @file lumpfile.h
 * Specialization of AbstractFile for working with the lumps of containers
 * objects such as WadFile and ZipFile.
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

#ifndef LIBDENG_FILESYS_LUMPFILE_H
#define LIBDENG_FILESYS_LUMPFILE_H

#include "lumpinfo.h"
#include "abstractfile.h"

#ifdef __cplusplus
namespace de {

class LumpDirectory;

/**
 * LumpFile. Runtime representation of a lump-file for use with LumpDirectory
 */
class LumpFile : public AbstractFile
{
public:
    LumpFile(DFile& file, char const* path, LumpInfo const& info);
    ~LumpFile();

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

struct lumpfile_s; // The lumpfile instance (opaque)
typedef struct lumpfile_s LumpFile;

/**
 * Constructs a new LumpFile instance which must be destroyed with LumpFile_Delete()
 * once it is no longer needed.
 *
 * @param file      Virtual file handle to the underlying file resource.
 * @param path      Virtual file system path to associate with the resultant LumpFile.
 * @param info      File info descriptor for the resultant LumpFile. A copy is made.
 */
LumpFile* LumpFile_New(DFile* file, char const* path, LumpInfo const* info);

/**
 * Destroy LumpFile instance @a lump.
 */
void LumpFile_Delete(LumpFile* lump);

LumpInfo const* LumpFile_LumpInfo(LumpFile* lump, int lumpIdx);

int LumpFile_LumpCount(LumpFile* lump);

int LumpFile_PublishLumpsToDirectory(LumpFile* lump, struct lumpdirectory_s* directory);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_LUMPFILE_H */
