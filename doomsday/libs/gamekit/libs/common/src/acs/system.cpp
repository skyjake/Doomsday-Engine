/** @file system.cpp  Action Code Script (ACS) system.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "acs/system.h"

#include <de/iserializable.h>
#include <de/log.h>
#include <de/nativepath.h>
#include "acs/module.h"
#include "acs/script.h"
#include "gamesession.h"

using namespace de;

namespace acs {

DE_PIMPL_NOREF(System)
{
    std::unique_ptr<Module> currentModule;
    List<Script *> scripts;  ///< Scripts for the current module (if any).

    /**
     * When a script must be started on a map that is not currently loaded -
     * a deferred task is enqueued.
     */
    class ScriptStartTask : public ISerializable
    {
    public:
        res::Uri mapUri;           ///< Unique identifier of the target map.
        dint32 scriptNumber;      ///< Script number to execute on the target map.
        Script::Args scriptArgs;

        ScriptStartTask() : scriptNumber(-1) {}
        ScriptStartTask(const res::Uri &mapUri, dint32 scriptNumber, const Script::Args &scriptArgs)
            : mapUri      (mapUri)
            , scriptNumber(scriptNumber)
            , scriptArgs  (scriptArgs)
        {}

        static ScriptStartTask *newFromReader(de::Reader &from)
        {
            std::unique_ptr<ScriptStartTask> task(new ScriptStartTask);
            from >> *task;
            return task.release();
        }

        void operator >> (de::Writer &to) const
        {
            to << mapUri.compose()
               << scriptNumber;
            for(const dbyte &arg : scriptArgs) to << arg;
        }

        void operator << (de::Reader &from)
        {
            String mapUriStr;
            from >> mapUriStr;
            mapUri = res::makeUri(mapUriStr);
            if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");

            from >> scriptNumber;
            for(dbyte &arg : scriptArgs) from >> arg;
        }
    };
    List<ScriptStartTask *> tasks;

    ~Impl()
    {
        clearTasks();
        unloadModule();
    }

    void unloadModule()
    {
        clearScripts();
        currentModule.release();
    }

    void clearScripts()
    {
        deleteAll(scripts); scripts.clear();
    }

    void makeScripts()
    {
        clearScripts();

        currentModule->forAllEntryPoints([this] (const Module::EntryPoint &ep)
        {
            scripts << new Script(ep);
            return LoopContinue;
        });
    }

    void clearTasks()
    {
        deleteAll(tasks); tasks.clear();
    }
};

System::System() : d(new Impl)
{
    mapVars.fill(0);
    worldVars.fill(0);
}

void System::reset()
{
    d->clearTasks();
    d->unloadModule();
    mapVars.fill(0);
    worldVars.fill(0);
}

void System::loadModuleForMap(const res::Uri &mapUri)
{
#if __JHEXEN__
    if(IS_CLIENT) return;

    // Only one module may be loaded at once...
    d->unloadModule();

    if(mapUri.isEmpty()) return;

    /// @todo Should be using MapManifest here...
    const lumpnum_t markerLumpNum = CentralLumpIndex().findLast(mapUri.path() + ".lmp");
    const lumpnum_t moduleLumpNum = markerLumpNum + 11 /*ML_BEHAVIOR*/;
    if(!CentralLumpIndex().hasLump(moduleLumpNum)) return;

    res::File1 &file = CentralLumpIndex()[moduleLumpNum];
    if(!Module::recognize(file)) return;

    // Attempt to load the new module.
    try
    {
        d->currentModule.reset(Module::newFromFile(file));
        d->makeScripts();
    }
    catch(const Module::FormatError &er)
    {
        // Empty file / invalid bytecode.
        LOG_SCR_WARNING("File %s:%s does not appear to be valid ACS bytecode\n")
                << NativePath(file.container().composePath())
                << file.name()
                << er.asText();
    }
#else
    DE_UNUSED(mapUri);
#endif
}

const Module &System::module() const
{
    DE_ASSERT(bool( d->currentModule ));
    return *d->currentModule;
}

dint System::scriptCount() const
{
    return d->scripts.count();
}

bool System::hasScript(dint scriptNumber) const
{
    for(const Script *script : d->scripts)
    {
        if(script->entryPoint().scriptNumber == scriptNumber)
        {
            return true;
        }
    }
    return false;
}

Script &System::script(dint scriptNumber) const
{
    for(const Script *script : d->scripts)
    {
        if(script->entryPoint().scriptNumber == scriptNumber)
        {
            return *const_cast<Script *>(script);
        }
    }
    /// @throw MissingScriptError  Invalid script number specified.
    throw MissingScriptError("acs::System::script", "Unknown script #" + String::asText(scriptNumber));
}

LoopResult System::forAllScripts(std::function<LoopResult (Script &)> func) const
{
    for(Script *script : d->scripts)
    {
        if(auto result = func(*script)) return result;
    }
    return LoopContinue;
}

bool System::deferScriptStart(const res::Uri &mapUri, dint scriptNumber,
    const Script::Args &scriptArgs)
{
    DE_ASSERT(!IS_CLIENT);
    DE_ASSERT(gfw_Session()->mapUri() != mapUri);
    LOG_AS("acs::System");

    // Don't defer tasks in deathmatch.
    /// @todo Why the restriction? -ds
    if (gfw_Rule(deathmatch))
        return true;

    // Don't allow duplicates.
    for(const Impl::ScriptStartTask *task : d->tasks)
    {
        if(task->scriptNumber == scriptNumber &&
           task->mapUri       == mapUri)
        {
            return false;
        }
    }

    // Add it to the store to be started when that map is next entered.
    d->tasks << new Impl::ScriptStartTask(mapUri, scriptNumber, scriptArgs);
    return true;
}

Block System::serializeWorldState() const
{
    Block data;
    de::Writer writer(data);

    // Write the world-global variable namespace.
    for(const auto &var : worldVars) writer << var;

    // Write the deferred task queue.
    writer << dint32( d->tasks.count() );
    for(const auto *task : d->tasks) writer << *task;

    return data;
}

void System::readWorldState(de::Reader &from)
{
    from.seek(sizeof(duint32)); /// @todo fixme: Where is this being written?

    // Read the world-global variable namespace.
    for(auto &var : worldVars) from >> var;

    // Read the deferred task queue.
    d->clearTasks();
    dint32 numTasks;
    from >> numTasks;
    for(dint32 i = 0; i < numTasks; ++i)
    {
        d->tasks << Impl::ScriptStartTask::newFromReader(from);
    }
}

void System::writeMapState(MapStateWriter *msw) const
{
    writer_s *writer = msw->writer();

    // Write each script state.
    for(const auto *script : d->scripts) script->write(writer);

    // Write each variable.
    for(const auto &var : mapVars) Writer_WriteInt32(writer, var);
}

void System::readMapState(MapStateReader *msr)
{
    reader_s *reader = msr->reader();

    // Read each script state.
    for(auto *script : d->scripts) script->read(reader);

    // Read each variable.
    for(auto &var : mapVars) var = Reader_ReadInt32(reader);
}

void System::runDeferredTasks(const res::Uri &mapUri)
{
    LOG_AS("acs::System");
    for(dint i = 0; i < d->tasks.count(); ++i)
    {
        Impl::ScriptStartTask *task = d->tasks[i];
        if(task->mapUri != mapUri) continue;

        if(hasScript(task->scriptNumber))
        {
            script(task->scriptNumber)
                .start(task->scriptArgs, nullptr, nullptr, 0, TICSPERSEC);
        }
        else
        {
            LOG_SCR_WARNING("Unknown script #%i") << task->scriptNumber;
        }

        delete d->tasks.takeAt(i);
        i -= 1;
    }
}

void System::worldSystemMapChanged()
{
    mapVars.fill(0);

    for(Script *script : d->scripts)
    {
        if(script->entryPoint().startWhenMapBegins)
        {
            bool justStarted = script->start(Script::Args()/*default args*/,
                                             nullptr, nullptr, 0, TICSPERSEC);
            DE_ASSERT(justStarted);
            DE_UNUSED(justStarted);
        }
    }
}

D_CMD(InspectACScript)
{
    DE_UNUSED(src, argc);
    System &scriptSys       = gfw_Session()->acsSystem();
    const dint scriptNumber = String(argv[1]).toInt();

    if(!scriptSys.hasScript(scriptNumber))
    {
        if(scriptSys.scriptCount())
        {
            LOG_SCR_WARNING("Unknown ACScript #%i") << scriptNumber;
        }
        else
        {
            LOG_SCR_MSG("No ACScripts are currently loaded");
        }
        return false;
    }

    const Script &script = scriptSys.script(scriptNumber);
    LOG_SCR_MSG("%s\n  %s") << script.describe() << script.description();
    return true;
}

D_CMD(ListACScripts)
{
    DE_UNUSED(src, argc, argv);
    System &scriptSys = gfw_Session()->acsSystem();
    
    if (scriptSys.scriptCount())
    {
        LOG_SCR_MSG("Available ACScripts:");
        scriptSys.forAllScripts([](const Script &script) {
            LOG_SCR_MSG("  %s") << script.describe();
            return LoopContinue;
        });
        
#ifdef DE_DEBUG
        LOG_SCR_MSG("World variables:");
        dint idx = 0;
        for (const dint &var : scriptSys.worldVars)
        {
            LOG_SCR_MSG("  #%i: %i") << (idx++) << var;
        }
        
        LOG_SCR_MSG("Map variables:");
        idx = 0;
        for (const dint &var : scriptSys.mapVars)
        {
            LOG_SCR_MSG("  #%i: %i") << (idx++) << var;
        }
#endif
    }
    else
    {
        LOG_SCR_MSG("No ACScripts are currently loaded");
    }
    return true;
}

void System::consoleRegister()  // static
{
    C_CMD("inspectacscript",        "i", InspectACScript);
    /* Alias */ C_CMD("scriptinfo", "i", InspectACScript);
    C_CMD("listacscripts",          "",  ListACScripts);
    /* Alias */ C_CMD("scriptinfo", "",  ListACScripts);
}

}  // namespace acs
