/** @file common.c  Top-level libcommon routines.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2014 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "g_common.h"
#include "g_update.h"
#include "p_map.h"
#include "polyobjs.h"
#include "r_common.h"

int Common_GetInteger(int id)
{
    switch(id)
    {
    case DD_MOBJ_SIZE:
        return sizeof(mobj_t); // game plugin specific

    case DD_POLYOBJ_SIZE:
        return sizeof(Polyobj);

    case DD_GAME_RECOMMENDS_SAVING:
        // The engine will use this as a hint whether to remind the user to
        // manually save the game before, e.g., upgrading to a new version.
        return G_GameState() == GS_MAP;

    default:
        break;
    }
    return 0;
}

void *Common_GetGameAPI(char const *name)
{
    #define HASH_ENTRY(Name, Func) std::make_pair(QByteArray(Name), de::function_cast<void *>(Func))
    static QHash<QByteArray, void *> const funcs(
    {
        HASH_ENTRY("DrawViewPort",          G_DrawViewPort),
        HASH_ENTRY("FinaleResponder",       FI_PrivilegedResponder),
        HASH_ENTRY("FinalizeMapChange",     P_FinalizeMapChange),
        HASH_ENTRY("HandleMapDataPropertyValue", P_HandleMapDataPropertyValue),
        HASH_ENTRY("HandleMapObjectStatusReport", P_HandleMapObjectStatusReport),
        HASH_ENTRY("HandlePacket",          D_HandlePacket),
        HASH_ENTRY("MobjCheckPositionXYZ",  P_CheckPositionXYZ),
        HASH_ENTRY("MobjFriction",          Mobj_Friction),
        HASH_ENTRY("MobjRestoreState",      Mobj_RestoreObjectState),
        HASH_ENTRY("MobjStateAsInfo",       Mobj_StateAsInfo),
        HASH_ENTRY("MobjThinker",           P_MobjThinker),
        HASH_ENTRY("MobjTryMoveXYZ",        P_TryMoveXYZ),
        HASH_ENTRY("NetConnect",            D_NetConnect),
        HASH_ENTRY("NetDisconnect",         D_NetDisconnect),
        HASH_ENTRY("NetPlayerEvent",        D_NetPlayerEvent),
        HASH_ENTRY("NetServerStart",        D_NetServerStarted),
        HASH_ENTRY("NetServerStop",         D_NetServerClose),
        HASH_ENTRY("NetWorldEvent",         D_NetWorldEvent),
        HASH_ENTRY("PrivilegedResponder",   G_PrivilegedResponder),
        HASH_ENTRY("Responder",             G_Responder),
        HASH_ENTRY("SectorHeightChangeNotification", P_HandleSectorHeightChange),
        HASH_ENTRY("Ticker",                G_Ticker),
        HASH_ENTRY("UpdateState",           G_UpdateState),
    });
    #undef HASH_ENTRY

    auto found = funcs.find(name);
    if (found != funcs.end()) return found.value();
    return nullptr;
}

GameRules &gfw_DefaultGameRules()
{
    static GameRules defaultGameRules;
    return defaultGameRules;
}

void GameRules_UpdateDefaultsFromCVars()
{
#if !__JHEXEN__
    gfw_SetDefaultRule(fast, cfg.common.defaultRuleFastMonsters);
#endif
}

#ifdef __JDOOM__
void fastMonstersChanged()
{
    GameRules_UpdateDefaultsFromCVars();
}
#endif

void Common_Register()
{
    // Movement
    C_VAR_FLOAT("player-move-speed",    &cfg.common.playerMoveSpeed,    0, 0, 1);
    C_VAR_INT  ("player-jump",          &cfg.common.jumpEnabled,        0, 0, 1);
    C_VAR_FLOAT("player-jump-power",    &cfg.common.jumpPower,          0, 0, 100);
    C_VAR_BYTE ("player-air-movement",  &cfg.common.airborneMovement,   0, 0, 32);

    // Gameplay
    C_VAR_BYTE ("sound-switch-origin",  &cfg.common.switchSoundOrigin,  0, 0, 1);
#ifdef __JDOOM__
    C_VAR_BYTE2("game-monsters-fast",   &cfg.common.defaultRuleFastMonsters, 0, 0, 1, fastMonstersChanged);
#endif
    C_VAR_BYTE ("game-objects-pushable-limit", &cfg.common.pushableMomentumLimitedToPusher, 0, 0, 1);
}
