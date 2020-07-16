/** @file
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_IDGAMESPACKAGEINFOFILE_H
#define LIBDOOMSDAY_IDGAMESPACKAGEINFOFILE_H

#include <de/file.h>
#include <de/remotefile.h>

/**
 * Generates a Doomsday 2 package metadata for mods downloaded from the idgames archive.
 * @ingroup fs
 */
class IdgamesPackageInfoFile : public de::File, public de::IDownloadable
{
public:
    IdgamesPackageInfoFile(const de::String &name);

    void setSourceFiles(const de::RemoteFile &dataFile, const de::RemoteFile &descriptionFile);

    // Downloadable interface.
    de::Asset &asset() override;
    const de::Asset &asset() const override;
    de::dsize downloadSize() const override;
    void download() override;
    void cancelDownload() override;

    // Stream access.
    const IIStream &operator >> (de::IByteArray &bytes) const override;

private:
    DE_PRIVATE(d)
};

#endif // LIBDOOMSDAY_IDGAMESPACKAGEINFOFILE_H
