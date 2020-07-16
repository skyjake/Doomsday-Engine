/**
 * @file filehandle.h
 * Reference/handle to a unique file in FS1.
 * @ingroup fs
 *
 * @deprecated Should use FS2 instead for file access.
 *
 * @author Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_FILESYS_FILEHANDLE_H
#define DE_FILESYS_FILEHANDLE_H

#include "../libdoomsday.h"
#include <de/libcore.h>
#include <de/list.h>
#include <de/legacy/types.h>
#include <stdio.h>

namespace res {

class File1;
class FileHandle;

/// Seek methods
enum SeekMethod
{
    SeekSet = 0,
    SeekCur = 1,
    SeekEnd = 2
};

/// List of file search results.
typedef de::List<FileHandle *> FileList;

/**
 * Reference/handle to a unique file in the engine's virtual file system.
 */
class LIBDOOMSDAY_PUBLIC FileHandle
{
public:
    ~FileHandle();

    /**
     * Close the file if open. Note that this clears any previously buffered data.
     */
    FileHandle &close();

    /// @todo Should not be visible outside the engine.
    FileList *list();

    /// @todo Should not be visible outside the engine.
    FileHandle &setList(FileList *list);

    bool hasFile() const;

    File1 &file();
    File1 &file() const;

    /// @return  @c true iff this handle's internal state is valid.
    bool isValid() const;

    /// @return  The length of the file, in bytes.
    size_t length();

    /// @return  Offset in bytes from the start of the file to begin read.
    size_t baseOffset() const;

    /**
     * @return  Number of bytes read (at most @a count bytes will be read).
     */
    size_t read(uint8_t *buffer, size_t count);

    /**
     * Read a character from the stream, advancing the read position in the process.
     */
    unsigned char getC();

    /**
     * @return  @c true iff the stream has reached the end of the file.
     */
    bool atEnd();

    /**
     * @return  Current position in the stream as an offset from the beginning of the file.
     */
    size_t tell();

    /**
     * @return  The current position in the file, before the move, as an
     *      offset from the beginning of the file.
     */
    size_t seek(size_t offset, SeekMethod whence);

    /**
     * Rewind the stream to the start of the file.
     */
    FileHandle &rewind();

public:
    /**
     * Create a new handle on the File @a file.
     *
     * @param file  The file being opened.
     */
    static FileHandle *fromFile(File1 &file);

    /**
     * Create a new handle on @a lump.
     *
     * @param lump  The lump to be opened.
     * @param dontBuffer  @c true= do not buffer a copy of the lump.
     */
    static FileHandle *fromLump(File1 &lump, bool dontBuffer = false);

    /**
     * Create a new handle on the specified native file.
     *
     * @param nativeFile  Native file system handle to the file being opened.
     * @param baseOffset  Offset from the start of the file in bytes to begin.
     */
    static FileHandle *fromNativeFile(FILE &nativeFile, size_t baseOffset);

private:
    FileHandle();

    DE_PRIVATE(d)
};

} // namespace res

#endif /* DE_FILESYS_FILEHANDLE_H */
