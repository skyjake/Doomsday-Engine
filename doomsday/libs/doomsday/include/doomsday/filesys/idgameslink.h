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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_FILESYS_IDGAMESLINK_H
#define LIBDOOMSDAY_FILESYS_IDGAMESLINK_H

#include <de/filesys/webhostedlink.h>

class IdgamesLink : public de::filesys::WebHostedLink
{
public:
    static de::filesys::Link *construct(const de::String &address);

    de::File *populateRemotePath(const de::String &packageId,
                                 const de::filesys::RepositoryPath &path) const override;

    void parseRepositoryIndex(const de::Block &data) override;

    de::StringList categoryTags() const override;

    de::LoopResult forPackageIds(std::function<de::LoopResult (const de::String &packageId)> func) const override;

    de::String findPackagePath(const de::String &packageId) const override;

protected:
    IdgamesLink(const de::String &address);

    void setFileTree(FileTree *tree) override;

private:
    DE_PRIVATE(d)
};

#endif // LIBDOOMSDAY_FILESYS_IDGAMESLINK_H
