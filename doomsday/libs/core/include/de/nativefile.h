/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_NATIVEFILE_H
#define LIBCORE_NATIVEFILE_H

#include "de/libcore.h"
#include "de/bytearrayfile.h"
#include "de/nativepath.h"

#include <fstream>

namespace de {

/**
 * Reads from and writes to files in the native file system. The contents
 * of the native file are available as a byte array.
 *
 * @ingroup fs
 */
class DE_PUBLIC NativeFile : public ByteArrayFile
{
public:
    /**
     * Constructs a NativeFile that accesses a file in the native file system
     * in read-only mode.
     *
     * @param name        Name of the file object.
     * @param nativePath  Path in the native file system to access. Relative
     *                    to the current working directory.
     */
    NativeFile(const String &name, const NativePath &nativePath);

    virtual ~NativeFile();

    String describe() const;
    Block metaId() const;

    void clear();
    void flush();

    /**
     * Returns the native path of the file.
     */
    const NativePath &nativePath() const;

    void setMode(const Flags &newMode);

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, const Byte *values, Size count);

public:
    /**
     * Constructs a native file instance to be used independently, outside of the
     * normal folder structure. This should be used when accessing arbitrary files
     * as requested by the user.
     *
     * The returned file object is not included in file system indices.
     *
     * @param nativePath  Native file path.
     *
     * @return Native file initialized with the current status of the source file.
     * Caller gets ownership.
     */
    static NativeFile *newStandalone(const NativePath &nativePath);

protected:
    /// Close any open streams.
    void close();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif /* LIBCORE_NATIVEFILE_H */
