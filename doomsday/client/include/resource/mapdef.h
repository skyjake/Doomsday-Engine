/** @file mapdef.h  Map asset/resource definition.
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
#include <de/String>
#include "Game"

/**
 * Definition for a map asset/resource.
 *
 * @ingroup resource
 */
class MapDef : public de::PathTree::Node
{
public:
    MapDef(de::PathTree::NodeArgs const &args)
        : Node(args)
    {}

    MapDef &setId(de::String const &newId) {
        _id = newId;
        return *this;
    }

    de::String const &id() const {
        return _id;
    }

    /**
     * Returns the URI this resource will be known by.
     */
    inline de::Uri composeUri() const { return de::Uri("Maps", _id); }

    /**
     * Returns the id used to uniquely reference the map in some (old) definitions.
     */
    de::String composeUniqueId(de::Game const &currentGame) const {
        return de::String("%1|%2|%3|%4")
                  .arg(id().fileNameWithoutExtension())
                  .arg(sourceFile()->name().fileNameWithoutExtension())
                  .arg(sourceFile()->hasCustom()? "pwad" : "iwad")
                  .arg(currentGame.identityKey())
                  .toLower();
    }

    MapDef &setSourceFile(de::File1 *newSourceFile) {
        _sourceFile = newSourceFile;
        return *this;
    }

    de::File1 *sourceFile() const {
        return _sourceFile;
    }

    MapDef &setRecognizer(de::Id1MapRecognizer *newRecognizer) {
        _recognized.reset(newRecognizer);
        return *this;
    }

    de::Id1MapRecognizer const &recognizer() const {
        DENG2_ASSERT(!_recognized.isNull());
        return *_recognized;
    }

private:
    //String cachePath;
    //bool lastLoadAttemptFailed;
    de::String _id;
    de::File1 *_sourceFile;
    QScopedPointer<de::Id1MapRecognizer> _recognized;
};

#endif /* LIBDENG_RESOURCE_MAPDEF_H */
