/** @file packageformatter.h  Abstract base for Doomsday-native .save package formatters.
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

#ifndef SAVEGAMETOOL_PACKAGEFORMATER_H
#define SAVEGAMETOOL_PACKAGEFORMATER_H

#include <de/string.h>
#include <de/error.h>
#include <de/block.h>
#include <de/path.h>
#include <de/string.h>
#include <doomsday/gamestatefolder.h>

/**
 * Base class for .save package formatters.
 */
class PackageFormatter
{
public:
    /// An error occured when attempting to open the source file. @ingroup errors
    DE_ERROR(FileOpenError);

    /// Base class for read-related errors. @ingroup errors
    DE_ERROR(ReadError);

    /// The source file format is unknown/unsupported. @ingroup errors
    DE_SUB_ERROR(ReadError, UnknownFormatError);

    de::StringList knownExtensions;
    de::StringList baseGameIds;

public:
    /**
     * @param knownExtensions  List of known file extensions for the format.
     * @param baseGameIdKeys   List of supported base game IDs for the format.
     */
    PackageFormatter(de::StringList knownExtensions, de::StringList baseGameIds);

    virtual ~PackageFormatter();

    /**
     * Formats .save package Info.
     *
     * @param metadata        Session metadata to be formatted.
     * @param sourceFile      Path to the original source file being reformatted.
     * @param oldSaveVersion  Original save format version.
     *
     * @return  Formated Info data.
     */
    de::String composeInfo(const GameStateMetadata &metadata, const de::Path &sourceFile,
                           de::dint32 oldSaveVersion) const;

    /**
     * Formats .save map state headers.
     *
     * @param magic        Native "magic" identifier, used for format recognition.
     * @param saveVersion  Save format Version.
     *
     * @return  Prepared map state header block.
     */
    de::Block *composeMapStateHeader(de::dint32 magic, de::dint32 saveVersion) const;

    /**
     * Returns the textual name for the format, used for log/error messages.
     */
    virtual de::String formatName() const = 0;

    /**
     * Attempt to recognize the format of the given file.
     *
     * @param path  Path to the file to be recognized.
     */
    virtual bool recognize(de::Path path) = 0;

    /**
     * @param savePath  Output file path.
     */
    virtual void convert(de::Path savePath) = 0;
};

#endif // SAVEGAMETOOL_PACKAGEFORMATER_H
