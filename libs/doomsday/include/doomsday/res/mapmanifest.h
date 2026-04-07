/** @file mapmanifest.h  Manifest for a map resource.
 *
 * @authors Copyright © 2014-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_RESOURCE_MAPMANIFEST_H
#define LIBDOOMSDAY_RESOURCE_MAPMANIFEST_H

#include "../filesys/file.h"
#include "../filesys/lumpindex.h"
#include "doomsday/game.h"
#include "../uri.h"
#include <de/pathtree.h>
#include <de/record.h>
#include <de/string.h>
#include <memory.h>

namespace res {

/**
 * Resource manifest for a map.
 *
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC MapManifest : public de::PathTree::Node, public de::Record
{
public:
    MapManifest(const de::PathTree::NodeArgs &args);

    /**
     * Returns a textual description of the manifest.
     *
     * @return Human-friendly description the manifest.
     */
    de::String description(res::Uri::ComposeAsTextFlags uriCompositionFlags = res::Uri::DefaultComposeAsTextFlags) const;

    /**
     * Returns the URI this resource will be known by.
     */
    inline res::Uri composeUri() const { return res::Uri("Maps", gets("id")); }

    /**
     * Returns the id used to uniquely reference the map in some (old) definitions.
     */
    de::String composeUniqueId(const Game &currentGame) const;

    MapManifest &setSourceFile(File1 *newSourceFile);
    File1 *sourceFile() const;

    MapManifest &setRecognizer(Id1MapRecognizer *newRecognizer);
    const Id1MapRecognizer &recognizer() const;

private:
    //de::String cachePath;
    //bool lastLoadAttemptFailed;
    File1 *_sourceFile;
    std::unique_ptr<Id1MapRecognizer> _recognized;
};

}  // namespace res

#endif  // LIBDOOMSDAY_RESOURCE_MAPMANIFEST_H
