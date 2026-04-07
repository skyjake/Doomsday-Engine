/** @file remote/nativelink.h
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

#ifndef DE_FILESYS_NATIVELINK_H
#define DE_FILESYS_NATIVELINK_H

#include "link.h"

namespace de { namespace filesys {

/**
 * Link to a native Doomsday remote repository (see RemoteFeedUser on server).
 */
class DE_PUBLIC NativeLink : public Link
{
public:
    static const char *URL_SCHEME;

    static Link *construct(const String &address);

    void         setLocalRoot(const String &rootPath) override;
    PackagePaths locatePackages(const StringList &packageIds) const override;
    LoopResult   forPackageIds(std::function<LoopResult(const String &pkgId)> func) const override;

protected:
    NativeLink(const String &address);

    void wasConnected() override;
    void transmit(const Query &query) override;

private:
    DE_PRIVATE(d)
};

}} // namespace de::filesys

#endif // DE_FILESYS_NATIVELINK_H
