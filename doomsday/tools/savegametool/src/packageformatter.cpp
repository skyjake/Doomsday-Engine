#include <utility>

/** @file packageformatter.cpp  Abstract base for Doomsday-native .save package formatters.
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

#include <de/nativepath.h>
#include <de/time.h>
#include <de/writer.h>
#include "packageformatter.h"
#include "savegametool.h"

using namespace de;

extern String versionText();

PackageFormatter::PackageFormatter(StringList knownExtensions, StringList baseGameIds)
    : knownExtensions(std::move(knownExtensions))
    , baseGameIds(std::move(baseGameIds))
{}

PackageFormatter::~PackageFormatter() // virtual
{}

String PackageFormatter::composeInfo(const GameStateMetadata &metadata,
                                     const Path &             sourceFile,
                                     dint32                   oldSaveVersion) const
{
    String info;

    // Write header and misc info.
    Time now;
    info += "# Doomsday Engine saved game session package.\n#";
    info += "\n# Generator: ";
    info += versionText();
    info += "\n# Generation Date: ";
    info += now.asText();
    info += "\n# Source file: \"";
    info += NativePath(sourceFile).pretty();
    info += "\"";
    info += "\n# Source version: ";
    info += String::asText(oldSaveVersion);

    // Write metadata.
    info += "\n\n";
    info += metadata.asInfo();
    info += "\n";

    return info;
}

Block *PackageFormatter::composeMapStateHeader(dint32 magic, dint32 saveVersion) const
{
    Block *hdr = new Block;
    Writer(*hdr) << magic << saveVersion;
    return hdr;
}
