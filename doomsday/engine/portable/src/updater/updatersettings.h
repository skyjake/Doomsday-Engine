#ifndef LIBDENG_UPDATERSETTINGS_H
#define LIBDENG_UPDATERSETTINGS_H

#include <de/Time>
#include <de/String>

class UpdaterSettings
{
public:
    enum Frequency
    {
        Daily    = 0,
        Biweekly = 1,   // 3.5 days
        Weekly   = 2,   // 7 days
        Monthly  = 3    // 30 days
    };
    enum Channel
    {
        Stable   = 0,
        Unstable = 1
    };

public:
    UpdaterSettings();

    Frequency frequency() const;
    Channel channel() const;
    de::Time lastCheckTime() const;
    bool onlyCheckManually() const;
    bool deleteAfterUpdate() const;
    bool isDefaultDownloadPath() const;
    de::String downloadPath() const;
    de::String pathToDeleteAtStartup() const;

    void setFrequency(Frequency freq);
    void setChannel(Channel channel);
    void setLastCheckTime(const de::Time& time);
    void setOnlyCheckManually(bool onlyManually);
    void setDeleteAfterUpdate(bool deleteAfter);
    void setDownloadPath(de::String downloadPath);
    void useDefaultDownloadPath();
    void setPathToDeleteAtStartup(de::String deletePath);

    static de::String defaultDownloadPath();
};

#endif // LIBDENG_UPDATERSETTINGS_H
