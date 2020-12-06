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
#include "g_defs.h"
#include "g_update.h"
#include "gamesession.h"
#include "p_map.h"
#include "p_start.h"
#include "polyobjs.h"
#include "r_common.h"

#if defined (__JHERETIC__) || defined (__JHEXEN__)
#  define HAVE_SEEKER_MISSILE 1
#  include "p_local.h"
#endif

#include <de/Binder>
#include <de/Context>
#include <de/NoneValue>
#include <de/ScriptSystem>
#include <doomsday/DoomsdayApp>
#include <doomsday/defs/thing.h>
#include <doomsday/players.h>
#include <doomsday/world/map.h>
#include <doomsday/world/entitydef.h>
#include <doomsday/world/thinkerdata.h>

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
    C_VAR_BYTE ("hud-title-author-nounknown",   &cfg.common.hideIWADAuthor,     0, 0, 1);

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

//-------------------------------------------------------------------------------------------------

static de::Binder *gameBindings;
static de::Record *gameModule;

de::Binder &Common_GameBindings()
{
    DE_ASSERT(gameBindings);
    return *gameBindings;
}

mobj_t &P_ContextMobj(const de::Context &ctx)
{
    using namespace de;

    const int id = ctx.selfInstance().geti(QStringLiteral("__id__"), 0);
    if (mobj_t *mo = Mobj_ById(id))
    {
        return *mo;
    }
    throw world::BaseMap::MissingObjectError("P_ContextMobj",
                                             String::format("Mobj %d does not exist", id));
}

static de::Value *Function_Thing_SpawnMissile(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    using namespace de;

    mobj_t *src = &P_ContextMobj(ctx);

    const mobjtype_t missileId = mobjtype_t(Defs().getMobjNum(args.at(0)->asText()));

    if (is<NoneValue>(args.at(1))) // Fire at target mobj.
    {
        if (src->target)
        {
#if defined(__JHERETIC__)
            if (mobj_t *mis = P_SpawnMissile(missileId, src, src->target, true))
            {
                if (missileId == MT_MUMMYFX1)
                {
                    // Tracer is used to keep track of where the missile is homing.
                    mis->tracer = src->target;
                }
                else if (missileId == MT_WHIRLWIND)
                {
                    P_InitWhirlwind(mis, src->target);
                }
            }
#else
            P_SpawnMissile(missileId, src, src->target);
#endif
        }
    }
    else
    {
#if defined(__JHERETIC__) || defined(__JHEXEN__)
        const double angle = args.at(1)->asNumber() / 360.0;
        const double momZ  = args.at(2)->asNumber();
        P_SpawnMissileAngle(missileId, src, angle_t(angle * ANGLE_MAX), momZ);
#endif
    }
    return nullptr;
}

#if defined (HAVE_SEEKER_MISSILE)
static de::Value *Function_Thing_SeekerMissile(de::Context &ctx,
                                               const de::Function::ArgumentValues &args)
{
    const angle_t thresh  = args.at(0)->asNumber() * ANGLE_1;
    const angle_t turnMax = args.at(1)->asNumber() * ANGLE_1;
    P_SeekerMissile(&P_ContextMobj(ctx), thresh, turnMax);
    return nullptr;
}
#endif

static int playerNumberArgument(const de::Value &arg)
{
    if (de::is<de::NoneValue>(arg))
    {
        return CONSOLEPLAYER;
    }
    const int plrNum = arg.asInt();
    if (plrNum < 0 || plrNum >= MAXPLAYERS)
    {
        throw de::Error("playerNumberArgument", "Player index out of bounds");
    }
    return plrNum;
}

static de::Value *Function_Game_SetMessage(de::Context &, const de::Function::ArgumentValues &args)
{
    const int plrNum = playerNumberArgument(*args.at(1));
    P_SetMessage(&players[plrNum], args.at(0)->asText().toLatin1());
    return nullptr;
}

static de::Value *Function_Game_Rules(de::Context &, const de::Function::ArgumentValues &args)
{
    return new de::RecordValue(gfw_Session()->rules().asRecord());
}

#if defined (__JHEXEN__)
static de::Value *Function_SetYellowMessage(de::Context &, const de::Function::ArgumentValues &args)
{
    const int plrNum = playerNumberArgument(*args.at(1));
    P_SetYellowMessage(&players[plrNum], args.at(0)->asText().toLatin1());
    return nullptr;
}
#endif

static de::Value *Function_World_SpawnThing(de::Context &, const de::Function::ArgumentValues &args)
{
    using namespace de;

    mobjtype_t mobjType   = mobjtype_t(Defs().getMobjNum(args.at(0)->asText()));
    int        spawnFlags = args.at(3)->asInt();
    Vector3d   pos;

    if (args.at(1)->size() == 2)
    {
        pos = vectorFromValue<Vector2d>(*args.at(1));
        spawnFlags |= MSF_Z_FLOOR;
    }
    else
    {
        pos = vectorFromValue<Vector3d>(*args.at(1));
    }

    angle_t angle =
        angle_t((is<NoneValue>(args.at(2)) ? 360.0f * randf() : args.at(2)->asNumber()) / 180.0f *
                ANGLE_180);

    if (mobjType < 0)
    {
        throw Error("Function_World_SpawnThing", "Invalid thing type: " + args.at(0)->asText());
    }

    if (mobj_t *mobj = P_SpawnMobjXYZ(mobjType, pos.x, pos.y, pos.z, angle, spawnFlags))
    {
        return new RecordValue(THINKER_NS(mobj->thinker));
    }
    return new NoneValue;
}

player_t &P_ContextPlayer(const de::Context &ctx)
{
    const int num = ctx.selfInstance().geti("__id__", 0);
    if (num < 0 || num >= MAXPLAYERS)
    {
        throw de::Error("P_ContextPlayer", "invalid player number");
    }
    return players[num];
}

static de::Value *Function_Player_Health(de::Context &ctx, const de::Function::ArgumentValues &)
{
    return new de::NumberValue(P_ContextPlayer(ctx).health);
}

static de::Value *Function_Player_SetHealth(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    auto &player = P_ContextPlayer(ctx);
    const int hp = de::max(0, args.at(0)->asInt());
    if (hp <= 0)
    {
        // Kill the player.
        P_DamageMobj(player.plr->mo, NULL, NULL, 10000, false);
    }
    else
    {
        player.health = hp;
        if (player.plr->mo)
        {
            player.plr->mo->health = hp;
        }
        player.update |= PSF_HEALTH;
    }
    return nullptr;
}

#if !defined (__JHEXEN__)
#  define HAVE_DOOM_ARMOR_BINDINGS 1

static de::Value *Function_Player_Armor(de::Context &ctx, const de::Function::ArgumentValues &)
{
    return new de::NumberValue(P_ContextPlayer(ctx).armorPoints);
}

static de::Value *Function_Player_ArmorType(de::Context &ctx, const de::Function::ArgumentValues &)
{
    return new de::NumberValue(P_ContextPlayer(ctx).armorType);
}

static de::Value *Function_Player_GiveBackpack(de::Context &ctx, const de::Function::ArgumentValues &)
{
    P_GiveBackpack(&P_ContextPlayer(ctx));
    return nullptr;
}
#endif

static de::Value *Function_Player_GiveArmor(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    player_t *plr = &P_ContextPlayer(ctx);
    const int type = args.at(0)->asInt();
    const int points = args.at(1)->asInt();

#if defined(__JHEXEN__)
    const bool ok = P_GiveArmorAlt(plr, armortype_t(type), points) != 0;
#else
    const bool ok = P_GiveArmor(plr, type, points) != 0;
#endif

    return new de::NumberValue(ok);
}

static de::Value *Function_Player_Power(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    int power = args.at(0)->asInt();
    if (power < PT_FIRST || power >= NUM_POWER_TYPES)
    {
        throw de::Error("Function_Player_Power", "invalid power type");
    }
    return new de::NumberValue(P_ContextPlayer(ctx).powers[power]);
}

static de::Value *Function_Player_GivePower(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    P_GivePower(&P_ContextPlayer(ctx), powertype_t(args.at(0)->asInt()));
    return nullptr;
}

static de::Value *Function_Player_GiveAmmo(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    const int type = args.at(0)->asInt();
    if (type <= AT_FIRST || type > NUM_AMMO_TYPES)
    {
        throw de::Error("Function_Player_GiveAmmo", "Invalid ammo type");
    }
    P_GiveAmmo(&P_ContextPlayer(ctx), ammotype_t(type), args.at(1)->asInt());
    return nullptr;
}

static de::Value *Function_Player_ShotAmmo(de::Context &ctx, const de::Function::ArgumentValues &)
{
    P_ShotAmmo(&P_ContextPlayer(ctx));
    return nullptr;
}

#if defined (HAVE_EARTHQUAKE)
// Script function to control earthquakes.
static de::Value *Function_Player_SetLocalQuake(de::Context &ctx, const de::Function::ArgumentValues &args)
{
    const auto plr = &P_ContextPlayer(ctx) - players;
    localQuakeHappening[plr] = args.at(0)->asInt();
    localQuakeTimeout[plr] = args.at(1)->asInt();
    players[plr].update |= PSF_LOCAL_QUAKE;
    return nullptr;
}
#endif

//-------------------------------------------------------------------------------------------------

void Common_Load()
{
    using namespace de;

    // Script bindings.
    {
        auto &scr = ScriptSystem::get();

        DENG2_ASSERT(gameBindings == nullptr);
        gameBindings = new Binder(nullptr, Binder::FunctionsOwned); // must delete when plugin unloaded

        // Game module.
        {
            gameModule = new Record;
            scr.addNativeModule("Game", *gameModule);

            Function::Defaults setMessageArgs;
            setMessageArgs["player"] = new NoneValue;

            gameBindings->init(*gameModule)
                << DENG2_FUNC_DEFS (Game_SetMessage, "setMessage", "message" << "player", setMessageArgs)
                << DENG2_FUNC_NOARG(Game_Rules, "rules");

#if defined(__JHEXEN__)
            {
                Function::Defaults setYellowMessageArgs;
                setYellowMessageArgs["player"] = new NoneValue;
                *gameBindings
                    << DENG2_FUNC_DEFS(SetYellowMessage, "setYellowMessage", "message" << "player", setYellowMessageArgs);
            }
#endif
        }

        // World module.
        {
            Function::Defaults spawnThingArgs;
            spawnThingArgs["angle"] = new NoneValue;
            spawnThingArgs["flags"] = new NumberValue(0.0);

            gameBindings->init(scr["World"])
                << DENG2_FUNC_DEFS(World_SpawnThing,
                                   "spawnThing",
                                   "type" << "pos" << "angle" << "flags",
                                   spawnThingArgs);

            Function::Defaults spawnMissileArgs;
            spawnMissileArgs["angle"] = new NoneValue;
            spawnMissileArgs["momz"] = new NumberValue(0.0);

            gameBindings->init(scr.builtInClass("World", "Thing"))
                << DENG2_FUNC_DEFS(Thing_SpawnMissile,
                                   "spawnMissile",
                                   "id" << "angle" << "momz",
                                   spawnMissileArgs);

#if defined (HAVE_SEEKER_MISSILE)
            *gameBindings << DE_FUNC(Thing_SeekerMissile, "seekerMissile", "thresh" << "turnMax");
#endif

            auto &world = scr["World"];
            world.set("MSF_Z_FLOOR", MSF_Z_FLOOR);
            world.set("MSF_Z_CEIL", MSF_Z_CEIL);
#if defined (MSF_DEAF)
            world.set("MSF_AMBUSH", MSF_DEAF);
#endif
#if defined (MSF_AMBUSH)
            world.set("MSF_AMBUSH", MSF_AMBUSH);
#endif
        }

        // App.Player
        {
            auto &playerClass = scr.builtInClass("App", "Player");
            gameBindings->init(playerClass)
                << DENG2_FUNC_NOARG (Player_Health, "health")
                << DENG2_FUNC       (Player_Power, "power", "type")
                << DENG2_FUNC_NOARG (Player_ShotAmmo, "shotAmmo")
                << DE_FUNC          (Player_GiveAmmo, "giveAmmo", "type" << "amount")
                << DENG2_FUNC       (Player_GiveArmor, "giveArmor", "type" << "points")
                << DE_FUNC          (Player_GivePower, "givePower", "type")
                << DE_FUNC          (Player_SetHealth, "setHealth", "hp");
#if !defined(__JHEXEN__)
            *gameBindings
                << DE_FUNC_NOARG    (Player_GiveBackpack, "giveBackpack");
#endif

#if defined(HAVE_DOOM_ARMOR_BINDINGS)
            *gameBindings
                << DENG2_FUNC_NOARG (Player_Armor, "armor")
                << DENG2_FUNC_NOARG (Player_ArmorType, "armorType");
#endif

#if defined(HAVE_EARTHQUAKE)
            Function::Defaults setLocalQuakeArgs;
            setLocalQuakeArgs["duration"] = new NumberValue(0);
            *gameBindings
                << DENG2_FUNC_DEFS (Player_SetLocalQuake, "setLocalQuake", "intensity" << "duration", setLocalQuakeArgs);
#endif
        }
    }
}

void Common_Unload()
{
    using namespace de;

    auto &scr = ScriptSystem::get();

    // After game is unloaded, binder will delete its functions but
    // other symbols need to be manually cleaned up.
    {
        scr["World"].removeMembersWithPrefix("MSF_");
    }

    DENG2_ASSERT(gameBindings != nullptr);
    scr.removeNativeModule("Game");
    delete gameBindings;
    gameBindings = nullptr;
    delete gameModule;
    gameModule = nullptr;
}

void Common_RegisterMapObjs()
{
    P_RegisterMapObj(MO_THING, "Thing");
    P_RegisterMapObjProperty(MO_THING, MO_X, "X", DDVT_DOUBLE);
    P_RegisterMapObjProperty(MO_THING, MO_Y, "Y", DDVT_DOUBLE);
    P_RegisterMapObjProperty(MO_THING, MO_Z, "Z", DDVT_DOUBLE);
    P_RegisterMapObjProperty(MO_THING, MO_ANGLE, "Angle", DDVT_ANGLE);
    P_RegisterMapObjProperty(MO_THING, MO_DOOMEDNUM, "DoomEdNum", DDVT_INT);
    P_RegisterMapObjProperty(MO_THING, MO_SKILLMODES, "SkillModes", DDVT_INT);
    P_RegisterMapObjProperty(MO_THING, MO_FLAGS, "Flags", DDVT_INT);

    P_RegisterMapObj(MO_XLINEDEF, "XLinedef");
    P_RegisterMapObjProperty(MO_XLINEDEF, MO_TAG, "Tag", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_XLINEDEF, MO_TYPE, "Type", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_XLINEDEF, MO_FLAGS, "Flags", DDVT_SHORT);

    P_RegisterMapObj(MO_XSECTOR, "XSector");
    P_RegisterMapObjProperty(MO_XSECTOR, MO_TAG, "Tag", DDVT_SHORT);
    P_RegisterMapObjProperty(MO_XSECTOR, MO_TYPE, "Type", DDVT_SHORT);
}
