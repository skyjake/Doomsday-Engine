/** @file
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/console/cmd.h"
#include "doomsday/console/alias.h"
#include "doomsday/console/knownword.h"
#include "doomsday/help.h"
#include <de/memoryblockset.h>
#include <de/memoryzone.h>
#include <de/c_wrapper.h>
#include <de/Log>
#include <de/App>
#include <QMap>

using namespace de;

static ccmd_t *ccmdListHead;

/// @todo Replace with a data structure that allows for deletion of elements.
static blockset_t *ccmdBlockSet;

/// Running total of the number of uniquely-named commands.
static uint numUniqueNamedCCmds;

static QMap<String, String> mappedConfigVariables;

void Con_InitCommands()
{
    ccmdListHead = 0;
    ccmdBlockSet = 0;
    numUniqueNamedCCmds = 0;
}

void Con_ClearCommands(void)
{
    if(ccmdBlockSet)
    {
        BlockSet_Delete(ccmdBlockSet);
    }
    ccmdBlockSet = 0;
    ccmdListHead = 0;
    numUniqueNamedCCmds = 0;
}

void Con_AddKnownWordsForCommands()
{
    /// @note ccmd list is NOT yet sorted.
    for(ccmd_t* ccmd = ccmdListHead; ccmd; ccmd = ccmd->next)
    {
        // Skip overloaded variants.
        if(ccmd->prevOverload) continue;

        Con_AddKnownWord(WT_CCMD, ccmd);
    }
}

void Con_AddCommand(ccmdtemplate_t const* ccmd)
{
    int minArgs, maxArgs;
    cvartype_t args[DENG_MAX_ARGS];
    ccmd_t* newCCmd, *overloaded = 0;

    if(!ccmd) return;

    DENG_ASSERT(ccmd->name != 0);

    // Decode the usage string if present.
    if(ccmd->argTemplate != 0)
    {
        size_t l, len;
        cvartype_t type = CVT_NULL;
        dd_bool unlimitedArgs;
        char c;

        len = strlen(ccmd->argTemplate);
        minArgs = 0;
        unlimitedArgs = false;
        for(l = 0; l < len; ++l)
        {
            c = ccmd->argTemplate[l];
            switch(c)
            {
            // Supported type symbols:
            case 'b': type = CVT_BYTE;     break;
            case 'i': type = CVT_INT;      break;
            case 'f': type = CVT_FLOAT;    break;
            case 's': type = CVT_CHARPTR;  break;

            // Special symbols:
            case '*':
                // Variable arg list.
                if(l != len-1)
                    App_FatalError("Con_AddCommand: CCmd '%s': '*' character "
                                   "not last in argument template: \"%s\".",
                                   ccmd->name, ccmd->argTemplate);

                unlimitedArgs = true;
                type = CVT_NULL; // Not a real argument.
                break;

            // Erroneous symbol:
            default:
                App_FatalError("Con_AddCommand: CCmd '%s': Invalid character "
                               "'%c' in argument template: \"%s\".", ccmd->name, c,
                               ccmd->argTemplate);
            }

            if(type != CVT_NULL)
            {
                if(minArgs >= DENG_MAX_ARGS)
                    App_FatalError("Con_AddCommand: CCmd '%s': Too many arguments. "
                                   "Limit is %i.", ccmd->name, DENG_MAX_ARGS);

                args[minArgs++] = type;
            }
        }

        // Set the min/max parameter counts for this ccmd.
        if(unlimitedArgs)
        {
            maxArgs = -1;
            if(minArgs == 0)
                minArgs = -1;
        }
        else
        {
            maxArgs = minArgs;
        }
    }
    else // It's usage is NOT validated by Doomsday.
    {
        minArgs = maxArgs = -1;
    }

    // Now check that the ccmd to be registered is unique.
    // We allow multiple ccmds with the same name if we can determine by
    // their paramater lists that they are unique (overloading).
    { ccmd_t* other;
    if((other = Con_FindCommand(ccmd->name)) != 0)
    {
        dd_bool unique = true;

        // The ccmd being registered is NOT a deng validated ccmd
        // and there is already an existing ccmd by this name?
        if(minArgs == -1 && maxArgs == -1)
            unique = false;

        if(unique)
        {
            // Check each variant.
            ccmd_t* variant = other;
            do
            {
                // An existing ccmd with no validation?
                if(variant->minArgs == -1 && variant->maxArgs == -1)
                    unique = false;
                // An existing ccmd with a lower minimum and no maximum?
                else if(variant->minArgs < minArgs && variant->maxArgs == -1)
                    unique = false;
                // An existing ccmd with a larger min and this ccmd has no max?
                else if(variant->minArgs > minArgs && maxArgs == -1)
                    unique = false;
                // An existing ccmd with the same minimum number of args?
                else if(variant->minArgs == minArgs)
                {
                    // \todo Implement support for paramater type checking.
                    unique = false;
                }

                // Sanity check.
                if(!unique && variant->execFunc == ccmd->execFunc)
                    App_FatalError("Con_AddCommand: A CCmd by the name '%s' is already registered and the callback funcs are "
                                   "the same, is this really what you wanted?", ccmd->name);
            } while((variant = variant->nextOverload) != 0);
        }

        if(!unique)
            App_FatalError("Con_AddCommand: A CCmd by the name '%s' is already registered. Their parameter lists would be ambiguant.", ccmd->name);

        overloaded = other;
    }}

    if(!ccmdBlockSet)
        ccmdBlockSet = BlockSet_New(sizeof(ccmd_t), 32);

    newCCmd = (ccmd_t*) BlockSet_Allocate(ccmdBlockSet);

    // Make a static copy of the name in the zone (this allows the source
    // data to change in case of dynamic registrations).
    char* nameCopy = (char*) Z_Malloc(strlen(ccmd->name) + 1, PU_APPSTATIC, NULL);
    if(!nameCopy) App_FatalError("Con_AddCommand: Failed on allocation of %lu bytes for command name.", (unsigned long) (strlen(ccmd->name) + 1));

    strcpy(nameCopy, ccmd->name);
    newCCmd->name = nameCopy;
    newCCmd->execFunc = ccmd->execFunc;
    newCCmd->flags = ccmd->flags;
    newCCmd->nextOverload = newCCmd->prevOverload = 0;
    newCCmd->minArgs = minArgs;
    newCCmd->maxArgs = maxArgs;
    memcpy(newCCmd->args, &args, sizeof(newCCmd->args));

    // Link it to the head of the global list of ccmds.
    newCCmd->next = ccmdListHead;
    ccmdListHead = newCCmd;

    if(!overloaded)
    {
        ++numUniqueNamedCCmds;
        Con_UpdateKnownWords();
        return;
    }

    // Link it to the head of the overload list.
    newCCmd->nextOverload = overloaded;
    overloaded->prevOverload = newCCmd;
}

void Con_AddCommandList(ccmdtemplate_t const* cmdList)
{
    if(!cmdList) return;
    for(; cmdList->name; ++cmdList)
    {
        Con_AddCommand(cmdList);
    }
}

ccmd_t *Con_FindCommand(char const *name)
{
    /// @todo Use a faster than O(n) linear search.
    if(name && name[0])
    {
        for(ccmd_t *ccmd = ccmdListHead; ccmd; ccmd = ccmd->next)
        {
            if(qstricmp(name, ccmd->name)) continue;

            // Locate the head of the overload list.
            while(ccmd->prevOverload) { ccmd = ccmd->prevOverload; }
            return ccmd;
        }
    }
    return 0;
}

/**
 * Outputs the usage information for the given ccmd to the console.
 *
 * @param ccmd          The ccmd to print the usage info for.
 * @param allOverloads  @c true= print usage info for all overloaded variants.
 *                      Otherwise only the info for @a ccmd.
 */
void Con_PrintCommandUsage(ccmd_t const *ccmd, bool allOverloads)
{
    if(!ccmd) return;

    if(allOverloads)
    {
        // Locate the head of the overload list.
        while(ccmd->prevOverload) { ccmd = ccmd->prevOverload; }
    }

    LOG_SCR_NOTE(_E(b) "Usage:" _E(.) "\n  " _E(>) + Con_CmdUsageAsStyledText(ccmd));

    if(allOverloads)
    {
        while((ccmd = ccmd->nextOverload))
        {
            LOG_SCR_MSG("  " _E(>) + Con_CmdUsageAsStyledText(ccmd));
        }
    }
}

ccmd_t *Con_FindCommandMatchArgs(cmdargs_t *args)
{
    if(!args) return 0;

    if(ccmd_t *ccmd = Con_FindCommand(args->argv[0]))
    {
        // Check each variant.
        ccmd_t *variant = ccmd;
        do
        {
            dd_bool invalidArgs = false;

            // Are we validating the arguments?
            // Note that strings are considered always valid.
            if(!(variant->minArgs == -1 && variant->maxArgs == -1))
            {
                // Do we have the right number of arguments?
                if(args->argc-1 < variant->minArgs)
                {
                    invalidArgs = true;
                }
                else if(variant->maxArgs != -1 && args->argc-1 > variant->maxArgs)
                {
                    invalidArgs = true;
                }
                else
                {
                    // Presently we only validate upto the minimum number of args.
                    /// @todo Validate non-required args.
                    for(int i = 0; i < variant->minArgs && !invalidArgs; ++i)
                    {
                        switch(variant->args[i])
                        {
                        case CVT_BYTE:
                            invalidArgs = !M_IsStringValidByte(args->argv[i+1]);
                            break;
                        case CVT_INT:
                            invalidArgs = !M_IsStringValidInt(args->argv[i+1]);
                            break;
                        case CVT_FLOAT:
                            invalidArgs = !M_IsStringValidFloat(args->argv[i+1]);
                            break;
                        default:
                            break;
                        }
                    }
                }
            }

            if(!invalidArgs)
            {
                return variant; // This is the one!
            }
        } while((variant = variant->nextOverload) != 0);

        // Perhaps the user needs some help.
        Con_PrintCommandUsage(ccmd);
    }

    // No command found, or none with matching arguments.
    return 0;
}

dd_bool Con_IsValidCommand(char const* name)
{
    if(!name || !name[0])
        return false;

    // Try the console commands first.
    if(Con_FindCommand(name) != 0)
        return true;

    // Try the aliases (aliai?) then.
    return (Con_FindAlias(name) != 0);
}

String Con_CmdUsageAsStyledText(ccmd_t const *ccmd)
{
    DENG2_ASSERT(ccmd != 0);

    if(ccmd->minArgs == -1 && ccmd->maxArgs == -1)
        return String();

    // Print the expected form for this ccmd.
    String argText;
    for(int i = 0; i < ccmd->minArgs; ++i)
    {
        switch(ccmd->args[i])
        {
        case CVT_BYTE:    argText += " (byte)";   break;
        case CVT_INT:     argText += " (int)";    break;
        case CVT_FLOAT:   argText += " (float)";  break;
        case CVT_CHARPTR: argText += " (string)"; break;

        default: break;
        }
    }
    if(ccmd->maxArgs == -1)
    {
        argText += " ...";
    }

    return _E(b) + String(ccmd->name) + _E(.) _E(l) + argText + _E(.);
}

String Con_CmdAsStyledText(ccmd_t *cmd)
{
    char const *str;
    if((str = DH_GetString(DH_Find(cmd->name), HST_DESCRIPTION)))
    {
        return String(_E(b) "%1 " _E(.) _E(>) _E(2) "%2" _E(.) _E(<)).arg(cmd->name).arg(str);
    }
    else
    {
        return String(_E(b) "%1" _E(.)).arg(cmd->name);
    }
}

D_CMD(MappedConfigVariable)
{
    DENG_UNUSED(src);

    // Look up the variable.
    auto const found = mappedConfigVariables.constFind(argv[0]);
    DENG2_ASSERT(found != mappedConfigVariables.constEnd()); // mapping must be defined

    Variable &var = App::config().names()[found.value()];

    if(argc == 1)
    {
        // No argumnets, just print the current value.
        LOG_SCR_MSG(_E(b) "%s" _E(.) " = " _E(>) "%s " _E(l)_E(C) "[Config.%s]")
                << argv[0]
                << var.value().asText()
                << found.value();
    }
    else if(argc > 1)
    {
        // Retain the current type of the Config variable (numeric or text).
        if(var.value().maybeAs<TextValue>())
        {
            var.set(new TextValue(argv[1]));
        }
        else
        {
            var.set(new NumberValue(String(argv[1]).toDouble()));
        }
    }
    return true;
}

void Con_AddMappedConfigVariable(char const *consoleName, char const *opts, String const &configVariable)
{
    DENG2_ASSERT(!mappedConfigVariables.contains(consoleName)); // redefining not handled

    mappedConfigVariables.insert(consoleName, configVariable);

    C_CMD(consoleName, "",   MappedConfigVariable);
    C_CMD(consoleName, opts, MappedConfigVariable);
}
