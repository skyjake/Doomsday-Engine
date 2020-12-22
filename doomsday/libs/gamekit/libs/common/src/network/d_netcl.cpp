/** @file d_netcl.cpp  Common code related to netgames (client-side).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
#include "d_netcl.h"

#include <cstdio>
#include <cstring>
#include "d_netsv.h"       ///< @todo remove me
#include "g_common.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hu_inventory.h"
#include "p_actor.h"
#include "p_inventory.h"
#include "p_map.h"
#include "p_saveg.h"
#include "p_start.h"
#include "r_common.h"
#include "player.h"
#include "st_stuff.h"

using namespace de;
using namespace common;

void NetCl_UpdateGameState(reader_s *msg)
{
    BusyMode_FreezeGameForBusyMode();

    byte gsFlags = Reader_ReadByte(msg);

    // Game identity key.
    AutoStr *gsGameId = AutoStr_NewStd();
    Str_Read(gsGameId, msg);

    // Current map.
    uri_s *gsMapUri = Uri_FromReader(msg);
    Uri_SetScheme(gsMapUri, "Maps");

    // Current episode.
    AutoStr *gsEpisodeId = AutoStr_NewStd();
    Str_Read(gsEpisodeId, msg);

    /*uint gsMap     =*/ Reader_ReadByte(msg);

    /// @todo Not communicated to clients??
    //uint gsMapEntrance = ??;

    byte configFlags = Reader_ReadByte(msg);

    GameRules gsRules(gfw_Session()->rules()); // Initialize with a copy of the current rules.
    GameRules_Set(gsRules, deathmatch, configFlags & 0x3);
    GameRules_Set(gsRules, noMonsters, !(configFlags & 0x4? true : false));
#if !__JHEXEN__
    GameRules_Set(gsRules, respawnMonsters, (configFlags & 0x8? true : false));
#endif
    /// @todo Not applied??
    //byte gsJumping          = (configFlags & 0x10? true : false);

    GameRules_Set(gsRules, skill, skillmode_t(Reader_ReadByte(msg)));

    // Interpret skill modes outside the normal range as "spawn no things".
    if(gsRules.values.skill < SM_BABY || gsRules.values.skill >= NUM_SKILL_MODES)
    {
        GameRules_Set(gsRules, skill, SM_NOTHINGS);
    }

    coord_t gsGravity = Reader_ReadFloat(msg);

    LOGDEV_MAP_NOTE("NetCl_UpdateGameState: Flags=%x") << gsFlags;

    // Demo game state changes are only effective during demo playback.
    if(gsFlags & GSF_DEMO && !Get(DD_PLAYBACK))
    {
        Uri_Delete(gsMapUri);
        return;
    }

    // Check for a game mode mismatch.
    /// @todo  Automatically load the server's game if it is available.
    /// However, note that this can only occur if the server changes its game
    /// while a netgame is running (which currently will end the netgame).
    if (gfw_GameId().compare(Str_Text(gsGameId)))
    {
        LOG_NET_ERROR("Game mismatch: server's identity key (%s) is different to yours (%s)")
                << gsGameId << gfw_GameId();
        DD_Execute(false, "net disconnect");
        Uri_Delete(gsMapUri);
        return;
    }

    // Some statistics.
    LOG_NOTE("%s - %s\n  %s")
            << gsRules.description()
            << Str_Text(Uri_ToString(gsMapUri))
            << gsRules.asText();

    // Do we need to change the map?
    if(gsFlags & GSF_CHANGE_MAP)
    {
        gfw_Session()->end();
        gfw_Session()->begin(gsRules, Str_Text(gsEpisodeId),
                                  *reinterpret_cast<res::Uri *>(gsMapUri),
                                  gfw_Session()->mapEntryPoint() /*gsMapEntrance*/);
    }
    else
    {
        /// @todo Breaks session management logic; rules cannot change once the session has
        /// begun and setting the current map and/or entrance is illogical at this point.
        DE_ASSERT(!Str_Compare(gsEpisodeId, gfw_Session()->episodeId()));
        DE_ASSERT(*reinterpret_cast<res::Uri *>(gsMapUri) == gfw_Session()->mapUri());

        gfw_Session()->applyNewRules(gsRules);
    }

    // Set gravity.
    /// @todo This is a map-property, not a global property.
    DD_SetVariable(DD_MAP_GRAVITY, &gsGravity);

    // Camera init included?
    if(gsFlags & GSF_CAMERA_INIT)
    {
        player_t *pl = &players[CONSOLEPLAYER];
        if(mobj_t *mo = pl->plr->mo)
        {
            P_MobjUnlink(mo);
            mo->origin[VX] = Reader_ReadFloat(msg);
            mo->origin[VY] = Reader_ReadFloat(msg);
            mo->origin[VZ] = Reader_ReadFloat(msg);
            P_MobjLink(mo);
            mo->angle      = Reader_ReadUInt32(msg);
            // Update floorz and ceilingz.
#if __JDOOM__ || __JDOOM64__
            P_CheckPosition(mo, mo->origin);
#else
            P_CheckPositionXY(mo, mo->origin[VX], mo->origin[VY]);
#endif
            mo->floorZ     = tmFloorZ;
            mo->ceilingZ   = tmCeilingZ;
        }
        else
        {
            float mx       = Reader_ReadFloat(msg);
            float my       = Reader_ReadFloat(msg);
            float mz       = Reader_ReadFloat(msg);
            angle_t angle  = Reader_ReadUInt32(msg);

            LOGDEV_NET_WARNING("NetCl_UpdateGameState: Got camera init, but player has no mobj; "
                               "pos=%f,%f,%f Angle=%x") << mx << my << mz << angle;
        }
    }

    // Tell the server we're ready to begin receiving frames.
    Net_SendPacket(0, DDPT_OK, 0, 0);

    Uri_Delete(gsMapUri);
}

void NetCl_MobjImpulse(reader_s *msg)
{
    mobj_t *mo   = players[CONSOLEPLAYER].plr->mo;
    mobj_t *clmo = ClPlayer_ClMobj(CONSOLEPLAYER);

    if(!mo || !clmo) return;

    thid_t id = Reader_ReadUInt16(msg);
    if(id != clmo->thinker.id)
    {
        // Not applicable; wrong mobj.
        return;
    }

    App_Log(DE2_DEV_MAP_VERBOSE, "NetCl_MobjImpulse: Player %i, clmobj %i", CONSOLEPLAYER, id);

    // Apply to the local mobj.
    mo->mom[MX] += Reader_ReadFloat(msg);
    mo->mom[MY] += Reader_ReadFloat(msg);
    mo->mom[MZ] += Reader_ReadFloat(msg);
}

void NetCl_PlayerSpawnPosition(reader_s *msg)
{
    player_t *p = &players[CONSOLEPLAYER];

    coord_t x     = Reader_ReadFloat(msg);
    coord_t y     = Reader_ReadFloat(msg);
    coord_t z     = Reader_ReadFloat(msg);
    angle_t angle = Reader_ReadUInt32(msg);

    App_Log(DE2_DEV_MAP_NOTE, "Got player spawn position (%g, %g, %g) facing %x",
            x, y, z, angle);

    mobj_t *mo = p->plr->mo;
    DE_ASSERT(mo != 0);

    P_TryMoveXYZ(mo, x, y, z);
    mo->angle = angle;
}

void NetCl_UpdatePlayerState2(reader_s *msg, int plrNum)
{
    player_t *pl = &players[plrNum];

    if(!Get(DD_GAME_READY))
    {
        App_Log(DE2_DEV_NET_WARNING, "NetCl_UpdatePlayerState2: game isn't ready yet!");
        return;
    }

    if(plrNum < 0)
    {
        // Player number included in the message.
        plrNum = Reader_ReadByte(msg);
    }
    uint flags = Reader_ReadUInt32(msg);

    if(flags & PSF2_OWNED_WEAPONS)
    {
        int k = Reader_ReadUInt16(msg);
        for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
        {
            bool owned = CPP_BOOL(k & (1 << i));

            // Maybe unhide the HUD?
            if(owned && pl->weapons[i].owned == false)
            {
                ST_HUDUnHide(pl - players, HUE_ON_PICKUP_WEAPON);
            }

            pl->weapons[i].owned = owned;
        }
    }

    if(flags & PSF2_STATE)
    {
        int oldPlayerState = pl->playerState;

        byte b = Reader_ReadByte(msg);
        pl->playerState = playerstate_t(b & 0xf);
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        pl->armorType = b >> 4;
#endif

        App_Log(DE2_DEV_MAP_MSG, "NetCl_UpdatePlayerState2: New player state = %s",
                pl->playerState == PST_LIVE? "PST_LIVE" :
                pl->playerState == PST_DEAD? "PST_DEAD" : "PST_REBORN");

        // Player state changed?
        if(oldPlayerState != pl->playerState)
        {
            // Set or clear the DEAD flag for this player.
            if(pl->playerState == PST_LIVE)
            {
                // Becoming alive again...
                // After being reborn, the server will tell us the new weapon.
                pl->plr->flags |= DDPF_UNDEFINED_WEAPON;

                App_Log(DE2_DEV_MAP_MSG, "NetCl_UpdatePlayerState2: Player %i: Marking weapon as undefined",
                        (int)(pl - players));

                pl->plr->flags &= ~DDPF_DEAD;
            }
            else
            {
                pl->plr->flags |= DDPF_DEAD;
            }
        }

        pl->cheats = Reader_ReadByte(msg);

        // Set or clear the NOCLIP flag.
        if(P_GetPlayerCheats(pl) & CF_NOCLIP)
            pl->plr->flags |= DDPF_NOCLIP;
        else
            pl->plr->flags &= ~DDPF_NOCLIP;
    }
}

void NetCl_UpdatePlayerState(reader_s *msg, int plrNum)
{
    int i;
    byte b;
    int s;

    if(!Get(DD_GAME_READY))
        return;

    if(plrNum < 0)
    {
        plrNum = Reader_ReadByte(msg);
    }
    player_t *pl = &players[plrNum];

    int flags = Reader_ReadUInt16(msg);

    if(flags & PSF_STATE)       // and armor type (the same bit)
    {
        byte b = Reader_ReadByte(msg);
        pl->playerState = playerstate_t(b & 0xf);
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        pl->armorType = b >> 4;
#endif
        // Set or clear the DEAD flag for this player.
        if(pl->playerState == PST_LIVE)
            pl->plr->flags &= ~DDPF_DEAD;
        else
            pl->plr->flags |= DDPF_DEAD;

        //if(oldstate != pl->playerState) // && oldstate == PST_DEAD)
        {
            P_SetupPsprites(pl);
        }
    }

    if(flags & PSF_HEALTH)
    {
        int health = Reader_ReadByte(msg);

        if(health < pl->health)
            ST_HUDUnHide(plrNum, HUE_ON_DAMAGE);

        pl->health = health;
        if(pl->plr->mo)
        {
            pl->plr->mo->health = pl->health;
        }
        else
        {
            App_Log(DE2_DEV_MAP_ERROR, "NetCl_UpdatePlayerState: Player mobj not yet allocated at this time");
        }
    }

    if(flags & PSF_ARMOR_POINTS)
    {
        byte    ap;
#if __JHEXEN__
        for(i = 0; i < NUMARMOR; ++i)
        {
            ap = Reader_ReadByte(msg);

            // Maybe unhide the HUD?
            if(ap >= pl->armorPoints[i] &&
                pl == &players[CONSOLEPLAYER])
                ST_HUDUnHide(plrNum, HUE_ON_PICKUP_ARMOR);

            pl->armorPoints[i] = ap;
        }
#else
        ap = Reader_ReadByte(msg);

        // Maybe unhide the HUD?
        if(ap >= pl->armorPoints)
            ST_HUDUnHide(plrNum, HUE_ON_PICKUP_ARMOR);

        pl->armorPoints = ap;
#endif

    }

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    if(flags & PSF_INVENTORY)
    {
        for(uint i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
        {
            inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRST + i);
            uint count = P_InventoryCount(plrNum, type);

            for(uint j = 0; j < count; ++j)
            {
                P_InventoryTake(plrNum, type, true);
            }
        }

        uint count = Reader_ReadByte(msg);
        for(uint i = 0; i < count; ++i)
        {
            s = Reader_ReadUInt16(msg);

            const inventoryitemtype_t type = inventoryitemtype_t(s & 0xff);
            const uint num                 = s >> 8;

            for(uint j = 0; j < num; ++j)
            {
                P_InventoryGive(plrNum, type, true);
            }
        }
    }
#endif

    if(flags & PSF_POWERS)
    {
        b = Reader_ReadByte(msg);

        // Only the non-zero powers are included in the message.
#if __JHEXEN__ || __JSTRIFE__
        for(i = 0; i < NUM_POWER_TYPES - 1; ++i)
        {
            byte val = ((b & (1 << i))? (Reader_ReadByte(msg) * 35) : 0);

            // Maybe unhide the HUD?
            if(val > pl->powers[i])
                ST_HUDUnHide(pl - players, HUE_ON_PICKUP_POWER);

            pl->powers[i + 1] = val;
        }
#else
        for(i = 0; i < NUM_POWER_TYPES; ++i)
        {
#  if __JDOOM__ || __JDOOM64__
            if(i == PT_IRONFEET || i == PT_STRENGTH)
                continue;
#  endif
            {
                int val = ((b & (1 << i))? (Reader_ReadByte(msg) * 35) : 0);

                /**
                 * @todo This function duplicates logic in P_GivePower(). The
                 * redundancy should be removed for instance by adding a new
                 * game packet GPT_GIVE_POWER that calls the appropriate
                 * P_GivePower() on clientside after it has been called on the
                 * server. -jk
                 */

                // Maybe unhide the HUD?
                if(val > pl->powers[i])
                    ST_HUDUnHide(plrNum, HUE_ON_PICKUP_POWER);

                pl->powers[i] = val;

                if(val && i == PT_FLIGHT && pl->plr->mo)
                {
                    pl->plr->mo->flags2 |= MF2_FLY;
                    pl->plr->mo->flags |= MF_NOGRAVITY;
                    pl->flyHeight = 10;
                    pl->powers[i] = val;

                    App_Log(DE2_DEV_MAP_MSG, "NetCl_UpdatePlayerState: Local mobj flight enabled");
                }

                // Should we reveal the map?
                if(val && i == PT_ALLMAP && plrNum == CONSOLEPLAYER)
                {
                    App_Log(DE2_DEV_MAP_MSG, "NetCl_UpdatePlayerState: Revealing automap");

                    ST_RevealAutomap(plrNum, true);
                }
            }
        }
#endif
    }

    if(flags & PSF_KEYS)
    {
        b = Reader_ReadByte(msg);
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        for(i = 0; i < NUM_KEY_TYPES; ++i)
        {
            dd_bool val = (b & (1 << i)) != 0;

            // Maybe unhide the HUD?
            if(val && !pl->keys[i])
                ST_HUDUnHide(plrNum, HUE_ON_PICKUP_KEY);

            pl->keys[i] = val;
        }
#endif
#if __JHEXEN__
        if((pl->keys & b) != 0)
        {
            ST_HUDUnHide(plrNum, HUE_ON_PICKUP_KEY);
        }
        pl->keys = b;
#endif
    }

    if(flags & PSF_FRAGS)
    {
        de::zap(pl->frags);
        // First comes the number of frag counts included.
        for(i = Reader_ReadByte(msg); i > 0; i--)
        {
            s = Reader_ReadUInt16(msg);
            pl->frags[s >> 12] = s & 0xfff;
        }
    }

    if(flags & PSF_OWNED_WEAPONS)
    {
        b = Reader_ReadByte(msg);
        for(i = 0; i < NUM_WEAPON_TYPES; ++i)
        {
            bool owned = CPP_BOOL(b & (1 << i));

            // Maybe unhide the HUD?
            if(owned && pl->weapons[i].owned == false)
            {
                ST_HUDUnHide(plrNum, HUE_ON_PICKUP_WEAPON);
            }

            pl->weapons[i].owned = owned;
        }
    }

    if(flags & PSF_AMMO)
    {
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            int val = Reader_ReadInt16(msg);

            // Maybe unhide the HUD?
            if(val > pl->ammo[i].owned)
            {
                ST_HUDUnHide(plrNum, HUE_ON_PICKUP_AMMO);
            }

            pl->ammo[i].owned = val;
        }
    }

    if(flags & PSF_MAX_AMMO)
    {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ // Hexen has no use for max ammo.
        for(i = 0; i < NUM_AMMO_TYPES; i++)
            pl->ammo[i].max = Reader_ReadInt16(msg);
#endif
    }

    if(flags & PSF_COUNTERS)
    {
        pl->killCount   = Reader_ReadInt16(msg);
        pl->itemCount   = Reader_ReadByte(msg);
        pl->secretCount = Reader_ReadByte(msg);

        App_Log(DE2_DEV_MAP_MSG, "NetCl_UpdatePlayerState: kills=%i, items=%i, secrets=%i",
                pl->killCount, pl->itemCount, pl->secretCount);
    }

    if(flags & PSF_PENDING_WEAPON || flags & PSF_READY_WEAPON)
    {
        dd_bool wasUndefined = (pl->plr->flags & DDPF_UNDEFINED_WEAPON) != 0;

        b = Reader_ReadByte(msg);
        if(flags & PSF_PENDING_WEAPON)
        {
            if(!wasUndefined)
            {
                int weapon = b & 0xf;
                if(weapon != WT_NOCHANGE)
                {
                    App_Log(DE2_DEV_MAP_MSG, "NetCl_UpdatePlayerState: Weapon already known, "
                            "using an impulse to switch to %i", weapon);

                    P_Impulse(pl - players, CTL_WEAPON1 + weapon);
                }
            }
            else
            {
                pl->pendingWeapon = weapontype_t(b & 0xf);

                App_Log(DE2_DEV_MAP_MSG, "NetCl_UpdatePlayerState: pendingweapon=%i", pl->pendingWeapon);
            }

            pl->plr->flags &= ~DDPF_UNDEFINED_WEAPON;
        }

        if(flags & PSF_READY_WEAPON)
        {
            if(wasUndefined)
            {
                pl->readyWeapon = weapontype_t(b >> 4);
                App_Log(DE2_DEV_MAP_MSG, "NetCl_UpdatePlayerState: readyweapon=%i", pl->readyWeapon);
            }
            else
            {
                App_Log(DE2_DEV_MAP_NOTE, "NetCl_UpdatePlayerState: Readyweapon already known (%i), "
                        "not setting server's value %i", pl->readyWeapon, b >> 4);
            }

            pl->plr->flags &= ~DDPF_UNDEFINED_WEAPON;
        }

        if(!(pl->plr->flags & DDPF_UNDEFINED_WEAPON) && wasUndefined)
        {
            App_Log(DE2_DEV_MAP_NOTE, "NetCl_UpdatePlayerState: Weapon was undefined, bringing it up now");

            // Bring it up now.
            P_BringUpWeapon(pl);
        }
    }

    if(flags & PSF_VIEW_HEIGHT)
    {
        pl->viewHeight = (float) Reader_ReadByte(msg);
    }

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
    if(flags & PSF_MORPH_TIME)
    {
        pl->morphTics = Reader_ReadByte(msg) * 35;
        App_Log(DE2_DEV_MAP_MSG, "NetCl_UpdatePlayerState: Player %i morphtics = %i", plrNum, pl->morphTics);
    }
#endif

#if defined(HAVE_EARTHQUAKE) || __JSTRIFE__
    if(flags & PSF_LOCAL_QUAKE)
    {
        localQuakeHappening[plrNum] = Reader_ReadByte(msg);
    }
#endif
}

void NetCl_UpdatePSpriteState(reader_s * /*msg*/)
{
    // Not used.
    /*
    unsigned short s;

    NetCl_SetReadBuffer(data);
    s = NetCl_ReadShort();
    P_SetPsprite(&players[CONSOLEPLAYER], ps_weapon, s);
     */
}

void NetCl_Intermission(reader_s *msg)
{
    int flags = Reader_ReadByte(msg);

    if(flags & IMF_BEGIN)
    {
        // Close any HUDs left open at the end of the previous map.
        for(uint i = 0; i < MAXPLAYERS; ++i)
        {
            ST_CloseAll(i, true/*fast*/);
        }

        G_ResetViewEffects();

#if __JHEXEN__
        SN_StopAllSequences();
#endif

        /// @todo jHeretic does not transmit the intermission info!
#if !defined(__JHERETIC__)
#  if __JDOOM__ || __JDOOM64__
        ::wmInfo.maxKills   = de::max<int>(1, Reader_ReadUInt16(msg));
        ::wmInfo.maxItems   = de::max<int>(1, Reader_ReadUInt16(msg));
        ::wmInfo.maxSecret  = de::max<int>(1, Reader_ReadUInt16(msg));
#  endif
        Uri_Read(reinterpret_cast<uri_s *>(&::wmInfo.nextMap), msg);
#  if __JHEXEN__
        ::wmInfo.nextMapEntryPoint = Reader_ReadByte(msg);
#  else
        Uri_Read(reinterpret_cast<uri_s *>(&::wmInfo.currentMap), msg);
#  endif
#  if __JDOOM__ || __JDOOM64__
        ::wmInfo.didSecret  = Reader_ReadByte(msg);

        G_PrepareWIData();
#  endif
#endif

        IN_Begin(::wmInfo);

#if __JDOOM64__
        S_StartMusic("dm2int", true);
#elif __JDOOM__
        S_StartMusic((gameModeBits & GM_ANY_DOOM2)? "dm2int" : "inter", true);
#elif __JHERETIC__
        S_StartMusic("intr", true);
#elif __JHEXEN__
        S_StartMusic("hub", true);
#endif
        G_ChangeGameState(GS_INTERMISSION);
    }

    if(flags & IMF_END)
    {
        IN_End();
    }

    if(flags & IMF_STATE)
    {
#if __JDOOM__ || __JDOOM64__
        IN_SetState(interludestate_t(Reader_ReadInt16(msg)));
#elif __JHERETIC__ || __JHEXEN__
        IN_SetState(Reader_ReadInt16(msg));
#endif
    }

#if __JHERETIC__
    if(flags & IMF_TIME)
    {
        IN_SetTime(Reader_ReadUInt16(msg));
    }
#endif
}

#if 0 // MOVED INTO THE ENGINE
/**
 * This is where clients start their InFine interludes.
 */
void NetCl_Finale(int packetType, reader_s *msg)
{
    int         flags, len, numConds, i;
    byte       *script = NULL;

    flags = Reader_ReadByte(msg);
    if(flags & FINF_SCRIPT)
    {
        // First read the values of the conditions.
        if(packetType == GPT_FINALE2)
        {
            numConds = Reader_ReadByte(msg);
            for(i = 0; i < numConds; ++i)
            {
                FI_SetCondition(i, Reader_ReadByte(msg));
            }
        }

        // Read the script into map-scope memory. It will be freed
        // when the next map is loaded.
        len = Reader_ReadUInt32(msg);
        script = Z_Malloc(len + 1, PU_MAP, 0);
        Reader_Read(msg, script, len);
        script[len] = 0;
    }

    if(flags & FINF_BEGIN && script)
    {
        // Start the script.
        FI_Start((char*)script,
                 (flags & FINF_AFTER) ? FIMODE_AFTER : (flags & FINF_OVERLAY) ?
                 FIMODE_OVERLAY : FIMODE_BEFORE);
    }

    if(flags & FINF_END)
    {   // Stop InFine.
        FI_End();
    }

    if(flags & FINF_SKIP)
    {
        FI_SkipRequest();
    }
}
#endif

void NetCl_UpdatePlayerInfo(reader_s *msg)
{
    int num = Reader_ReadByte(msg);
    cfg.playerColor[num] = Reader_ReadByte(msg);
    players[num].colorMap = cfg.playerColor[num];
#if __JHEXEN__ || __JHERETIC__
    cfg.playerClass[num] = playerclass_t(Reader_ReadByte(msg));
    players[num].class_ = cfg.playerClass[num];
#endif

#if __JDOOM__ || __JDOOM64__
    App_Log(DE2_MAP_VERBOSE, "Player %i color set to %i", num, cfg.playerColor[num]);
#else
    App_Log(DE2_MAP_VERBOSE, "Player %i color set to %i and class to %i", num, cfg.playerColor[num], cfg.playerClass[num]);
#endif
}

/**
 * Send CONSOLEPLAYER's settings to the server.
 */
void NetCl_SendPlayerInfo()
{
    if(!IS_CLIENT) return;

    writer_s *msg = D_NetWrite();

    Writer_WriteByte(msg, cfg.common.netColor);
#ifdef __JHEXEN__
    Writer_WriteByte(msg, cfg.netClass);
#else
    Writer_WriteByte(msg, PCLASS_PLAYER);
#endif

    Net_SendPacket(0, GPT_PLAYER_INFO, Writer_Data(msg), Writer_Size(msg));
}

void NetCl_SaveGame(reader_s *msg)
{
#if __JHEXEN__
    DE_UNUSED(msg);
#endif

    if(Get(DD_PLAYBACK)) return;

#if !__JHEXEN__
    SV_SaveGameClient(Reader_ReadUInt32(msg));
#endif
#if __JDOOM__ || __JDOOM64__
    P_SetMessageWithFlags(&players[CONSOLEPLAYER], TXT_GAMESAVED, LMF_NO_HIDE);
#endif
}

void NetCl_LoadGame(reader_s *msg)
{
#if __JHEXEN__
    DE_UNUSED(msg);
#endif

    if(!IS_CLIENT || Get(DD_PLAYBACK)) return;

#if !__JHEXEN__
    SV_LoadGameClient(Reader_ReadUInt32(msg));
#endif
#if __JDOOM__ || __JDOOM64__
    P_SetMessage(&players[CONSOLEPLAYER], GET_TXT(TXT_CLNETLOAD));
#endif
}

void NetCl_CheatRequest(const char *command)
{
    writer_s *msg = D_NetWrite();

    Writer_WriteUInt16(msg, uint16_t(strlen(command)));
    Writer_Write(msg, command, strlen(command));

    if(IS_CLIENT)
    {
        Net_SendPacket(0, GPT_CHEAT_REQUEST, Writer_Data(msg), Writer_Size(msg));
    }
    else
    {
        NetSv_ExecuteCheat(CONSOLEPLAYER, command);
    }
}

void NetCl_UpdateJumpPower(reader_s *msg)
{
    netJumpPower = Reader_ReadFloat(msg);

    App_Log(DE2_LOG_VERBOSE, "Jump power: %g", netJumpPower);
}

void NetCl_DismissHUDs(reader_s *msg)
{
    dd_bool fast = Reader_ReadByte(msg)? true : false;
    ST_CloseAll(CONSOLEPLAYER, fast);
}

void NetCl_FloorHitRequest(player_t *player)
{
    writer_s *msg;
    mobj_t *mo;

    if(!IS_CLIENT || !player->plr->mo)
        return;

    mo = player->plr->mo;
    msg = D_NetWrite();

    App_Log(DE2_DEV_MAP_VERBOSE, "NetCl_FloorHitRequest: Player %i", (int)(player - players));

    // Include the position and momentum of the hit.
    Writer_WriteFloat(msg, mo->origin[VX]);
    Writer_WriteFloat(msg, mo->origin[VY]);
    Writer_WriteFloat(msg, mo->origin[VZ]);
    Writer_WriteFloat(msg, mo->mom[MX]);
    Writer_WriteFloat(msg, mo->mom[MY]);
    Writer_WriteFloat(msg, mo->mom[MZ]);

    Net_SendPacket(0, GPT_FLOOR_HIT_REQUEST, Writer_Data(msg), Writer_Size(msg));
}

void NetCl_PlayerActionRequest(player_t *player, int actionType, int actionParam)
{
    writer_s *msg;

    if(!IS_CLIENT)
        return;

    msg = D_NetWrite();

    App_Log(DE2_DEV_NET_VERBOSE, "NetCl_PlayerActionRequest: Player %i, action %i",
            (int)(player - players), actionType);

    // Type of the request.
    Writer_WriteInt32(msg, actionType);

    // Position of the action.
    if(G_GameState() == GS_MAP)
    {
        Writer_WriteFloat(msg, player->plr->mo->origin[VX]);
        Writer_WriteFloat(msg, player->plr->mo->origin[VY]);
        Writer_WriteFloat(msg, player->plr->mo->origin[VZ]);

        // Which way is the player looking at?
        Writer_WriteUInt32(msg, player->plr->mo->angle);
        Writer_WriteFloat(msg, player->plr->lookDir);
    }
    else
    {
        // Not in a map, so can't provide position/direction.
        Writer_WriteFloat(msg, 0);
        Writer_WriteFloat(msg, 0);
        Writer_WriteFloat(msg, 0);
        Writer_WriteUInt32(msg, 0);
        Writer_WriteFloat(msg, 0);
    }

    Writer_WriteInt32(msg, actionParam);

    Net_SendPacket(0, GPT_ACTION_REQUEST, Writer_Data(msg), Writer_Size(msg));
}

void NetCl_LocalMobjState(reader_s *msg)
{
    thid_t mobjId = Reader_ReadUInt16(msg);
    thid_t targetId = Reader_ReadUInt16(msg);
    int newState = 0;
    int special1 = 0;
    mobj_t* mo = 0;
    ddstring_t* stateName = Str_New();

    Str_Read(stateName, msg);
    newState = Defs().getStateNum(Str_Text(stateName));
    Str_Delete(stateName);

    special1 = Reader_ReadInt32(msg);

    if(!(mo = ClMobj_Find(mobjId)))
    {
        App_Log(DE2_DEV_MAP_NOTE, "NetCl_LocalMobjState: ClMobj %i not found", mobjId);
        return;
    }

    // Let it run the sequence locally.
    ClMobj_EnableLocalActions(mo, true);

    App_Log(DE2_DEV_MAP_VERBOSE, "ClMobj %i => state %i (target:%i, special1:%i)",
            mobjId, newState, targetId, special1);

    if(!targetId)
    {
        mo->target = NULL;
    }
    else
    {
        mo->target = ClMobj_Find(targetId);
    }
#if !defined(__JDOOM__) && !defined(__JDOOM64__)
    mo->special1 = special1;
#endif
    P_MobjChangeState(mo, statenum_t(newState));
}

void NetCl_DamageRequest(mobj_t *target, mobj_t *inflictor, mobj_t *source, int damage)
{
    if(!IS_CLIENT) return;
    if(!target) return;

    App_Log(DE2_DEV_NET_MSG,
            "NetCl_DamageRequest: Damage %i on target=%i via inflictor=%i by source=%i",
            damage, target->thinker.id, inflictor? inflictor->thinker.id : 0,
            source? source->thinker.id : 0);

    writer_s *msg = D_NetWrite();

    // Amount of damage.
    Writer_WriteInt32(msg, damage);

    // Mobjs.
    Writer_WriteUInt16(msg, target->thinker.id);
    Writer_WriteUInt16(msg, inflictor? inflictor->thinker.id : 0);
    Writer_WriteUInt16(msg, source? source->thinker.id : 0);

    Net_SendPacket(0, GPT_DAMAGE_REQUEST, Writer_Data(msg), Writer_Size(msg));
}

void NetCl_UpdateTotalCounts(reader_s *msg)
{
#ifndef __JHEXEN__
    totalKills  = Reader_ReadInt32(msg);
    totalItems  = Reader_ReadInt32(msg);
    totalSecret = Reader_ReadInt32(msg);

    App_Log(DE2_DEV_NET_MSG, "NetCl_UpdateTotalCounts: kills=%i, items=%i, secrets=%i",
            totalKills, totalItems, totalSecret);
#else
    DE_UNUSED(msg);
#endif
}
