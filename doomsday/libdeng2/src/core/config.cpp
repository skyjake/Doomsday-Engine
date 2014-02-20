/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 
#include "de/Config"
#include "de/App"
#include "de/Archive"
#include "de/Refuge"
#include "de/Log"
#include "de/Folder"
#include "de/ArrayValue"
#include "de/NumberValue"
#include "de/ArrayValue"
#include "de/Reader"
#include "de/Writer"
#include "de/Version"

namespace de {

DENG2_PIMPL_NOREF(Config)
{
    /// Configuration file name.
    Path configPath;

    /// Saved configuration data (inside persist.pack).
    Refuge refuge;

    /// The configuration namespace.
    Process config;

    /// Previous installed version (__version__ in the read persistent Config).
    Version oldVersion;

    Instance(Path const &path)
        : configPath(path)
        , refuge("modules/Config")
        , config(&refuge.names())
    {}

    void setOldVersion(Value const &old)
    {
        try
        {
            ArrayValue const &vers = old.as<ArrayValue>();
            oldVersion.major = int(vers.at(0).asNumber());
            oldVersion.minor = int(vers.at(1).asNumber());
            oldVersion.patch = int(vers.at(2).asNumber());
            oldVersion.build = int(vers.at(3).asNumber());
        }
        catch(...)
        {}
    }
};

Config::Config(Path const &path) : d(new Instance(path))
{}

void Config::read()
{
    LOG_AS("Config::read");
    
    // Current version.
    Version verInfo;
    QScopedPointer<ArrayValue> version(new ArrayValue);
    *version << NumberValue(verInfo.major)
             << NumberValue(verInfo.minor)
             << NumberValue(verInfo.patch)
             << NumberValue(verInfo.build);

    File &scriptFile = App::rootFolder().locate<File>(d->configPath);
    bool shouldRunScript = App::commandLine().has("-reconfig");

    try
    {
        // If we already have a saved copy of the config, read it.
        d->refuge.read();

        LOG_DEBUG("Found serialized Config:\n") << names();
        
        // If the saved config is from a different version, rerun the script.
        Value const &oldVersion = names()["__version__"].value();
        d->setOldVersion(oldVersion);
        if(oldVersion.compare(*version))
        {
            // Version mismatch: store the old version in a separate variable.
            d->config.globals().add(new Variable("__oldversion__", oldVersion.duplicate(),
                                                 Variable::AllowArray | Variable::ReadOnly));
            shouldRunScript = true;
        }
        else
        {
            // Versions match.
            LOG_MSG("") << d->refuge.path() << " matches version " << version->asText();
        }

        // Also check the timestamp of written config vs. the config script.
        // If script is newer, it should be rerun.
        if(scriptFile.status().modifiedAt > d->refuge.lastWrittenAt())
        {
            LOG_MSG("%s is newer than %s, rerunning the script.")
                    << d->configPath << d->refuge.path();
            shouldRunScript = true;
        }
    }
    catch(Archive::NotFoundError const &)
    {
        // It is missing from persist.pack if the config hasn't been written yet.
        shouldRunScript = true;
    }
    catch(Error const &error)
    {
        LOG_WARNING(error.what());

        // Something is wrong, maybe rerunning will fix it.
        shouldRunScript = true;
    }
            
    // The version of libdeng2 is automatically included in the namespace.
    d->config.globals().add(new Variable("__version__", version.take(),
                                         Variable::AllowArray | Variable::ReadOnly));

    if(shouldRunScript)
    {
        // Read the main configuration.
        Script script(scriptFile);
        d->config.run(script);
        d->config.execute();
    }
}

void Config::write() const
{
    d->refuge.write();
}

Record &Config::names()
{
    return d->config.globals();
}

Record const &Config::names() const
{
    return d->config.globals();
}

Variable &Config::operator [] (String const &name)
{
    return names()[name];
}

Variable const &Config::operator [] (String const &name) const
{
    return names()[name];
}

Version Config::upgradedFromVersion() const
{
    return d->oldVersion;
}

Value &Config::get(String const &name) const
{
    return d->config.globals()[name].value();
}

dint Config::geti(String const &name) const
{
    return dint(get(name).asNumber());
}

bool Config::getb(String const &name) const
{
    return get(name).isTrue();
}

duint Config::getui(String const &name) const
{
    return duint(get(name).asNumber());
}

ddouble Config::getd(String const &name) const
{
    return get(name).asNumber();
}

String Config::gets(String const &name) const
{
    return get(name).asText();
}

ArrayValue &Config::geta(String const &name) const
{
    return getAs<ArrayValue>(name);
}

Variable &Config::set(String const &name, bool value)
{
    return names().set(name, value);
}

Variable &Config::set(String const &name, Value::Number const &value)
{
    return names().set(name, value);
}

Variable &Config::set(String const &name, dint value)
{
    return names().set(name, value);
}

Variable &Config::set(String const &name, duint value)
{
    return names().set(name, value);
}

Variable &Config::set(String const &name, ArrayValue *value)
{
    return names().set(name, value);
}

Variable &Config::set(String const &name, Value::Text const &value)
{
    return names().set(name, value);
}

} // namespace de
