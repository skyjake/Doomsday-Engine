/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
 
#include "de/Config"
#include "de/App"
#include "de/Log"
#include "de/Folder"
#include "de/ArrayValue"
#include "de/NumberValue"
#include "de/Reader"
#include "de/Writer"

using namespace de;

Config::Config(const String& path) : _configPath(path)
{
    _writtenConfigPath = String("/home") / _configPath.fileNameWithoutExtension() + ".config";
}

Config::~Config()
{
    LOG_AS("Config::~Config");
    try
    {
        write();
    }
    catch(const Error& err)
    {
        LOG_ERROR("") << err.asText();
    }
}

void Config::read()
{
    LOG_AS("Config::read");
    
    // Current version.
    QScopedPointer<ArrayValue> version(new ArrayValue());
    version->add(new NumberValue(LIBDENG2_MAJOR_VERSION));
    version->add(new NumberValue(LIBDENG2_MINOR_VERSION));
    version->add(new NumberValue(LIBDENG2_PATCHLEVEL));

    try
    {
        // If we already have a saved copy of the config, read it.
        File& file = App::rootFolder().locate<File>(_writtenConfigPath);
        Reader(file) >> names();
        
        // If the saved config is from a different version, rerun the script.
        // Otherwise, we're done.
        const Value& oldVersion = names()["__version__"].value();
        if(!oldVersion.compare(*version))
        {
            // Versions match.
            LOG_MSG("") << _writtenConfigPath << " matches version " << version->asText();
            return;
        }

        // Version mismatch: store the old version in a separate variable.
        _config.globals().add(new Variable("__oldversion__", oldVersion.duplicate(),
                                           Variable::AllowArray | Variable::ReadOnly));
    }
    catch(const Folder::NotFoundError&)
    {
        // It is missing if the config hasn't been written yet.
    }
    catch(const Error& error)
    {
        LOG_WARNING(error.what());
    }
            
    // The version of libdeng2 is automatically included.
    _config.globals().add(new Variable("__version__", version.take(),
                                       Variable::AllowArray | Variable::ReadOnly));

    // Read the main configuration. 
    Script script(App::rootFolder().locate<File>(_configPath));
    _config.run(script);
    _config.execute();
}

void Config::write()
{
    File& file = App::rootFolder().replaceFile(_writtenConfigPath);
    Writer(file) << names();    
}

Record& Config::names()
{
    return _config.globals();
}

Value& Config::get(const String& name)
{
    return _config.globals()[name].value();
}

dint Config::geti(const String& name)
{
    return dint(get(name).asNumber());
}

duint Config::getui(const String& name)
{
    return duint(get(name).asNumber());
}

ddouble Config::getd(const String& name)
{
    return get(name).asNumber();
}

String Config::gets(const String& name)
{
    return get(name).asText();
}
