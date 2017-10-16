/** @file remotefile.h  Remote file.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_REMOTEFILE_H
#define LIBDENG2_REMOTEFILE_H

#include "../LinkFile"
#include "../Asset"

namespace de {

/**
 * File that represents file/data on a remote backend and manages the making of a
 * local copy of the data.
 *
 * RemoteFile provides status information as an Asset.
 */
class DENG2_PUBLIC RemoteFile : public LinkFile, public Asset
{
public:
    /// Data of the file has not yet been fetched. @ingroup errors
    DENG2_ERROR(UnfetchedError);

public:
    RemoteFile(String const &name, String const &remotePath, Block const &remoteMetaId);

    String describe() const override;
    Block  metaId()   const override;

    /**
     * Initiates downloading of the file contents from the remote backend.
     */
    void fetchContents();

    // Implements IByteArray.
    //void get(Offset at, Byte *values, Size count) const override;
    //void set(Offset at, Byte const *values, Size count) override;

    // File streaming.
    IIStream const &operator >> (IByteArray &bytes) const override;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_REMOTEFILE_H
