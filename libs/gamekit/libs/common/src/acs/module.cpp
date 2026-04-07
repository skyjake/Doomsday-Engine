/** @file module.cpp  Action Code Script (ACS) module.
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

#include "common.h"           // IS_CLIENT
#include "acs/module.h"

#include <de/list.h>
#include <de/keymap.h>
#include <de/log.h>
#include "acs/interpreter.h"  // ACS_INTERPRETER_MAX_SCRIPT_ARGS
#include "gamesession.h"

using namespace de;

namespace acs {

DE_PIMPL_NOREF(Module)
{
    Block                  pcode;
    List<EntryPoint>       entryPoints;
    KeyMap<int, EntryPoint *> epByScriptNumberLut;
    List<String>           constants;

    void buildEntryPointLut()
    {
        epByScriptNumberLut.clear();
        for (auto &ep : entryPoints)
        {
            epByScriptNumberLut.insert(ep.scriptNumber, &ep);
        }
    }
};

Module::Module() : d(new Impl)
{}

bool Module::recognize(const res::File1 &file)  // static
{
    if(file.size() <= 4) return false;

    // ACS bytecode begins with the magic identifier "ACS".
    Block magic(4);
    const_cast<res::File1 &>(file).read(magic.data(), 0, 4);
    if(!magic.beginsWith("ACS")) return false;

    // ZDoom uses the fourth byte for versioning of their extended formats.
    // Currently such formats are not supported.
    return magic.at(3) == 0;
}

Module *Module::newFromBytecode(const Block &bytecode)  // static
{
    DE_ASSERT(!IS_CLIENT);
    LOG_AS("acs::Module");

    std::unique_ptr<Module> module(new Module);

    // Copy the complete bytecode data into a local buffer (we'll be randomly
    // accessing this frequently).
    module->d->pcode = bytecode;

    de::Reader from(module->d->pcode);
    dint32 magic, scriptInfoOffset;
    from >> magic >> scriptInfoOffset;

    // Read script entry point info.
    from.seek(dint(scriptInfoOffset) - dint(from.offset()));
    dint32 numEntryPoints;
    from >> numEntryPoints;
    module->d->entryPoints.reserve(numEntryPoints);
    for(dint32 i = 0; i < numEntryPoints; ++i)
    {
#define OPEN_SCRIPTS_BASE 1000

        EntryPoint ep;

        from >> ep.scriptNumber;
        if(ep.scriptNumber >= OPEN_SCRIPTS_BASE)
        {
            ep.scriptNumber -= OPEN_SCRIPTS_BASE;
            ep.startWhenMapBegins = true;
        }

        dint32 offset;
        from >> offset;
        if(offset > dint32(module->d->pcode.size()))
        {
            throw FormatError("acs::Module", "Invalid script entrypoint offset");
        }
        ep.pcodePtr = reinterpret_cast<const int *>(module->d->pcode.constData() + offset);

        from >> ep.scriptArgCount;
        if(ep.scriptArgCount > ACS_INTERPRETER_MAX_SCRIPT_ARGS)
        {
            throw FormatError("acs::Module",
                              "Too many script arguments (" + String::asText(ep.scriptArgCount) +
                                  " > " + String::asText(ACS_INTERPRETER_MAX_SCRIPT_ARGS) + ")");
        }

        module->d->entryPoints << ep;  // makes a copy.

#undef OPEN_SCRIPTS_BASE
    }
    // Prepare a script-number => EntryPoint LUT.
    module->d->buildEntryPointLut();

    // Read constant (string-)values.
    dint32 numConstants;
    from >> numConstants;
    List<dint32> constantOffsets;
//    constantOffsets.reserve(numConstants);
    for(dint32 i = 0; i < numConstants; ++i)
    {
        dint32 offset;
        from >> offset;
        constantOffsets << offset;
    }
    for (const auto &offset : constantOffsets)
    {
        Block utf;
        from.setOffset(dsize(offset));
        from.readUntil(utf, 0, false /*delimiter omitted*/);
        module->d->constants << String::fromUtf8(utf);
    }

    return module.release();
}

Module *Module::newFromFile(const res::File1 &file)  // static
{
    DE_ASSERT(!IS_CLIENT);
    LOG_AS("acs::Module");
    LOG_SCR_VERBOSE("Loading from %s:%s...")
            << NativePath(file.container().composePath()).pretty()
            << file.name();

    // Buffer the whole file.
    Block buffer(file.size());
    const_cast<res::File1 &>(file).read(buffer.data());

    return newFromBytecode(buffer);
}

String Module::constant(int stringNumber) const
{
    if(stringNumber >= 0 && stringNumber < d->constants.count())
    {
        return d->constants[stringNumber];
    }
    /// @throw MissingConstantError  Invalid constant (string-)value number specified.
    throw MissingConstantError("acs::Module::constant", "Unknown constant #" + String::asText(stringNumber));
}

int Module::entryPointCount() const
{
    return d->entryPoints.count();
}

bool Module::hasEntryPoint(int scriptNumber) const
{
    return d->epByScriptNumberLut.contains(scriptNumber);
}

const Module::EntryPoint &Module::entryPoint(int scriptNumber) const
{
    if(hasEntryPoint(scriptNumber)) return *d->epByScriptNumberLut[scriptNumber];
    /// @throw MissingEntryPointError  Invalid script number specified.
    throw MissingEntryPointError("acs::Module::entryPoint", "Unknown script #" + String::asText(scriptNumber));
}

LoopResult Module::forAllEntryPoints(const std::function<LoopResult (EntryPoint &)>& func) const
{
    for(EntryPoint &ep : d->entryPoints)
    {
        if(auto result = func(ep)) return result;
    }
    return LoopContinue;
}

const Block &Module::pcode() const
{
    return d->pcode;
}

} // namespace acs
