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

#include "de/version.h"
#include "de/list.h"

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

Version::Version(int major, int minor, int patch, int buildNumber)
    : major(major)
    , minor(minor)
    , patch(patch)
    , build(buildNumber)
{}

Version Version::currentBuild()
{
    Version v;
    v.major = LIBCORE_MAJOR_VERSION;
    v.minor = LIBCORE_MINOR_VERSION;
    v.patch = LIBCORE_PATCHLEVEL;

#ifdef LIBCORE_BUILD_TEXT
    v.build = String(LIBCORE_BUILD_TEXT).toInt();
#else
    v.build = Time().asBuildNumber();   // only used in development builds
#endif

    v.label = LIBCORE_RELEASE_LABEL;

#ifdef LIBCORE_GIT_DESCRIPTION
    v.gitDescription = LIBCORE_GIT_DESCRIPTION;
#endif
    return v;
}

Version::Version(const String &version, int buildNumber)
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

String Version::compactNumber() const
{
    if (patch != 0)
    {
        return Stringf("%d.%d.%d", major, minor, patch);
    }
    return Stringf("%d.%d", major, minor);
}

String Version::fullNumber() const
{
    if (build != 0)
    {
        return Stringf("%d.%d.%d.%d", major, minor, patch, build);
    }
    return Stringf("%d.%d.%d", major, minor, patch);
}

String Version::asHumanReadableText() const
{
    String v = compactNumber();
    if (label || build)
    {
        v += " (";
        v += label.lower();
        if (build)
        {
            v += String::format("%sbuild %d)", label ? " " : "", build);
        }
        else
        {
            v += ")";
        }
    }
    return v;
}

void Version::parseVersionString(const String &version)
{
    major = minor = patch = build = 0;
    label.clear();
    gitDescription.clear();

    if (version.isEmpty()) return;

    auto dashPos = version.indexOf('-');

    StringList parts = version.left(dashPos).split('.');
    if (parts.size() >= 1) major = parts[0].toInt();
    if (parts.size() >= 2) minor = parts[1].toInt(nullptr, 10, String::AllowSuffix);
    if (parts.size() >= 3) patch = parts[2].toInt(nullptr, 10, String::AllowSuffix);
    if (parts.size() >= 4) build = parts[3].toInt(nullptr, 10, String::AllowSuffix);

    if (dashPos >= 0 && dashPos < version.sizei() - 1)
    {
        label = version.substr(dashPos + 1);
    }
}

bool Version::operator<(const Version &other) const
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

bool Version::operator==(const Version &other) const
{
    return major == other.major && minor == other.minor && patch == other.patch &&
           build == other.build;
}

bool Version::operator > (const Version &other) const
{
    return !(*this < other || *this == other);
}

String Version::userAgent() const
{
    return Stringf("Doomsday Engine %s (%s)", fullNumber().c_str(), operatingSystem().c_str());
}

String Version::operatingSystem()
{
#if defined (DE_WINDOWS)
    return "windows";
#elif defined (MACOSX)
    return "macx";
#elif defined (DE_IOS)
    return "ios";
#else
    return "unix";
#endif
}

duint Version::cpuBits()
{
#ifdef DE_64BIT
    return 64;
#else
    return 32;
#endif
}

bool Version::isDebugBuild()
{
#ifdef DE_DEBUG
    return true;
#else
    return false;
#endif
}

} // namespace de
