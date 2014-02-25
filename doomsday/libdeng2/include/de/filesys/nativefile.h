/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_NATIVEFILE_H
#define LIBDENG2_NATIVEFILE_H

#include "../libdeng2.h"
#include "../ByteArrayFile"
#include "../NativePath"

#include <QFile>

namespace de {

/**
 * Reads from and writes to files in the native file system. The contents
 * of the native file are available as a byte array.
 *
 * @ingroup fs
 */
class DENG2_PUBLIC NativeFile : public ByteArrayFile
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
    NativeFile(String const &name, NativePath const &nativePath);

    virtual ~NativeFile();

    String describe() const;

    void clear();
    void flush();

    /**
     * Returns the native path of the file.
     */
    NativePath const &nativePath() const { return _nativePath; }

    void setMode(Flags const &newMode);

    // Implements IByteArray.
    Size size() const;
    void get(Offset at, Byte *values, Size count) const;
    void set(Offset at, Byte const *values, Size count);

protected:
    /// Returns the input stream.
    QFile &input() const;

    /// Returns the output stream.
    QFile &output();

    /// Close any open streams.
    void close();

private:
    /// Path of the native file in the OS file system.
    NativePath _nativePath;

    /// Input stream.
    mutable QFile *_in;

    /// Output stream.
    QFile *_out;
};

} // namespace de

#endif /* LIBDENG2_NATIVEFILE_H */
