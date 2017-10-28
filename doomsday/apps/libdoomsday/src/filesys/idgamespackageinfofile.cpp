/** @file idgamespackageinfofile.cpp
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

#include "doomsday/filesys/idgamespackageinfofile.h"

using namespace de;

DENG2_PIMPL(IdgamesPackageInfoFile)
, DENG2_OBSERVES(Asset, StateChange)
{
    Asset packageAsset;
    AssetGroup assets;
    SafePtr<RemoteFile const> dataFile;
    SafePtr<RemoteFile const> descriptionFile;

    Impl(Public *i) : Base(i)
    {
        assets.audienceForStateChange() += this;
    }

    void assetStateChanged(Asset &)
    {
        if (!assets.isEmpty() && assets.isReady())
        {
            // Looks like we can process the file contents.
            qDebug() << "[IdgamesPackageInfoFile] Time to unzip and analyze!";

            // The package is now ready for loading.
            packageAsset.setState(Asset::Ready);
        }
    }
};

IdgamesPackageInfoFile::IdgamesPackageInfoFile(String const &name)
    : File(name)
    , d(new Impl(this))
{}

void IdgamesPackageInfoFile::setSourceFiles(RemoteFile const &dataFile,
                                            RemoteFile const &descriptionFile)
{
    d->dataFile       .reset(&dataFile);
    d->descriptionFile.reset(&descriptionFile);

    d->assets += dataFile.asset();
    d->assets += descriptionFile.asset();
}

Asset &IdgamesPackageInfoFile::asset()
{
    return d->packageAsset;
}

Asset const &IdgamesPackageInfoFile::asset() const
{
    return d->packageAsset;
}

dsize IdgamesPackageInfoFile::downloadSize() const
{
    // No additional download for the info file is required.
    return 0;
}

void IdgamesPackageInfoFile::download()
{}

void IdgamesPackageInfoFile::cancelDownload()
{}

IIStream const &IdgamesPackageInfoFile::operator >> (IByteArray &bytes) const
{

    return *this;
}
