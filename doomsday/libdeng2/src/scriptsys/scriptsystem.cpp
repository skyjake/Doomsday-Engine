/** @file scriptsystem.cpp  Subsystem for scripts.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/ScriptSystem"
#include "de/App"
#include "de/Record"
#include "de/Module"
#include "de/Version"
#include "de/ArrayValue"
#include "de/NumberValue"
#include "de/math.h"

#include <QMap>

namespace de {

DENG2_PIMPL(ScriptSystem), DENG2_OBSERVES(Record, Deletion)
{
    /// Built-in special modules. These are constructed by native code and thus not
    /// parsed from any script.
    typedef QMap<String, Record *> NativeModules;
    NativeModules nativeModules;
    Record versionModule; // Version: information about the platform and build

    /// Resident modules.
    typedef QMap<String, Module *> Modules;
    Modules modules;

    Instance(Public *i) : Base(*i)
    {
        // Setup the Version module.
        Version ver;
        Record &mod = versionModule;
        ArrayValue *num = new ArrayValue;
        *num << NumberValue(ver.major) << NumberValue(ver.minor)
             << NumberValue(ver.patch) << NumberValue(ver.build);
        mod.addArray("VERSION", num).setReadOnly();
        mod.addText("TEXT", ver.asText()).setReadOnly();
        mod.addNumber("BUILD", ver.build).setReadOnly();
        mod.addText("OS", Version::operatingSystem()).setReadOnly();
        mod.addNumber("CPU_BITS", Version::cpuBits()).setReadOnly();
        mod.addBoolean("DEBUG", Version::isDebugBuild()).setReadOnly();

        addNativeModule("Version", mod);
    }

    ~Instance()
    {
        DENG2_FOR_EACH(NativeModules, i, nativeModules)
        {
            i.value()->audienceForDeletion -= this;
        }
    }

    void addNativeModule(String const &name, Record &module)
    {
        nativeModules.insert(name, &module);
        module.audienceForDeletion += this;
    }

    void recordBeingDeleted(Record &record)
    {
        QMutableMapIterator<String, Record *> iter(nativeModules);
        while(iter.hasNext())
        {
            iter.next();
            if(iter.value() == &record)
            {
                iter.remove();
            }
        }
    }
};

ScriptSystem::ScriptSystem() : d(new Instance(this))
{}

ScriptSystem::~ScriptSystem()
{
    delete d;
}

void ScriptSystem::addNativeModule(String const &name, Record &module)
{
    d->addNativeModule(name, module);
}

static int sortFilesByModifiedAt(File const *a, File const *b)
{
    return de::cmp(a->status().modifiedAt, b->status().modifiedAt);
}

Record &ScriptSystem::importModule(String const &name, String const &fromPath)
{
    LOG_AS("ScriptSystem::importModule");

    // There are some special native modules.
    Instance::NativeModules::const_iterator foundNative = d->nativeModules.constFind(name);
    if(foundNative != d->nativeModules.constEnd())
    {
        return *foundNative.value();
    }

    // Maybe we already have this module?
    Instance::Modules::iterator found = d->modules.find(name);
    if(found != d->modules.end())
    {
        return found.value()->names();
    }

    // Fall back on the default if the config hasn't been imported yet.
    std::auto_ptr<ArrayValue> defaultImportPath(new ArrayValue);
    defaultImportPath->add("");
    defaultImportPath->add("*"); // Newest module with a matching name.
    ArrayValue *importPath = defaultImportPath.get();
    try
    {
        importPath = &App::config().names()["importPath"].value<ArrayValue>();
    }
    catch(Record::NotFoundError const &)
    {}

    // Search the import path (array of paths).
    DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, importPath->elements())
    {
        String dir = (*i)->asText();
        String p;
        FileSystem::FoundFiles matching;
        File *found = 0;
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
            App::fileSystem().findAll(name + ".de", matching);
            if(matching.empty())
            {
                continue;
            }
            matching.sort(sortFilesByModifiedAt);
            found = matching.back();
            LOG_VERBOSE("Chose ") << found->path() << " out of " << dint(matching.size()) << " candidates (latest modified).";
        }
        else
        {
            p = dir / name;
        }
        if(!found)
        {
            found = App::rootFolder().tryLocateFile(p + ".de");
        }
        if(found)
        {
            Module *module = new Module(*found);
            d->modules.insert(name, module);
            return module->names();
        }
    }

    throw NotFoundError("ScriptSystem::importModule", "Cannot find module '" + name + "'");
}

void ScriptSystem::timeChanged(Clock const &)
{
    // perform time-based processing/scheduling/events
}

} // namespace de
