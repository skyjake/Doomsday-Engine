/**
 * @file filehandle.h
 * Reference/handle to a unique file in the engine's virtual file system.
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_FILEHANDLE_H
#define LIBDENG_FILESYS_FILEHANDLE_H

#include <de/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Seek methods
typedef enum {
    SeekSet = 0,
    SeekCur = 1,
    SeekEnd = 2
} SeekMethod;

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

struct filelist_s;

namespace de {

/// @todo Should not be visible outside the engine.
class File1;
class FileHandleBuilder;

/**
 * Reference/handle to a unique file in the engine's virtual file system.
 */
class FileHandle
{
public:
    ~FileHandle();

    /**
     * Close the file if open. Note that this clears any previously buffered data.
     */
    FileHandle& close();

    /// @todo Should not be visible outside the engine.
    struct filelist_s* list();

    /// @todo Should not be visible outside the engine.
    FileHandle& setList(struct filelist_s* list);

    /// @todo Should not be visible outside the engine.
    bool hasFile() const;

    /// @todo Should not be visible outside the engine.
    File1& file();
    File1& file() const;

    /// @return  @c true iff this handle's internal state is valid.
    bool isValid() const;

    /// @return  The length of the file, in bytes.
    size_t length();

    /// @return  Offset in bytes from the start of the file to begin read.
    size_t baseOffset() const;

    /**
     * @return  Number of bytes read (at most @a count bytes will be read).
     */
    size_t read(uint8_t* buffer, size_t count);

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
    FileHandle& rewind();

    friend class FileHandleBuilder;

private:
    FileHandle();

    struct Instance;
    Instance* d;
};

} // namespace de

extern "C" {
#endif // __cplusplus

/**
 * C wrapper API:
 */

struct filehandle_s; // The filehandle instance (opaque).
typedef struct filehandle_s FileHandle;

void FileHandle_Close(FileHandle* hndl);

boolean FileHandle_IsValid(FileHandle const* hndl);

size_t FileHandle_Length(FileHandle* hndl);

size_t FileHandle_BaseOffset(FileHandle const* hndl);

size_t FileHandle_Read(FileHandle* hndl, uint8_t* buffer, size_t count);

unsigned char FileHandle_GetC(FileHandle* hndl);

boolean FileHandle_AtEnd(FileHandle* hndl);

size_t FileHandle_Tell(FileHandle* hndl);

size_t FileHandle_Seek(FileHandle* hndl, size_t offset, SeekMethod whence);

void FileHandle_Rewind(FileHandle* hndl);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_FILEHANDLE_H */
