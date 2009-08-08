/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/Folder"
#include "de/ArrayValue"
#include "de/NumberValue"
#include "de/Reader"
#include "de/Writer"

using namespace de;

Config::Config(const String& path) : configPath_(path)
{
    writtenConfigPath_ = String("/home") / configPath_.fileNameWithoutExtension() + ".config";
}

Config::~Config()
{
    write();
}

void Config::read()
{
    // Current version.
    std::auto_ptr<ArrayValue> version(new ArrayValue());
    version->add(new NumberValue(LIBDENG2_MAJOR_VERSION));
    version->add(new NumberValue(LIBDENG2_MINOR_VERSION));
    version->add(new NumberValue(LIBDENG2_PATCHLEVEL));

    try
    {
        // If we already have a saved copy of the config, read it.
        File& file = App::fileRoot().locate<File>(writtenConfigPath_);
        Reader(file) >> names();
        
        // If the saved config is from a different version, rerun the script.
        // Otherwise, we're done.
        if(!names()["__version__"].value().compare(*version))
        {
            // Versions match.
            std::cout << writtenConfigPath_ << " matches version " << version->asText() << "\n";
            return;
        }
    }
    catch(const Folder::NotFoundError&)
    {}
            
    // The version of libdeng2 is automatically included.
    config_.globals().add(new Variable("__version__", version.release(), 
        Variable::ARRAY | Variable::READ_ONLY));

    // Read the main configuration. 
    Script script(App::fileRoot().locate<File>(configPath_));
    config_.run(script);
    config_.execute();
}

void Config::write()
{
    File& file = App::fileRoot().replaceFile(writtenConfigPath_);
    Writer(file) << names();    
}

Record& Config::names()
{
    return config_.globals();
}

Value& Config::get(const String& name)
{
    return config_.globals()[name].value();
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
