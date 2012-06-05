#ifndef LIBDENG_VERSIOINFO_H
#define LIBDENG_VERSIOINFO_H

#include <QStringList>
#include <de/String>
#include <de/Time>
#include "dd_version.h"

struct VersionInfo
{
    int major;
    int minor;
    int revision;
    int patch;
    int build;

    /**
     * Version information.
     */
    VersionInfo() : patch(0), build(de::Time().asBuildNumber())
    {
        parseVersionString(DOOMSDAY_VERSION_BASE);
#ifdef DOOMSDAY_BUILD_TEXT
        build = de::String(DOOMSDAY_BUILD_TEXT).toInt();
#endif
    }

    VersionInfo(const de::String& version, int buildNumber) : build(buildNumber)
    {
        parseVersionString(version);
    }

    QString base() const
    {
        return QString("%1.%2.%3").arg(major).arg(minor).arg(revision);
    }

    QString asText() const
    {
        if(patch > 0)
        {
            return base() + QString("-%1 Build %2").arg(patch).arg(build);
        }
        return base() + QString(" Build %1").arg(build);
    }

    void parseVersionString(const de::String& version)
    {
        QStringList parts = version.split('.');
        major = parts[0].toInt();
        minor = parts[1].toInt();
        if(parts[2].contains('-'))
        {
            QStringList rev = parts[2].split('-');
            revision = rev[0].toInt();
            patch = rev[1].toInt();
        }
        else
        {
            revision = parts[2].toInt();
            patch = 0;
        }
    }

    bool operator < (const VersionInfo& other) const
    {
        if(major == other.major)
        {
            if(minor == other.minor)
            {
                if(revision == other.revision)
                {
                    return build < other.build;
                }
                return revision < other.revision;
            }
            return minor < other.minor;
        }
        return major < other.major;
    }

    bool operator == (const VersionInfo& other) const
    {
        return major == other.major && minor == other.minor &&
                revision == other.revision && build == other.build;
    }

    bool operator > (const VersionInfo& other) const
    {
        return !(*this < other || *this == other);
    }
};

#endif // LIBDENG_VERSIOINFO_H
