/** @file idownloadable.h
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

#ifndef IDOWNLOADABLE_H
#define IDOWNLOADABLE_H

#include "de/asset.h"

namespace de {

/**
 * Interface for downloadable objects.
 * @ingroup fs
 */
class DE_PUBLIC IDownloadable
{
public:
    virtual ~IDownloadable() = default;

    virtual Asset &asset() = 0;

    virtual const Asset &asset() const = 0;

    virtual dsize downloadSize() const = 0;

    virtual void download() = 0;

    virtual void cancelDownload() = 0;

    DE_DEFINE_AUDIENCE(Download, void downloadProgress(IDownloadable &, dsize remainingBytes))
};

} // namespace de

#endif // IDOWNLOADABLE_H
