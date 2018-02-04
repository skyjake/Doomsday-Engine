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

#include "de/ScriptSystem"
#include "de/Animation"
#include "de/App"
#include "de/ArrayValue"
#include "de/BlockValue"
#include "de/Config"
#include "de/DictionaryValue"
#include "de/FileSystem"
#include "de/Module"
#include "de/NumberValue"
#include "de/Record"
#include "de/RecordValue"
#include "de/TextValue"
#include "de/Version"
#include "de/math.h"

#include "../src/scriptsys/bindings_core.h"
#include "../src/scriptsys/bindings_math.h"

#include <QMap>

namespace de {

static ScriptSystem *_scriptSystem = 0;

DENG2_PIMPL(ScriptSystem)
, DENG2_OBSERVES(Record, Deletion)
{
    Binder binder;

    /// Built-in special modules. These are constructed by native code and thus not
    /// parsed from any script.
    typedef QHash<String, Record *> NativeModules;
    LockableT<NativeModules> nativeModules; // not owned
    Record coreModule;    ///< Script: built-in script classes and functions.
    Record mathModule;    ///< Math: math related functions.
    Record versionModule; ///< Version: information about the platform and build

    /// Resident modules.
    typedef QHash<String, Module *> Modules; // owned
    Modules modules;

    QList<Path> additionalImportPaths;

    Impl(Public *i) : Base(*i)
    {
        initCoreModule();
        initMathModule(binder, mathModule);
        addNativeModule("Math", mathModule);

        // Setup the Version module.
        {
            Version const ver = Version::currentBuild();
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
#ifdef DENG_STABLE
            mod.addBoolean("STABLE",   true).setReadOnly();
#else
            mod.addBoolean("STABLE",   false).setReadOnly();
#endif
            addNativeModule("Version", mod);
        }
    }

    ~Impl()
    {
        qDeleteAll(modules);
    }

    static Value *Function_ImportPath(Context &, Function::ArgumentValues const &)
    {
        DENG2_ASSERT(_scriptSystem != nullptr);

        StringList importPaths;
        _scriptSystem->d->listImportPaths(importPaths);

        auto *array = new ArrayValue;
        for (auto const &path : importPaths)
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
                << DENG2_FUNC_NOARG(ImportPath, "importPath");

        addNativeModule("Core", coreModule);
    }

    void addNativeModule(String const &name, Record &module)
    {
        DENG2_GUARD(nativeModules);
        nativeModules.value.insert(name, &module); // not owned
        module.audienceForDeletion() += this;
    }

    void removeNativeModule(String const &name)
    {
        DENG2_GUARD(nativeModules);
        if (!nativeModules.value.contains(name)) return;

        nativeModules.value[name]->audienceForDeletion() -= this;
        nativeModules.value.remove(name);
    }

    void recordBeingDeleted(Record &record)
    {
        DENG2_GUARD(nativeModules);
        QMutableHashIterator<String, Record *> iter(nativeModules);
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
        ArrayValue const *importPath;
        try
        {
            importPath = &App::config().geta("importPath");
        }
        catch (Record::NotFoundError const &)
        {
            importPath = defaultImportPath.get();
        }

        // Compile a list of all possible import locations.
        importPaths.clear();
        DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, importPath->elements())
        {
            importPaths << (*i)->asText();
        }
        foreach (Path const &path, additionalImportPaths)
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
    _scriptSystem = 0;
}

void ScriptSystem::addModuleImportPath(Path const &path)
{
    d->additionalImportPaths << path;
}

void ScriptSystem::removeModuleImportPath(Path const &path)
{
    d->additionalImportPaths.removeOne(path);
}

void ScriptSystem::addNativeModule(String const &name, Record &module)
{
    d->addNativeModule(name, module);
}

void ScriptSystem::removeNativeModule(String const &name)
{
    d->removeNativeModule(name);
}

Record &ScriptSystem::nativeModule(String const &name)
{
    DENG2_GUARD_FOR(d->nativeModules, G);
    Impl::NativeModules::const_iterator foundNative = d->nativeModules.value.constFind(name);
    DENG2_ASSERT(foundNative != d->nativeModules.value.constEnd());
    return *foundNative.value();
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
            return found.value()->names();
        }
    }
    throw NotFoundError("ScriptSystem::operator[]", "Module not found: " + name);
}

bool ScriptSystem::nativeModuleExists(const String &name) const
{
    DENG2_GUARD_FOR(d->nativeModules, G);
    return d->nativeModules.value.contains(name);
}

StringList ScriptSystem::nativeModules() const
{
    DENG2_GUARD_FOR(d->nativeModules, G);
    return d->nativeModules.value.keys();
}

namespace internal {
    static bool sortFilesByModifiedAt(File *a, File *b) {
        DENG2_ASSERT(a != b);
        return de::cmp(a->status().modifiedAt, b->status().modifiedAt) < 0;
    }
}

File const *ScriptSystem::tryFindModuleSource(String const &name, String const &localPath)
{
    // Compile a list of all possible import locations.
    StringList importPaths;
    d->listImportPaths(importPaths);

    // Search all import locations.
    foreach (String dir, importPaths)
    {
        String p;
        FS::FoundFiles matching;
        File *found = 0;
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
            matching.sort(internal::sortFilesByModifiedAt);
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

    return 0;
}

File const &ScriptSystem::findModuleSource(String const &name, String const &localPath)
{
    File const *src = tryFindModuleSource(name, localPath);
    if (!src)
    {
        /// @throw NotFoundError  Module could not be found.
        throw NotFoundError("ScriptSystem::findModuleSource", "Cannot find module '" + name + "'");
    }
    return *src;
}

Record &ScriptSystem::builtInClass(String const &name)
{
    return builtInClass(QStringLiteral("Core"), name);
}

Record &ScriptSystem::builtInClass(String const &nativeModuleName, String const &className)
{
    return const_cast<Record &>(ScriptSystem::get().nativeModule(nativeModuleName)
                                .getr(className).dereference());
}

ScriptSystem &ScriptSystem::get()
{
    DENG2_ASSERT(_scriptSystem);
    return *_scriptSystem;
}

Record &ScriptSystem::importModule(String const &name, String const &importedFromPath)
{
    LOG_AS("ScriptSystem::importModule");

    // There are some special native modules.
    {
        DENG2_GUARD_FOR(d->nativeModules, G);
        Impl::NativeModules::const_iterator foundNative = d->nativeModules.value.constFind(name);
        if (foundNative != d->nativeModules.value.constEnd())
        {
            return *foundNative.value();
        }
    }

    // Maybe we already have this module?
    Impl::Modules::iterator found = d->modules.find(name);
    if (found != d->modules.end())
    {
        return found.value()->names();
    }

    // Get it from a file, then.
    File const *src = tryFindModuleSource(name, importedFromPath.fileNamePath());
    if (src)
    {
        Module *module = new Module(*src);
        d->modules.insert(name, module); // owned
        return module->names();
    }

    throw NotFoundError("ScriptSystem::importModule", "Cannot find module '" + name + "'");
}

void ScriptSystem::timeChanged(Clock const &)
{
    // perform time-based processing/scheduling/events
}

} // namespace de
