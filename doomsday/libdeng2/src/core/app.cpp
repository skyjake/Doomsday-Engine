/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2010-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/App"
#include "de/Log"
#include "de/ArrayValue"
#include "de/Module"
#include "de/math.h"
#include <QDebug>

using namespace de;

App::App(int& argc, char** argv, bool useGUI)
    : QApplication(argc, argv, useGUI),
      _cmdLine(argc, argv)
{
    _appPath = applicationFilePath();
}

bool App::notify(QObject* receiver, QEvent* event)
{
    try
    {
        return QApplication::notify(receiver, event);
    }
    catch(const std::exception& error)
    {
        emit uncaughtException(error.what());
    }
    catch(...)
    {
        emit uncaughtException("de::App caught exception of unknown type.");
    }
    return false;
}

App& App::app()
{
    return *DENG2_APP;
}

CommandLine& App::commandLine()
{
    return DENG2_APP->_cmdLine;
}

String App::executablePath()
{
    return DENG2_APP->_appPath;
}

static int sortFilesByModifiedAt(const File* a, const File* b)
{
    return cmp(a->status().modifiedAt, b->status().modifiedAt);
}

Record& App::importModule(const String& name, const String& fromPath)
{
    LOG_AS("App::importModule");

    App& self = app();

    // There are some special modules.
    if(name == "Config")
    {
        return self.config().names();
    }

    // Maybe we already have this module?
    Modules::iterator found = self._modules.find(name);
    if(found != self._modules.end())
    {
        return found->second->names();
    }

    /// @todo  Move this path searching logic to FS.

    // Fall back on the default if the libdeng2 module hasn't been imported yet.
    std::auto_ptr<ArrayValue> defaultImportPath(new ArrayValue);
    defaultImportPath->add("");
    defaultImportPath->add("*"); // Newest module with a matching name.
    ArrayValue* importPath = defaultImportPath.get();
    try
    {
        importPath = &config().names()["deng.importPath"].value<ArrayValue>();
    }
    catch(const Record::NotFoundError&)
    {}

    // Search the import path (array of paths).
    DENG2_FOR_EACH_i(importPath->elements(), ArrayValue::Elements::const_iterator)
    {
        String dir = (*i)->asText();
        String p;
        FS::FoundFiles matching;
        File* found = 0;
        if(dir.empty())
        {
            if(!fromPath.empty())
            {
                // Try the local folder.
                p = fromPath.fileNamePath() / name;
            }
            else
            {
                continue;
            }
        }
        else if(dir == "*")
        {
            fileSystem().findAll(name + ".de", matching);
            if(matching.empty())
            {
                continue;
            }
            matching.sort(sortFilesByModifiedAt);
            found = matching.back();
            LOG_VERBOSE("Chose ") << found->path() << " out of " << dint(matching.size()) << " candidates.";
        }
        else
        {
            p = dir / name;
        }
        if(!found)
        {
            found = fileRoot().tryLocateFile(p + ".de");
        }
        if(found)
        {
            Module* module = new Module(*found);
            self._modules[name] = module;
            return module->names();
        }
    }

    throw NotFoundError("App::importModule", "Cannot find module '" + name + "'");
}
