/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/Version"

using namespace de;

Config::Config(String const &path) : _configPath(path)
{
    _writtenConfigPath = String("/home") / _configPath.fileNameWithoutExtension() + ".config";
}

Config::~Config()
{
    LOG_AS("~Config");
    try
    {
        write();
    }
    catch(Error const &err)
    {
        LOG_ERROR("") << err.asText();
    }
}

void Config::read()
{
    LOG_AS("Config::read");
    
    // Current version.
    Version verInfo;
    QScopedPointer<ArrayValue> version(new ArrayValue());
    version->add(new NumberValue(verInfo.major));
    version->add(new NumberValue(verInfo.minor));
    version->add(new NumberValue(verInfo.patch));
    version->add(new NumberValue(verInfo.build));

    File &scriptFile = App::rootFolder().locate<File>(_configPath);
    bool shouldRunScript = false;

    try
    {
        // If we already have a saved copy of the config, read it.
        File &file = App::rootFolder().locate<File>(_writtenConfigPath);
        Reader(file) >> names();
        
        // If the saved config is from a different version, rerun the script.
        Value const &oldVersion = names()["__version__"].value();
        if(oldVersion.compare(*version))
        {
            // Version mismatch: store the old version in a separate variable.
            _config.globals().add(new Variable("__oldversion__", oldVersion.duplicate(),
                                               Variable::AllowArray | Variable::ReadOnly));
            shouldRunScript = true;
        }
        else
        {
            // Versions match.
            LOG_MSG("") << _writtenConfigPath << " matches version " << version->asText();
        }

        // Also check the timestamp of written config vs. they config script.
        // If script is newer, it should be rerun.
        if(scriptFile.status().modifiedAt > file.status().modifiedAt)
        {
            LOG_MSG("%s is newer than %s, rerunning the script.")
                    << _configPath << _writtenConfigPath;
            shouldRunScript = true;
        }
    }
    catch(Folder::NotFoundError const &)
    {
        // It is missing if the config hasn't been written yet.
        shouldRunScript = true;
    }
    catch(Error const &error)
    {
        LOG_WARNING(error.what());

        // Something is wrong, maybe rerunning will fix it.
        shouldRunScript = true;
    }
            
    // The version of libdeng2 is automatically included in the namespace.
    _config.globals().add(new Variable("__version__", version.take(),
                                       Variable::AllowArray | Variable::ReadOnly));

    if(shouldRunScript)
    {
        // Read the main configuration.
        Script script(scriptFile);
        _config.run(script);
        _config.execute();
    }
}

void Config::write()
{
    File &file = App::rootFolder().replaceFile(_writtenConfigPath);
    Writer(file) << names();    
}

Record &Config::names()
{
    return _config.globals();
}

Value &Config::get(String const &name)
{
    return _config.globals()[name].value();
}

dint Config::geti(String const &name)
{
    return dint(get(name).asNumber());
}

duint Config::getui(String const &name)
{
    return duint(get(name).asNumber());
}

ddouble Config::getd(String const &name)
{
    return get(name).asNumber();
}

String Config::gets(String const &name)
{
    return get(name).asText();
}
