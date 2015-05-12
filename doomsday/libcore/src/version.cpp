/**
 * @file version.cpp
 * Version numbering and labeling for libcore.
 *
 * @authors Copyright © 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Version"
#include <QStringList>

namespace de {

Version::Version() : build(Time().asBuildNumber())
{
    major = LIBDENG2_MAJOR_VERSION;
    minor = LIBDENG2_MINOR_VERSION;
    patch = LIBDENG2_PATCHLEVEL;

#ifdef LIBDENG2_BUILD_TEXT
    build = String(LIBDENG2_BUILD_TEXT).toInt();
#endif

    label = LIBDENG2_RELEASE_LABEL;
}

Version::Version(String const &version, int buildNumber) : build(buildNumber)
{
    parseVersionString(version);
}

String Version::base() const
{
    String v = String("%1.%2.%3").arg(major).arg(minor).arg(patch);
    if(!label.isEmpty()) v += String(" (%1)").arg(label);
    return v;
}

String Version::asText() const
{
    if(!build) return base();
    return base() + String(" Build %1").arg(build);
}

/**
 * The version is has the following format: (major).(minor).(patch). The
 * release label is never part of the version.
 *
 * @param version  Version string.
 */
void Version::parseVersionString(String const &version)
{
    major = minor = patch = 0;

    QStringList parts = version.split('.');
    if(parts.size() >= 1) major = parts[0].toInt();
    if(parts.size() >= 2) minor = parts[1].toInt();
    if(parts.size() >= 3) patch = parts[2].toInt();
}

bool Version::operator < (Version const &other) const
{
    if(major == other.major)
    {
        if(minor == other.minor)
        {
            if(patch == other.patch)
            {
                return build < other.build;
            }
            return patch < other.patch;
        }
        return minor < other.minor;
    }
    return major < other.major;
}

bool Version::operator == (Version const &other) const
{
    return major == other.major && minor == other.minor &&
           patch == other.patch && build == other.build;
}

bool Version::operator > (Version const &other) const
{
    return !(*this < other || *this == other);
}

String Version::operatingSystem()
{
#ifdef WIN32
    return "windows";
#elif MACOSX
    return "macx";
#else
    return "unix";
#endif
}

duint Version::cpuBits()
{
#ifdef DENG2_64BIT
    return 64;
#else
    return 32;
#endif
}

bool Version::isDebugBuild()
{
#ifdef DENG2_DEBUG
    return true;
#else
    return false;
#endif
}

} // namespace de
