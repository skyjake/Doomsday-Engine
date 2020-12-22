/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/config.h"
#include "de/app.h"
#include "de/archive.h"
#include "de/arrayvalue.h"
#include "de/commandline.h"
#include "de/folder.h"
#include "de/log.h"
#include "de/numbervalue.h"
#include "de/package.h"
#include "de/reader.h"
#include "de/refuge.h"
#include "de/version.h"
#include "de/writer.h"

namespace de {

DE_PIMPL_NOREF(Config)
{
    /// Configuration file name.
    Path configPath;

    /// Saved configuration data (inside persist.pack).
    Refuge refuge;

    /// The configuration namespace.
    Process config;

    /// Previous installed version (__version__ in the read persistent Config).
    Version oldVersion;

    Impl(Path path)
        : configPath(std::move(path))
        , refuge("modules/Config")
        , config(&refuge.objectNamespace())
    {}

    void setOldVersion(const Value &old)
    {
        try
        {
            const auto &vers = old.as<ArrayValue>();
            oldVersion.major = int(vers.at(0).asNumber());
            oldVersion.minor = int(vers.at(1).asNumber());
            oldVersion.patch = int(vers.at(2).asNumber());
            oldVersion.build = int(vers.at(3).asNumber());
        }
        catch (...)
        {}
    }

    void write()
    {
        if (configPath.isEmpty()) return;
        refuge.write();
    }
};

Config::Config(const Path &path)
    : RecordAccessor(nullptr)
    , d(new Impl(path))
{
    setAccessedRecord(objectNamespace());
}

Config::ReadStatus Config::read()
{
    ReadStatus readStatus = WasNotRead;

    if (d->configPath.isEmpty()) return readStatus;

    LOG_AS("Config::read");

    // Current version.
    Version verInfo = Version::currentBuild();
    std::unique_ptr<ArrayValue> version(new ArrayValue);
    *version << NumberValue(verInfo.major)
             << NumberValue(verInfo.minor)
             << NumberValue(verInfo.patch)
             << NumberValue(verInfo.build);

    auto &scriptFile = App::rootFolder().locate<File>(d->configPath);
    bool shouldRunScript = App::commandLine().has("-reconfig");

    try
    {
        // If we already have a saved copy of the config, read it.
        d->refuge.read();
        readStatus = DifferentVersion;

        LOG_DEBUG("Found serialized Config:\n") << objectNamespace();

        // If the saved config is from a different version, rerun the script.
        if (objectNamespace().has("__version__"))
        {
            const Value &oldVersion = objectNamespace()["__version__"].value();
            d->setOldVersion(oldVersion);
            if (oldVersion.compare(*version))
            {
                // Version mismatch: store the old version in a separate variable.
                d->config.globals().add(new Variable("__oldversion__", oldVersion.duplicate(),
                                                 Variable::AllowArray | Variable::ReadOnly));
                shouldRunScript = true;
            }
            else
            {
                // Versions match.
                readStatus = SameVersion;
                LOG_MSG("") << d->refuge.path() << " matches version " << version->asText();
            }
        }
        else
        {
            // Don't know what version this is, run script to be sure.
            shouldRunScript = true;
        }

        // Also check the timestamp of written config vs. the config script.
        // If script is newer, it should be rerun.
        if (scriptFile.status().modifiedAt > d->refuge.lastWrittenAt())
        {
            LOG_MSG("%s is newer than %s, rerunning the script")
                    << d->configPath << d->refuge.path();
            shouldRunScript = true;
        }

        // Check the container, too.
        if (!shouldRunScript &&
            Package::containerOfFileModifiedAt(scriptFile) > d->refuge.lastWrittenAt())
        {
            LOG_MSG("Package '%s' is newer than %s, rerunning the script")
                    << Package::identifierForContainerOfFile(scriptFile)
                    << d->refuge.path();
            shouldRunScript = true;
        }
    }
    catch (const Archive::NotFoundError &)
    {
        // It is missing from persist.pack if the config hasn't been written yet.
        shouldRunScript = true;
    }
    catch (const IByteArray::OffsetError &)
    {
        // Empty or missing serialization?
        shouldRunScript = true;
    }
    catch (const Error &error)
    {
        LOG_WARNING(error.what());

        // Something is wrong, maybe rerunning will fix it.
        shouldRunScript = true;
    }

    // The version of libcore is automatically included in the namespace.
    d->config.globals().add(new Variable("__version__", version.release(),
                                         Variable::AllowArray | Variable::ReadOnly));

    if (shouldRunScript)
    {
        // Read the main configuration.
        Script script(scriptFile);
        d->config.run(script);
        d->config.execute();
    }

    return readStatus;
}

void Config::write() const
{
    d->write();
}

void Config::writeIfModified() const
{
    try
    {
        if (d->refuge.hasModifiedVariables())
        {
            write();
        }
    }
    catch (const Error &er)
    {
        LOG_WARNING("Failed to write Config: ") << er.asText();
    }
}

Record &Config::objectNamespace()
{
    return d->config.globals();
}

const Record &Config::objectNamespace() const
{
    return d->config.globals();
}

Config &Config::get()
{
    return App::config();
}

Variable &Config::get(const String &name) // static
{
    return get()[name];
}

bool Config::exists()
{
    return App::configExists();
}

Version Config::upgradedFromVersion() const
{
    return d->oldVersion;
}

Variable &Config::set(const String &name, bool value)
{
    return objectNamespace().set(name, value);
}

Variable &Config::set(const String &name, const Value::Number &value)
{
    return objectNamespace().set(name, NumberValue(value));
}

Variable &Config::set(const String &name, dint value)
{
    return objectNamespace().set(name, value);
}

Variable &Config::set(const String &name, duint value)
{
    return objectNamespace().set(name, value);
}

Variable &Config::set(const String &name, ArrayValue *value)
{
    return objectNamespace().set(name, value);
}

Variable &Config::set(const String &name, const Value::Text &value)
{
    return objectNamespace().set(name, value);
}

} // namespace de
