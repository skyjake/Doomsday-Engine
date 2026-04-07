/** @file scriptsystem.cpp  Subsystem for scripts.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/scripting/scriptsystem.h"
#include "de/animation.h"
#include "de/app.h"
#include "de/arrayvalue.h"
#include "de/blockvalue.h"
#include "de/config.h"
#include "de/dictionaryvalue.h"
#include "de/filesystem.h"
#include "de/scripting/module.h"
#include "de/numbervalue.h"
#include "de/record.h"
#include "de/recordvalue.h"
#include "de/textvalue.h"
#include "de/version.h"
#include "de/math.h"

#include "../src/scripting/bindings_core.h"
#include "../src/scripting/bindings_math.h"

namespace de {

static ScriptSystem *_scriptSystem = nullptr;

DE_PIMPL(ScriptSystem)
, DE_OBSERVES(Record, Deletion)
{
    Binder binder;

    /// Built-in special modules. These are constructed by native code and thus not
    /// parsed from any script.
    typedef Hash<String, Record *> NativeModules;
    LockableT<NativeModules> nativeModules; // not owned
    Record coreModule;    ///< Script: built-in script classes and functions.
    Record mathModule;    ///< Math: math related functions.
    Record versionModule; ///< Version: information about the platform and build

    /// Resident modules.
    typedef Hash<String, Module *> Modules; // owned
    Modules modules;

    List<Path> additionalImportPaths;

    Impl(Public *i) : Base(*i)
    {
        initCoreModule();
        initMathModule(binder, mathModule);
        addNativeModule("Math", mathModule);

        // Setup the Version module.
        {
            const Version ver = Version::currentBuild();
            Record &mod = versionModule;
            ArrayValue *num = new ArrayValue;
            *num << NumberValue(ver.major) << NumberValue(ver.minor)
                 << NumberValue(ver.patch) << NumberValue(ver.build);
            mod.addArray  ("VERSION",  num                       ).setReadOnly();
            mod.addText   ("TEXT",     ver.fullNumber()          ).setReadOnly();
            mod.addNumber ("BUILD",    ver.build                 ).setReadOnly();
            mod.addText   ("OS",       Version::operatingSystem()).setReadOnly();
            mod.addNumber ("CPU_BITS", Version::cpuBits()        ).setReadOnly();
            mod.addBoolean("DEBUG",    Version::isDebugBuild()   ).setReadOnly();
            mod.addText   ("GIT",      ver.gitDescription        ).setReadOnly();
#ifdef DE_STABLE
            mod.addBoolean("STABLE",   true).setReadOnly();
#else
            mod.addBoolean("STABLE",   false).setReadOnly();
#endif
            addNativeModule("Version", mod);
        }
    }

    ~Impl()
    {
        modules.deleteAll();
    }

    static Value *Function_ImportPath(Context &, const Function::ArgumentValues &)
    {
        DE_ASSERT(_scriptSystem != nullptr);

        StringList importPaths;
        _scriptSystem->d->listImportPaths(importPaths);

        auto *array = new ArrayValue;
        for (const auto &path : importPaths)
        {
            *array << TextValue(path);
        }
        return array;
    }

    void initCoreModule()
    {
        de::initCoreModule(binder, coreModule);

        // General functions.
        binder.init(coreModule)
                << DE_FUNC_NOARG(ImportPath, "importPath");

        addNativeModule("Core", coreModule);
    }

    void addNativeModule(const String &name, Record &module)
    {
        DE_GUARD(nativeModules);
        auto existing = nativeModules.value.find(name);
        if (existing != nativeModules.value.end())
        {
            existing->second->audienceForDeletion() -= this;
        }
        nativeModules.value.insert(name, &module); // not owned
        module.audienceForDeletion() += this;
    }

    void removeNativeModule(const String &name)
    {
        DE_GUARD(nativeModules);
        if (!nativeModules.value.contains(name)) return;

        nativeModules.value[name]->audienceForDeletion() -= this;
        nativeModules.value.remove(name);
    }

    void recordBeingDeleted(Record &record)
    {
        DE_GUARD(nativeModules);
        MutableHashIterator<String, Record *> iter(nativeModules);
        while (iter.hasNext())
        {
            iter.next();
            if (iter.value() == &record)
            {
                iter.remove();
            }
        }
    }

    void listImportPaths(StringList &importPaths)
    {
        std::unique_ptr<ArrayValue> defaultImportPath(new ArrayValue);
        defaultImportPath->add("");
        //defaultImportPath->add("*"); // Newest module with a matching name.
        const ArrayValue *importPath;
        try
        {
            importPath = &App::config().geta("importPath");
        }
        catch (const Record::NotFoundError &)
        {
            importPath = defaultImportPath.get();
        }

        // Compile a list of all possible import locations.
        importPaths.clear();
        for (const auto *i : importPath->elements())
        {
            importPaths << i->asText();
        }
        for (const Path &path : additionalImportPaths)
        {
            importPaths << path;
        }
    }
};

ScriptSystem::ScriptSystem() : d(new Impl(this))
{
    _scriptSystem = this;
}

ScriptSystem::~ScriptSystem()
{
    _scriptSystem = nullptr;
}

void ScriptSystem::addModuleImportPath(const Path &path)
{
    d->additionalImportPaths << path;
}

void ScriptSystem::removeModuleImportPath(const Path &path)
{
    d->additionalImportPaths.removeOne(path);
}

void ScriptSystem::addNativeModule(const String &name, Record &module)
{
    d->addNativeModule(name, module);
}

void ScriptSystem::removeNativeModule(const String &name)
{
    d->removeNativeModule(name);
}

Record &ScriptSystem::nativeModule(const String &name)
{
    DE_GUARD_FOR(d->nativeModules, G);
    auto foundNative = d->nativeModules.value.find(name);
    DE_ASSERT(foundNative != d->nativeModules.value.end());
    return *foundNative->second;
}

Record &ScriptSystem::operator[](const String &name)
{
    if (nativeModuleExists(name))
    {
        return nativeModule(name);
    }
    // Imported modules.
    {
        const auto &mods = d->modules;
        auto found = mods.find(name);
        if (found != mods.end())
        {
            return found->second->names();
        }
    }
    throw NotFoundError("ScriptSystem::operator[]", "Module not found: " + name);
}

bool ScriptSystem::nativeModuleExists(const String &name) const
{
    DE_GUARD_FOR(d->nativeModules, G);
    return d->nativeModules.value.contains(name);
}

StringList ScriptSystem::nativeModules() const
{
    DE_GUARD_FOR(d->nativeModules, G);
    return map<StringList>(d->nativeModules.value,
                           [](const std::pair<String, Record *> &v) { return v.first; });
}

const File *ScriptSystem::tryFindModuleSource(const String &name, const String &localPath)
{
    // Compile a list of all possible import locations.
    StringList importPaths;
    d->listImportPaths(importPaths);

    // Search all import locations.
    for (const String &dir : importPaths)
    {
        String p;
        FS::FoundFiles matching;
        File *found = nullptr;
        if (dir.empty())
        {
            if (!localPath.empty())
            {
                // Try the local folder.
                p = localPath / name;
            }
            else
            {
                continue;
            }
        }
        else if (dir == "*")
        {
            App::fileSystem().findAll(name + ".ds", matching);
            if (matching.empty())
            {
                continue;
            }
            matching.sort([](File *a, File *b) {
                DE_ASSERT(a != b);
                return de::cmp(a->status().modifiedAt, b->status().modifiedAt) < 0;
            });
            found = matching.back();
            LOG_SCR_VERBOSE("Chose ") << found->path() << " out of " << dint(matching.size())
                                      << " candidates (latest modified)";
        }
        else
        {
            p = dir / name;
        }
        if (!found)
        {
            found = App::rootFolder().tryLocateFile(p + ".ds");
        }
        if (found)
        {
            return found;
        }
    }

    return nullptr;
}

const File &ScriptSystem::findModuleSource(const String &name, const String &localPath)
{
    const File *src = tryFindModuleSource(name, localPath);
    if (!src)
    {
        /// @throw NotFoundError  Module could not be found.
        throw NotFoundError("ScriptSystem::findModuleSource", "Cannot find module '" + name + "'");
    }
    return *src;
}

Record &ScriptSystem::builtInClass(const String &name)
{
    return builtInClass(DE_STR("Core"), name);
}

Record &ScriptSystem::builtInClass(const String &nativeModuleName, const String &className)
{
    return const_cast<Record &>(ScriptSystem::get().nativeModule(nativeModuleName)
                                .getr(className).dereference());
}

ScriptSystem &ScriptSystem::get()
{
    DE_ASSERT(_scriptSystem);
    return *_scriptSystem;
}

Record &ScriptSystem::importModule(const String &name, const String &importedFromPath)
{
    LOG_AS("ScriptSystem::importModule");

    // There are some special native modules.
    {
        DE_GUARD_FOR(d->nativeModules, G);
        auto foundNative = d->nativeModules.value.find(name);
        if (foundNative != d->nativeModules.value.end())
        {
            return *foundNative->second;
        }
    }

    // Maybe we already have this module?
    auto found = d->modules.find(name);
    if (found != d->modules.end())
    {
        return found->second->names();
    }

    // Get it from a file, then.
    const File *src = tryFindModuleSource(name, importedFromPath.fileNamePath());
    if (src)
    {
        Module *module = new Module(*src);
        d->modules.insert(name, module); // owned
        return module->names();
    }

    throw NotFoundError("ScriptSystem::importModule", "Cannot find module '" + name + "'");
}

void ScriptSystem::timeChanged(const Clock &)
{
    // perform time-based processing/scheduling/events
}

} // namespace de
