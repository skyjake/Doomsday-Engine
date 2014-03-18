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

#ifndef SAVEGAMETOOl_PACKAGEFORMATER_H
#define SAVEGAMETOOl_PACKAGEFORMATER_H

#include <de/Error>
#include <de/Block>
#include <de/game/SavedSession>
#include <de/Path>
#include <de/String>

/**
 * Base class for .save package formatters.
 */
class PackageFormatter
{
public:
    typedef de::game::SavedSession SavedSession;
    typedef de::game::SessionMetadata SessionMetadata;

    /// An error occured when attempting to open the source file. @ingroup errors
    DENG2_ERROR(FileOpenError);

    de::String textualId;
    int magic;
    QStringList knownExtensions;
    QStringList baseGameIdKeys;

public:
    /**
     * @param textualId        Textual identifier for the format, used for log/error messages.
     * @param magic            Native "magic" identifier, used for format recognition.
     * @param knownExtensions  List of known file extensions for the format.
     * @param baseGameIdKeys   List of supported base game identity keys for the format.
     */
    PackageFormatter(de::String textualId, int magic, QStringList knownExtensions,
                     QStringList baseGameIdKeys);

    de::String composeInfo(SessionMetadata const &metadata, de::Path const &sourceFile,
                           de::dint32 oldSaveVersion) const;

    de::Block *composeMapStateHeader(de::dint32 magic, de::dint32 saveVersion) const;

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

#endif // SAVEGAMETOOl_PACKAGEFORMATER_H
