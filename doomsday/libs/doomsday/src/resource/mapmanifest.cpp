/** @file mapdef.cpp  Resource manifest for a map.
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

#include "doomsday/res/mapmanifest.h"
#include <de/nativepath.h>

using namespace de;

namespace res {

MapManifest::MapManifest(const PathTree::NodeArgs &args)
    : Node(args), Record(), _sourceFile(nullptr)
{}

String MapManifest::description(res::Uri::ComposeAsTextFlags uriCompositionFlags) const
{
    String info = composeUri().compose(uriCompositionFlags | res::Uri::DecodePath); //,
                                   //( uriCompositionFlags.testFlag(res::Uri::OmitScheme)? -14 : -22 );
    // FIXME: Was used for alignment?
    if (_sourceFile)
    {
        info += Stringf(" " _E(C) "\"%s\"" _E(.),
                               NativePath(sourceFile()->composePath()).pretty().c_str());
    }
    return info;
}

String MapManifest::composeUniqueId(const Game &currentGame) const
{
    return (gets("id").fileNameWithoutExtension() + "|" +
            sourceFile()->name().fileNameWithoutExtension() + "|" +
            (sourceFile()->hasCustom() ? "pwad|" : "iwad|") + currentGame.id())
        .lower();
}

MapManifest &MapManifest::setSourceFile(File1 *newSourceFile)
{
    _sourceFile = newSourceFile;
    return *this;
}

File1 *MapManifest::sourceFile() const
{
    return _sourceFile;
}

MapManifest &MapManifest::setRecognizer(Id1MapRecognizer *newRecognizer)
{
    _recognized.reset(newRecognizer);
    return *this;
}

const Id1MapRecognizer &MapManifest::recognizer() const
{
    DE_ASSERT(_recognized);
    return *_recognized;
}

}  // namespace res
