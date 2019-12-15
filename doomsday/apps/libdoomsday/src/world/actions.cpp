/** @file world/actions.cpp
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/world/actions.h"
#include "doomsday/gameapi.h"
#include "doomsday/doomsdayapp.h"
#include "doomsday/players.h"
#include "doomsday/world/mobjthinkerdata.h"
#include "doomsday/defs/ded.h"

#include <de/DictionaryValue>
#include <de/NativePointerValue>
#include <de/Process>
#include <de/String>
#include <de/TextValue>
#include <QMap>

using namespace de;

static QMap<String, acfnptr_t> s_actions; ///< name => native function pointer.
static String s_currentAction;

static void C_DECL A_DoomsdayScript(void *actor)
{
    struct mobj_s *mobj = reinterpret_cast<struct mobj_s *>(actor);
    int plrNum = -1;

    auto &plrs = DoomsdayApp::players();

    // The actor can also be a player in the case of psprites.
    // Look up the corresponding player.
    {
        for (int i = 0; i < DDMAXPLAYERS; ++i)
        {
            // Note: It is assumed that the player data structure begins with a pointer to
            // the ddplayer_t.

            if (&plrs.at(i).publicData() ==
                *reinterpret_cast<const ddplayer_t * const *>(actor))
            {
                // Refer to the player mobj instead.
                mobj = plrs.at(i).publicData().mo;
                plrNum = i;
            }
        }
    }

    LOG_AS("A_DoomsdayScript");
    try
    {
        const ThinkerData &data = THINKER_DATA(mobj->thinker, ThinkerData);
        Record ns;
        if (plrNum >= 0)
        {
            ns.add(new Variable(QStringLiteral("player"),
                                new RecordValue(plrs.at(plrNum).objectNamespace())));
        }
        ns.add(new Variable(QStringLiteral("self"), new RecordValue(data.objectNamespace())));
        Process proc(&ns);
        const Script script(s_currentAction);
        proc.run(script);
        proc.execute();
    }
    catch (const Error &er)
    {
        LOG_SCR_ERROR(er.asText());
    }
}

static bool isScriptAction(const String &name)
{
    return !name.beginsWith("A_");
}

void P_GetGameActions()
{
    s_actions.clear();

    // Action links are provided by the game (which owns the actual action functions).
    if (auto getVar = DoomsdayApp::plugins().gameExports().GetPointer)
    {
        auto const *links = (actionlink_t const *) getVar(DD_ACTION_LINK);
        for (actionlink_t const *link = links; link && link->name; link++)
        {
            s_actions.insert(String(link->name).toLower(), link->func);
        }
    }
}

void P_SetCurrentAction(const String &name)
{
    s_currentAction = name;
}

void P_SetCurrentActionState(int state)
{
    P_SetCurrentAction(DED_Definitions()->states[state].gets(QStringLiteral("action")));
}

acfnptr_t P_GetAction(const String &name)
{
    if (!name.isEmpty())
    {
        if (isScriptAction(name))
        {
            return de::function_cast<acfnptr_t>(A_DoomsdayScript);
        }
        auto found = s_actions.find(name.toLower());
        if (found != s_actions.end()) return found.value();
    }
    return nullptr;  // Not found.
}

acfnptr_t P_GetAction(const char *name)
{
    return P_GetAction(String(name));
}
