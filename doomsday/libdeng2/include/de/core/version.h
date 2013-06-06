/**
 * @file version.h
 * Version numbering and labeling for libdeng2.
 *
 * @authors Copyright &copy; 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_VERSION_H
#define LIBDENG2_VERSION_H

#ifdef __cplusplus

#include "../String"
#include "../Time"

namespace de {

/**
 * Version information about libdeng2. The version numbers are defined in
 * libdeng2.pro.
 *
 * @note For the time being, this is separate from the libdeng1/engine version
 * number. libdeng2 versioning starts from 2.0.0. When the project as a whole
 * switches to major version 2, libdeng2 version will be synced with the rest
 * of the project. Also note that unlike libdeng1, there is only ever three
 * components in the version (or four, counting the build number).
 */
class Version
{
public:
    int major;
    int minor;
    int patch;
    int build;
    String label; ///< Informative label, only intended for humans.

    /**
     * Version information about this build.
     */
    Version();

    /**
     * Version information.
     *
     * @param version      Version number in the form "x.y.z".
     * @param buildNumber  Build number.
     */
    Version(String const &version, int buildNumber);

    /**
     * Forms a version string in the form "x.y.z". If a release label is
     * defined, it will be included, too: "x.y.z (label)".
     */
    String base() const;

    /**
     * Forms a version string that includes the build number (unless it is
     * zero).
     */
    String asText() const;

    /**
     * Converts a textual version and updates the Version instance with the
     * values. The version has the following format: (major).(minor).(patch).
     * The release label is never part of the version string.
     *
     * @param version  Version string. Cannot include a label.
     */
    void parseVersionString(String const &version);

    bool operator < (Version const &other) const;

    bool operator == (Version const &other) const;

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
