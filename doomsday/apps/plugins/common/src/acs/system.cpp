/** @file system.cpp  Action Code Script (ACS) system.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/ISerializable>
#include <de/Log>
#include <de/NativePath>
#include "acs/interpreter.h"
#include "acs/script.h"
#include "gamesession.h"

using namespace de;

namespace internal
{
    /**
     * Bytecode header. Read directly from the data resource file.
     */
#pragma pack(1)
    struct BytecodeHeader
    {
        dint32 marker;
        dint32 infoOffset;
        dint32 code;
    };
#pragma pack()

    /**
     * A deferred task is enqueued when a script is started on a map not currently loaded.
     */
    class DeferredTask : public ISerializable
    {
    public:
        de::Uri mapUri;                ///< Unique identifier of the target map.
        dint32 scriptNumber;           ///< Script number to execute on the target map.
        acs::Script::Args scriptArgs;

        DeferredTask() : scriptNumber(-1) {}
        DeferredTask(de::Uri const &mapUri, dint32 scriptNumber, acs::Script::Args const &scriptArgs)
            : mapUri      (mapUri)
            , scriptNumber(scriptNumber)
            , scriptArgs  (scriptArgs)
        {}
        DeferredTask(DeferredTask const &other)
            : mapUri      (other.mapUri)
            , scriptNumber(other.scriptNumber)
            , scriptArgs  (other.scriptArgs)
        {}

        static DeferredTask *newFromReader(de::Reader &from)
        {
            std::unique_ptr<DeferredTask> task(new DeferredTask);
            from >> *task;
            return task.release();
        }

        void operator >> (de::Writer &to) const
        {
            to << mapUri.compose()
               << scriptNumber;
            for(dbyte const &arg : scriptArgs) to << arg;
        }

        void operator << (de::Reader &from)
        {
            String mapUriStr;
            from >> mapUriStr;
            mapUri = de::Uri(mapUriStr, RC_NULL);
            if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");

            from >> scriptNumber;
            for(dbyte &arg : scriptArgs) from >> arg;
        }
    };

}  // namespace internal
using namespace internal;

namespace acs {

DENG2_PIMPL(System)
{
    byte const *pcode = nullptr;  ///< Loaded bytecode (if any).
    QList<Script *> scripts;
    QList<String> strings;

    QList<DeferredTask *> deferredTasks;

    Instance(Public *i) : Base(i) {}
    ~Instance()
    {
        clearDeferredTasks();
        clearScripts();
    }

    void clearScripts()
    {
        qDeleteAll(scripts); scripts.clear();
    }

    void clearStrings()
    {
        strings.clear();
    }

    void clearDeferredTasks()
    {
        qDeleteAll(deferredTasks); deferredTasks.clear();
    }

    void runDeferredTasks(de::Uri const &mapUri)
    {
        for(int i = 0; i < deferredTasks.count(); ++i)
        {
            DeferredTask *task = deferredTasks[i];
            if(task->mapUri != mapUri) continue;

            if(self.hasScript(task->scriptNumber))
            {
                self.script(task->scriptNumber)
                        .start(task->scriptArgs, nullptr, nullptr, 0, TICSPERSEC);
            }
            else
            {
                LOG_SCR_WARNING("Unknown script #%i") << task->scriptNumber;
            }

            delete deferredTasks.takeAt(i);
            i -= 1;
        }
    }
};

System::System() : d(new Instance(this))
{
    mapVars.fill(0);
    worldVars.fill(0);
}

void System::loadBytecode(File1 &file)
{
#define OPEN_SCRIPTS_BASE 1000

    DENG2_ASSERT(!IS_CLIENT);

    LOG_AS("acs::System");
    LOG_SCR_VERBOSE("Loading bytecode from %s:%s...")
            << NativePath(file.container().composePath()).pretty()
            << file.name();

    d->clearScripts();
    d->clearStrings();

    int numScripts = 0;

    int const *readBuf = 0;
    if(file.size() >= sizeof(BytecodeHeader))
    {
        void *region = Z_Malloc(file.size(), PU_MAP, nullptr);
        file.read((uint8_t *)region);
        BytecodeHeader const *header = (BytecodeHeader const *) region;
        d->pcode = (byte const *) header;

        if(LONG(header->infoOffset) < (int)file.size())
        {
            readBuf = (int *) ((byte const *) header + LONG(header->infoOffset));
            numScripts = LONG(*readBuf++);
        }
    }

    if(!numScripts)
    {
        // Empty file / invalid bytecode.
        LOG_SCR_WARNING("File %s:%s does not appear to be valid ACS bytecode")
                << NativePath(file.container().composePath())
                << file.name();
        return;
    }

    for(int i = 0; i < numScripts; ++i)
    {
        Script::EntryPoint ep;

        ep.scriptNumber = LONG(*readBuf++);
        if(ep.scriptNumber >= OPEN_SCRIPTS_BASE)
        {
            ep.scriptNumber -= OPEN_SCRIPTS_BASE;
            ep.startWhenMapBegins = true;
        }
        ep.pcodePtr = (int const *)(d->pcode + LONG(*readBuf++));
        ep.scriptArgCount = LONG(*readBuf++);
        if(ep.scriptArgCount > ACS_INTERPRETER_MAX_SCRIPT_ARGS)
            throw Error("acs::System::loadBytecode", "Too many script arguments (" + String::number(ep.scriptArgCount) + " > " + String::number(ACS_INTERPRETER_MAX_SCRIPT_ARGS) + ")");

        d->scripts << new Script(ep/*make a copy*/);
    }

    int const numStrings = LONG(*readBuf++);
    for(int i = 0; i < numStrings; ++i)
    {
        d->strings << String((char const *)(d->pcode + LONG(*readBuf++)));
    }

#undef OPEN_SCRIPTS_BASE
}

void System::reset()
{
    worldVars.fill(0);
    mapVars.fill(0);
    d->clearDeferredTasks();
}

int System::scriptCount() const
{
    return d->scripts.count();
}

bool System::hasScript(int scriptNumber)
{
    for(Script const *script : d->scripts)
    {
        if(script->entryPoint().scriptNumber == scriptNumber)
        {
            return true;
        }
    }
    return false;
}

Script &System::script(int scriptNumber) const
{
    for(Script const *script : d->scripts)
    {
        if(script->entryPoint().scriptNumber == scriptNumber)
        {
            return *const_cast<Script *>(script);
        }
    }
    /// @throw MissingScriptError  Invalid script number specified.
    throw MissingScriptError("acs::System::script", "Unknown script #" + String::number(scriptNumber));
}

LoopResult System::forAllScripts(std::function<LoopResult (Script &)> func) const
{
    for(Script *script : d->scripts)
    {
        if(auto result = func(*script)) return result;
    }
    return LoopContinue;
}

bool System::deferScriptStart(de::Uri const &mapUri, int scriptNumber,
    Script::Args const &scriptArgs)
{
    DENG2_ASSERT(!IS_CLIENT);
    DENG2_ASSERT(COMMON_GAMESESSION->mapUri() != mapUri);
    LOG_AS("acs::System");

    // Don't defer tasks in deathmatch.
    /// @todo Why the restriction? -ds
    if(COMMON_GAMESESSION->rules().deathmatch)
        return true;

    // Don't allow duplicates.
    for(DeferredTask const *task : d->deferredTasks)
    {
        if(task->scriptNumber == scriptNumber &&
           task->mapUri       == mapUri)
        {
            return false;
        }
    }

    // Add it to the store to be started when that map is next entered.
    d->deferredTasks << new DeferredTask(mapUri, scriptNumber, scriptArgs);
    return true;
}

byte const *System::pcode() const
{
    return d->pcode;
}

String System::stringConstant(int stringNumber) const
{
    if(stringNumber >= 0 && stringNumber < d->strings.count())
    {
        return d->strings[stringNumber];
    }
    /// @throw MissingStringError  Invalid string-constant number specified.
    throw MissingStringError("acs::System::stringConstant", "Unknown string-constant #" + String::number(stringNumber));
}

de::Block System::serializeWorldState() const
{
    de::Block data;
    de::Writer writer(data);

    // Write the world-global variable namespace.
    for(int const &var : worldVars) writer << var;

    // Write the deferred task queue.
    writer << d->deferredTasks.count();
    for(DeferredTask const *task : d->deferredTasks) writer << *task;

    return data;
}

void System::readWorldState(de::Reader &from)
{
    // Read the world-global variable namespace.
    for(int &var : worldVars) from >> var;

    // Read the deferred task queue.
    d->clearDeferredTasks();
    int numDeferredTasks;
    from >> numDeferredTasks;
    for(int i = 0; i < numDeferredTasks; ++i)
    {
        d->deferredTasks << DeferredTask::newFromReader(from);
    }
}

void System::writeMapState(MapStateWriter *msw) const
{
    writer_s *writer = msw->writer();

    // Write each script state.
    for(Script const *script : d->scripts) script->write(writer);

    // Write each variable.
    for(int const &var : mapVars) Writer_WriteInt32(writer, var);
}

void System::readMapState(MapStateReader *msr)
{
    reader_s *reader = msr->reader();

    // Read each script state.
    for(Script *script : d->scripts) script->read(reader);

    // Read each variable.
    for(int &var : mapVars) var = Reader_ReadInt32(reader);
}

void System::runDeferredTasks(de::Uri const &mapUri)
{
    LOG_AS("acs::System");
    d->runDeferredTasks(mapUri);
}

D_CMD(InspectACScript)
{
    DENG2_UNUSED2(src, argc);
    System &scriptSys      = Game_ACScriptSystem();
    int const scriptNumber = String(argv[1]).toInt();

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

    Script const &script = scriptSys.script(scriptNumber);
    LOG_SCR_MSG("%s\n%s") << script.describe() << script.description();
    return true;
}

D_CMD(ListACScripts)
{
    DENG2_UNUSED3(src, argc, argv);
    System &scriptSys = Game_ACScriptSystem();

    if(scriptSys.scriptCount())
    {
        LOG_SCR_MSG("World variables:");
        int idx = 0;
        for(int const &var : scriptSys.worldVars)
        {
            LOG_SCR_MSG("  #%i: %i") << (idx++) << var;
        }

        LOG_SCR_MSG("Map variables:");
        idx = 0;
        for(int const &var : scriptSys.mapVars)
        {
            LOG_SCR_MSG("  #%i: %i") << (idx++) << var;
        }

        LOG_SCR_MSG("Available ACScripts:");
        scriptSys.forAllScripts([&scriptSys] (Script const &script)
        {
            LOG_SCR_MSG("  %s") << script.describe();
            return LoopContinue;
        });
    }
    else
    {
        LOG_SCR_MSG("No ACScripts are currently loaded");
    }
    return true;
}

void System::consoleRegister()  // static
{
    C_CMD("scriptinfo",  "i",    InspectACScript);
    C_CMD("scriptinfo",  "",     ListACScripts);
}

}  // namespace acs

static acs::System scriptSys;  ///< The One acs::System instance.

acs::System &Game_ACScriptSystem()
{
    return scriptSys;
}

// C wrapper API: --------------------------------------------------------------

dd_bool Game_ACScriptSystem_StartScript(int scriptNumber, uri_s const *mapUri_,
    byte const args[], mobj_t *activator, Line *line, int side)
{
    de::Uri const *mapUri = reinterpret_cast<de::Uri const *>(mapUri_);
    acs::Script::Args scriptArgs(args, 4);

    if(!mapUri || COMMON_GAMESESSION->mapUri() == *mapUri)
    {
        if(scriptSys.hasScript(scriptNumber))
        {
            return scriptSys.script(scriptNumber).start(scriptArgs, activator, line, side);
        }
        return false;
    }

    return scriptSys.deferScriptStart(*mapUri, scriptNumber, scriptArgs);
}
