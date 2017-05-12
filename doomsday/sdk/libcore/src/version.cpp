/**
 * @file version.cpp
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

#include "de/Version"
#include <QStringList>

#ifdef __GNUC__
#  undef major
#  undef minor
#endif

namespace de {

Version::Version()
    : major(0)
    , minor(0)
    , patch(0)
    , build(0)
{}

Version Version::currentBuild()
{
    Version v;
    v.major = LIBDENG2_MAJOR_VERSION;
    v.minor = LIBDENG2_MINOR_VERSION;
    v.patch = LIBDENG2_PATCHLEVEL;

#ifdef LIBDENG2_BUILD_TEXT
    v.build = String(LIBDENG2_BUILD_TEXT).toInt();
#else
    v.build = Time().asBuildNumber(); // only used in development builds
#endif

    v.label = LIBDENG2_RELEASE_LABEL;

#ifdef LIBDENG2_GIT_DESCRIPTION
    v.gitDescription = LIBDENG2_GIT_DESCRIPTION;
#endif
    return v;
}

Version::Version(String const &version, int buildNumber)
{
    parseVersionString(version);
    if (buildNumber)
    {
        build = buildNumber;
    }
}

bool Version::isValid() const
{
    return major || minor || patch || build ||
           !label.isEmpty() || !gitDescription.isEmpty();
}

String Version::base() const
{
    String v = compactNumber();
    if (!label.isEmpty()) v += String("-%1").arg(label);
    return v;
}

String Version::compactNumber() const
{
    if (patch != 0)
    {
        return String("%1.%2.%3").arg(major).arg(minor).arg(patch);
    }
    return String("%1.%2").arg(major).arg(minor);
}

String Version::fullNumber() const
{
    if (build != 0)
    {
        return String("%1.%2.%3.%4").arg(major).arg(minor).arg(patch).arg(build);
    }
    return String("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

String Version::asHumanReadableText() const
{
    if (!build) return base();
    return base() + String(" [#%1]").arg(build);
}

void Version::parseVersionString(String const &version)
{
    major = minor = patch = build = 0;
    label.clear();
    gitDescription.clear();

    int dashPos = version.indexOf('-');

    QStringList parts = version.left(dashPos).split('.');
    if (parts.size() >= 1) major = String(parts[0]).toInt();
    if (parts.size() >= 2) minor = String(parts[1]).toInt(nullptr, 10, String::AllowSuffix);
    if (parts.size() >= 3) patch = String(parts[2]).toInt(nullptr, 10, String::AllowSuffix);
    if (parts.size() >= 4) build = String(parts[3]).toInt(nullptr, 10, String::AllowSuffix);

    if (dashPos >= 0 && dashPos < version.size() - 1)
    {
        label = version.substr(dashPos + 1);
    }
}

bool Version::operator < (Version const &other) const
{
    if (major == other.major)
    {
        if (minor == other.minor)
        {
            if (patch == other.patch)
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
#if defined (WIN32)
    return "windows";
#elif defined (MACOSX)
    return "macx";
#elif defined (DENG_IOS)
    return "ios";
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
