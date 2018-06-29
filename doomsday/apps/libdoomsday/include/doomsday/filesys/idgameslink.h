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

#include <de/filesys/WebHostedLink>

class IdgamesLink : public de::filesys::WebHostedLink
{
public:
    static de::filesys::Link *construct(de::String const &address);

    de::File *populateRemotePath(de::String const &packageId,
                                 de::filesys::RepositoryPath const &path) const override;

    void parseRepositoryIndex(const de::Block &data) override;

    de::StringList categoryTags() const override;

    de::LoopResult forPackageIds(std::function<de::LoopResult (de::String const &packageId)> func) const override;

    de::String findPackagePath(de::String const &packageId) const override;

protected:
    IdgamesLink(de::String const &address);

    void setFileTree(FileTree *tree) override;

private:
    DE_PRIVATE(d)
};

#endif // LIBDOOMSDAY_FILESYS_IDGAMESLINK_H
