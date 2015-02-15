/** @file mapdef.h  Map asset/resource definition/manifest.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCE_MAPDEF_H
#define LIBDENG_RESOURCE_MAPDEF_H

#include <doomsday/filesys/file.h>
#include <doomsday/filesys/lumpindex.h>
#include <de/PathTree>
#include <de/Record>
#include <de/String>
#include "Game"

/**
 * Definition/manifeset for a map asset/resource.
 *
 * @ingroup resource
 */
class MapDef : public de::PathTree::Node, public de::Record
{
public:
    MapDef(de::PathTree::NodeArgs const &args);

    /**
     * Returns a textual description of the map definition.
     *
     * @return Human-friendly description the map definition.
     */
    de::String description(de::Uri::ComposeAsTextFlags uriCompositionFlags = de::Uri::DefaultComposeAsTextFlags) const;

    /**
     * Returns the URI this resource will be known by.
     */
    inline de::Uri composeUri() const { return de::Uri("Maps", gets("id")); }

    /**
     * Returns the id used to uniquely reference the map in some (old) definitions.
     */
    de::String composeUniqueId(de::Game const &currentGame) const;

    MapDef &setSourceFile(de::File1 *newSourceFile);

    de::File1 *sourceFile() const;

    MapDef &setRecognizer(de::Id1MapRecognizer *newRecognizer);

    de::Id1MapRecognizer const &recognizer() const;

private:
    //String cachePath;
    //bool lastLoadAttemptFailed;
    de::File1 *_sourceFile;
    QScopedPointer<de::Id1MapRecognizer> _recognized;
};

#endif /* LIBDENG_RESOURCE_MAPDEF_H */
