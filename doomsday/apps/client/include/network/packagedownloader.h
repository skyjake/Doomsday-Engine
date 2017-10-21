/** @file packagedownloader.h  Utility for downloading packages from a remote repository.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_CLIENT_PACKAGEDOWNLOADER_H
#define DENG_CLIENT_PACKAGEDOWNLOADER_H

#include <de/shell/ServerInfo>
#include <de/String>

/**
 * Utility for downloading packages from a remote repository.
 * @ingroup network
 */
class PackageDownloader
{
public:
    PackageDownloader();

    void mountFileRepository(de::shell::ServerInfo const &serverInfo);

    void unmountFileRepository();

    void download(de::StringList packageIds, std::function<void ()> callback);

    de::String fileRepository() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_PACKAGEDOWNLOADER_H
