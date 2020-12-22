/** @file extension.cpp  Binary extension components.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/extension.h"

#include <map>

namespace de {

struct KeyCompare {
    bool operator()(const char *a, const char *b) const { return iCmpStr(a, b) < 0; }
};

using Extensions = std::map<const char *, void *(*)(const char *), KeyCompare>;

static Extensions &extMap()
{
    static Extensions ext;
    return ext;
}

void registerExtension(const char *name, void *(*getProcAddress)(const char *))
{
    extMap()[name] = getProcAddress;
}

bool isExtensionRegistered(const char *name)
{
    return extMap().find(name) != extMap().end();
}

StringList extensions()
{
    return map<StringList>(extMap(), [](const Extensions::value_type &i) {
        return i.first;
    });
}

void *extensionSymbol(const char *extensionName, const char *symbolName)
{
    const auto &ext = extMap();
    auto e = ext.find(extensionName);
    DE_ASSERT(e != ext.end());
    return e->second(symbolName);
}

} // namespace de
