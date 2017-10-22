/**
 * @file version.h
 * Version numbering and labeling for libcore.
 *
 * @authors Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_VERSION_H
#define LIBDENG2_VERSION_H

#ifdef __cplusplus

#include "../String"
#include "../Time"

namespace de {

/**
 * Version information.
 *
 * The format of a version as text is: "x.y.z-label". x, y and z must be
 * numbers, while the label can be any text string.
 *
 * The build's version is specified using preprocessor defines
 * LIBDENG2_MAJOR_VERSION, LIBDENG2_MINOR_VERSION, LIBDENG2_PATCHLEVEL,
 * LIBDENG2_BUILD_TEXT, LIBDENG2_RELEASE_LABEL, and LIBDENG2_GIT_DESCRIPTION.
 *
 * @ingroup core
 */
class DENG2_PUBLIC Version
{
public:
    int major;
    int minor;
    int patch;
    int build;
    String label;           ///< Informative label, only intended for humans.
    String gitDescription;  ///< Output from "git describe".

    /**
     * Initializes an invalid all-zero version.
     */
    Version();

    Version(int major, int minor, int patch, int buildNumber = 0);

    /**
     * Version information.
     *
     * This constructor sets the Git description to a blank string.
     *
     * @param version      Version number in the form "x.y.z". The label may
     *                     be optionally suffixed after a dash "x.y.z-label".
     * @param buildNumber  Build number.
     */
    Version(String const &version, int buildNumber = 0);

    /**
     * Version information about this build. The version information is hardcoded in the
     * build configuration.
     */
    static Version currentBuild();

    /**
     * Determines if the version is valid, i.e., it contains something other
     * than all zeroes and empty strings.
     */
    bool isValid() const;

    /**
     * Returns a version string in the form "x.y.z". If a release
     * label is defined, it will be included, too: "x.y.z-label".
     */
    String base() const;

    /**
     * Returns a version string in the form "x.y.z".
     */
    String compactNumber() const;

    /**
     * Returns a version string in the form "x.y.z.build".
     */
    String fullNumber() const;

    /**
     * Returns a version string that includes the build number (unless it is
     * zero), in the form "x.y.z-label [#build]".
     */
    String asHumanReadableText() const;

    /**
     * Converts a textual version and updates the Version instance with the
     * values. The version has the following format: (major).(minor).(patch).
     * The release label can be suffixed after a dash: "-label".
     *
     * @param version  Version string.
     */
    void parseVersionString(String const &version);

    bool operator < (Version const &other) const;

    bool operator == (Version const &other) const;

    bool operator != (Version const &other) const {
        return !(*this == other);
    }

    bool operator > (Version const &other) const;

    /**
     * Determines the operating system.
     */
    static String operatingSystem();

    static duint cpuBits();

    static bool isDebugBuild();
};

} // namespace de

#endif // __cplusplus

#endif // LIBDENG2_VERSION_H
