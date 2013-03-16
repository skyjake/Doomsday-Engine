/**
 * @file d_netcl.c
 * Common code related to netgames (client-side). @ingroup client
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "p_saveg.h"
#include "d_net.h"
#include "d_netsv.h"
#include "p_player.h"
#include "p_map.h"
#include "p_start.h"
#include "g_common.h"
#include "p_actor.h"
#include "p_inventory.h"
#include "hu_inventory.h"
#include "st_stuff.h"

void NetCl_UpdateGameState(Reader* msg)
{
    byte len;
    byte gsFlags = 0;
    char gsGameIdentity[256];
    Uri* mapUri;
    byte gsEpisode = 0;
    byte gsMap = 0;
    byte gsMapEntryPoint = 0;
    byte configFlags = 0;
    byte gsDeathmatch = 0;
    byte gsMonsters = 0;
    byte gsRespawn = 0;
    byte gsJumping = 0;
    byte gsSkill = 0;
    coord_t gsGravity = 0;

    gsFlags = Reader_ReadByte(msg);

    // Game identity key.
    len = Reader_ReadByte(msg);
    Reader_Read(msg, gsGameIdentity, len);
    gsGameIdentity[len] = 0;

    // Current map.
    mapUri = Uri_FromReader(msg);

    gsEpisode = Reader_ReadByte(msg);
    gsMap = Reader_ReadByte(msg);

    /// @todo Not communicated to clients??
    //gsMapEntryPoint = ??;

    configFlags = Reader_ReadByte(msg);
    gsDeathmatch = configFlags & 0x3;
    gsMonsters = (configFlags & 0x4? true : false);
    gsRespawn = (configFlags & 0x8? true : false);
    gsJumping = (configFlags & 0x10? true : false);
    gsSkill = Reader_ReadByte(msg);
    gsGravity = Reader_ReadFloat(msg);

    VERBOSE(
        AutoStr* str = Uri_ToString(mapUri);
        Con_Message("NetCl_UpdateGameState: Flags=%x, Map uri=\"%s\"", gsFlags, Str_Text(str));
    )

    // Demo game state changes are only effective during demo playback.
    if(gsFlags & GSF_DEMO && !Get(DD_PLAYBACK))
        return;

    // Check for a game mode mismatch.
    /// @todo  Automatically load the server's game if it is available.
    /// However, note that this can only occur if the server changes its game
    /// while a netgame is running (which currently will end the netgame).
    {
        GameInfo gameInfo;
        DD_GameInfo(&gameInfo);
        if(strcmp(gameInfo.identityKey, gsGameIdentity))
        {
            Con_Message("NetCl_UpdateGameState: Server's game mode (%s) is different than yours (%s).",
                        gsGameIdentity, gameInfo.identityKey);
            DD_Execute(false, "net disconnect");
            return;
        }
    }

    deathmatch = gsDeathmatch;
    noMonstersParm = !gsMonsters;
#if !__JHEXEN__
    respawnMonsters = gsRespawn;
#endif

    // Some statistics.
#if __JHEXEN__
    Con_Message("Game state: Map=%u Skill=%i %s", gsMap+1, gsSkill,
                deathmatch == 1 ? "Deathmatch" : deathmatch ==
                2 ? "Deathmatch2" : "Co-op");
#else
    Con_Message("Game state: Map=%u Episode=%u Skill=%i %s", gsMap+1,
                gsEpisode+1, gsSkill,
                deathmatch == 1 ? "Deathmatch" : deathmatch ==
                2 ? "Deathmatch2" : "Co-op");
#endif
#if !__JHEXEN__
    Con_Message("  Respawn=%s Monsters=%s Jumping=%s Gravity=%.1f",
                respawnMonsters ? "yes" : "no", !noMonstersParm ? "yes" : "no",
                gsJumping ? "yes" : "no", gsGravity);
#else
    Con_Message("  Monsters=%s Jumping=%s Gravity=%.1f",
                !noMonstersParm ? "yes" : "no",
                gsJumping ? "yes" : "no", gsGravity);
#endif

    // Do we need to change the map?
    if(gsFlags & GSF_CHANGE_MAP)
    {
        G_NewGame(gsSkill, gsEpisode, gsMap, gameMapEntryPoint /*gsMapEntryPoint*/);

        /// @todo Necessary?
        G_SetGameAction(GA_NONE);
    }
    else
    {
        gameSkill = gsSkill;
        gameEpisode = gsEpisode;
        gameMap = gsMap;

        /// @todo Not communicated to clients??
        //gameMapEntryPoint = gsMapEntryPoint;
    }

    // Set gravity.
    /// @todo This is a map-property, not a global property.
    DD_SetVariable(DD_GRAVITY, &gsGravity);

    // Camera init included?
    if(gsFlags & GSF_CAMERA_INIT)
    {
        player_t* pl = &players[CONSOLEPLAYER];
        mobj_t* mo;

        mo = pl->plr->mo;
        if(mo)
        {
            P_MobjUnsetOrigin(mo);
            mo->origin[VX] = Reader_ReadFloat(msg);
            mo->origin[VY] = Reader_ReadFloat(msg);
            mo->origin[VZ] = Reader_ReadFloat(msg);
            P_MobjSetOrigin(mo);
            mo->angle = Reader_ReadUInt32(msg);
            // Update floorz and ceilingz.
#if __JDOOM__ || __JDOOM64__
            P_CheckPosition(mo, mo->origin);
#else
            P_CheckPositionXY(mo, mo->origin[VX], mo->origin[VY]);
#endif
            mo->floorZ = tmFloorZ;
            mo->ceilingZ = tmCeilingZ;
        }
        else // mo == NULL
        {
            float mx = Reader_ReadFloat(msg);
            float my = Reader_ReadFloat(msg);
            float mz = Reader_ReadFloat(msg);
            angle_t angle = Reader_ReadUInt32(msg);
            Con_Message("NetCl_UpdateGameState: Got camera init, but player has no mobj.");
            Con_Message("  Pos=%f,%f,%f Angle=%x", mx, my, mz, angle);
        }
    }

    // Tell the server we're ready to begin receiving frames.
    Net_SendPacket(0, DDPT_OK, 0, 0);
}

void NetCl_MobjImpulse(Reader* msg)
{
    mobj_t* mo = players[CONSOLEPLAYER].plr->mo;
    mobj_t* clmo = ClPlayer_ClMobj(CONSOLEPLAYER);
    thid_t id = 0;

    if(!mo || !clmo) return;

    id = Reader_ReadUInt16(msg);
    if(id != clmo->thinker.id)
    {
        // Not applicable; wrong mobj.
        return;
    }

#ifdef _DEBUG
    Con_Message("NetCl_MobjImpulse: Player %i, clmobj %i", CONSOLEPLAYER, id);
#endif

    // Apply to the local mobj.
    mo->mom[MX] += Reader_ReadFloat(msg);
    mo->mom[MY] += Reader_ReadFloat(msg);
    mo->mom[MZ] += Reader_ReadFloat(msg);
}

void NetCl_PlayerSpawnPosition(Reader* msg)
{
    player_t* p = &players[CONSOLEPLAYER];
    coord_t x, y, z;
    angle_t angle;
    mobj_t* mo;

    x = Reader_ReadFloat(msg);
    y = Reader_ReadFloat(msg);
    z = Reader_ReadFloat(msg);
    angle = Reader_ReadUInt32(msg);

#ifdef _DEBUG
    Con_Message("NetCl_PlayerSpawnPosition: Got spawn position %f, %f, %f facing %x",
                x, y, z, angle);
#endif

    mo = p->plr->mo;
    assert(mo != 0);

    P_TryMoveXYZ(mo, x, y, z);
    mo->angle = angle;
}

void NetCl_UpdatePlayerState2(Reader* msg, int plrNum)
{
    player_t *pl = &players[plrNum];
    unsigned int flags;
    //int     oldstate = pl->playerState;
    byte    b;
    int     i, k;

    if(!Get(DD_GAME_READY))
    {
#ifdef _DEBUG
        Con_Message("NetCl_UpdatePlayerState2: Discarded because game isn't ready.");
#endif
        return;
    }

    if(plrNum < 0)
    {
        // Player number included in the message.
        plrNum = Reader_ReadByte(msg);
    }
    flags = Reader_ReadUInt32(msg);

    if(flags & PSF2_OWNED_WEAPONS)
    {
        boolean val;

        k = Reader_ReadUInt16(msg);
        for(i = 0; i < NUM_WEAPON_TYPES; ++i)
        {
            val = (k & (1 << i)) != 0;

            // Maybe unhide the HUD?
            if(val == true && pl->weapons[i].owned == false)
                ST_HUDUnHide(pl - players, HUE_ON_PICKUP_WEAPON);

            pl->weapons[i].owned = val;
        }
    }

    if(flags & PSF2_STATE)
    {
        int oldPlayerState = pl->playerState;

        b = Reader_ReadByte(msg);
        pl->playerState = b & 0xf;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        pl->armorType = b >> 4;
#endif

#ifdef _DEBUG
        Con_Message("NetCl_UpdatePlayerState2: New state = %s",
                    pl->playerState == PST_LIVE?  "PST_LIVE" :
                    pl->playerState == PST_DEAD? "PST_DEAD" : "PST_REBORN");
#endif

        // Player state changed?
        if(oldPlayerState != pl->playerState)
        {
            // Set or clear the DEAD flag for this player.
            if(pl->playerState == PST_LIVE)
            {
                // Becoming alive again...
                // After being reborn, the server will tell us the new weapon.
                pl->plr->flags |= DDPF_UNDEFINED_WEAPON;
#ifdef _DEBUG
                Con_Message("NetCl_UpdatePlayerState2: Player %i: Marking weapon as undefined.", (int)(pl - players));
#endif

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

void NetCl_UpdatePlayerState(Reader *msg, int plrNum)
{
    int i;
    player_t* pl = 0;
    byte b;
    int flags, s;

    if(!Get(DD_GAME_READY))
        return;

    if(plrNum < 0)
    {
        plrNum = Reader_ReadByte(msg);
    }
    pl = &players[plrNum];

    flags = Reader_ReadUInt16(msg);

    /*
#ifdef _DEBUG
    VERBOSE( Con_Message("NetCl_UpdatePlayerState: fl=%x", flags) );
#endif
    */

    if(flags & PSF_STATE)       // and armor type (the same bit)
    {
        b = Reader_ReadByte(msg);
        pl->playerState = b & 0xf;
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
#if _DEBUG
            Con_Message("FIXME: NetCl_UpdatePlayerState: Player mobj not yet allocated at this time, ignoring.");
#endif
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
        uint i, count;

        for(i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
        {
            inventoryitemtype_t type = IIT_FIRST + i;
            uint j, count = P_InventoryCount(plrNum, type);

            for(j = 0; j < count; ++j)
                P_InventoryTake(plrNum, type, true);
        }

        count = Reader_ReadByte(msg);
        for(i = 0; i < count; ++i)
        {
            inventoryitemtype_t type;
            uint j, num;

            s = Reader_ReadUInt16(msg);
            type = s & 0xff;
            num = s >> 8;

            for(j = 0; j < num; ++j)
                P_InventoryGive(plrNum, type, true);
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

                // Maybe unhide the HUD?
                if(val > pl->powers[i])
                    ST_HUDUnHide(plrNum, HUE_ON_PICKUP_POWER);

                pl->powers[i] = val;

                // Should we reveal the map?
                if(val && i == PT_ALLMAP && plrNum == CONSOLEPLAYER)
                {
#ifdef _DEBUG
                    Con_Message("NetCl_UpdatePlayerState: Revealing automap.");
#endif
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
            boolean val = (b & (1 << i)) != 0;

            // Maybe unhide the HUD?
            if(val && !pl->keys[i])
                ST_HUDUnHide(plrNum, HUE_ON_PICKUP_KEY);

            pl->keys[i] = val;
        }
#endif
    }

    if(flags & PSF_FRAGS)
    {
        memset(pl->frags, 0, sizeof(pl->frags));
        // First comes the number of frag counts included.
        for(i = Reader_ReadByte(msg); i > 0; i--)
        {
            s = Reader_ReadUInt16(msg);
            pl->frags[s >> 12] = s & 0xfff;
        }
    }

    if(flags & PSF_OWNED_WEAPONS)
    {
        boolean val;

        b = Reader_ReadByte(msg);
        for(i = 0; i < NUM_WEAPON_TYPES; ++i)
        {
            val = (b & (1 << i)) != 0;

            // Maybe unhide the HUD?
            if(val == true && pl->weapons[i].owned == false)
                ST_HUDUnHide(plrNum, HUE_ON_PICKUP_WEAPON);

            pl->weapons[i].owned = val;
        }
    }

    if(flags & PSF_AMMO)
    {
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            int val = Reader_ReadInt16(msg);

            // Maybe unhide the HUD?
            if(val > pl->ammo[i].owned)
                ST_HUDUnHide(plrNum, HUE_ON_PICKUP_AMMO);

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

#ifdef _DEBUG
        Con_Message("NetCl_UpdatePlayerState: kills=%i, items=%i, secrets=%i",
                    pl->killCount, pl->itemCount, pl->secretCount);
#endif
    }

    if(flags & PSF_PENDING_WEAPON || flags & PSF_READY_WEAPON)
    {
        boolean wasUndefined = (pl->plr->flags & DDPF_UNDEFINED_WEAPON) != 0;

        b = Reader_ReadByte(msg);
        if(flags & PSF_PENDING_WEAPON)
        {
            if(!wasUndefined)
            {
                int weapon = b & 0xf;
                if(weapon != WT_NOCHANGE)
                {
                    P_Impulse(pl - players, CTL_WEAPON1 + weapon);
#ifdef _DEBUG
                    Con_Message("NetCl_UpdatePlayerState: Weapon already known, using an impulse to switch to %i.", weapon);
#endif
                }
            }
            else
            {
                pl->pendingWeapon = b & 0xf;
#ifdef _DEBUG
                Con_Message("NetCl_UpdatePlayerState: pendingweapon=%i", pl->pendingWeapon);
#endif
            }

            pl->plr->flags &= ~DDPF_UNDEFINED_WEAPON;
        }

        if(flags & PSF_READY_WEAPON)
        {
            if(wasUndefined)
            {
                pl->readyWeapon = b >> 4;
#ifdef _DEBUG
                Con_Message("NetCl_UpdatePlayerState: readyweapon=%i", pl->readyWeapon);
#endif
            }
            else
            {
#ifdef _DEBUG
                Con_Message("NetCl_UpdatePlayerState: Readyweapon already known (%i), not setting server's value %i.",
                            pl->readyWeapon, b >> 4);
#endif
            }

            pl->plr->flags &= ~DDPF_UNDEFINED_WEAPON;
        }

        if(!(pl->plr->flags & DDPF_UNDEFINED_WEAPON) && wasUndefined)
        {
#ifdef _DEBUG
            Con_Message("NetCl_UpdatePlayerState: Weapon was undefined, bringing it up now.");
#endif

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
#ifdef _DEBUG
        Con_Message("NetCl_UpdatePlayerState: Player %i morphtics = %i", plrNum, pl->morphTics);
#endif
    }
#endif

#if __JHEXEN__ || __JSTRIFE__
    if(flags & PSF_LOCAL_QUAKE)
    {
        localQuakeHappening[plrNum] = Reader_ReadByte(msg);
    }
#endif
}

void NetCl_UpdatePSpriteState(Reader *msg)
{
    // Not used.
    /*
    unsigned short s;

    NetCl_SetReadBuffer(data);
    s = NetCl_ReadShort();
    P_SetPsprite(&players[CONSOLEPLAYER], ps_weapon, s);
     */
}

void NetCl_Intermission(Reader* msg)
{
    int flags = Reader_ReadByte(msg);

    if(flags & IMF_BEGIN)
    {
        uint i;

        // Close any HUDs left open at the end of the previous map.
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            ST_AutomapOpen(i, false, true);
#if __JHERETIC__ || __JHEXEN__
            Hu_InventoryOpen(i, false);
#endif
        }

        GL_SetFilter(false);

#if __JHEXEN__
        SN_StopAllSequences();
#endif

        // @todo jHeretic does not transmit the intermission info!
#if __JDOOM__ || __JDOOM64__
        wmInfo.maxKills = Reader_ReadUInt16(msg);
        wmInfo.maxItems = Reader_ReadUInt16(msg);
        wmInfo.maxSecret = Reader_ReadUInt16(msg);
        wmInfo.nextMap = Reader_ReadByte(msg);
        wmInfo.currentMap = Reader_ReadByte(msg);
        wmInfo.didSecret = Reader_ReadByte(msg);
        wmInfo.episode = gameEpisode;

        G_PrepareWIData();
#elif __JHERETIC__
        wmInfo.episode = gameEpisode;
#elif __JHEXEN__
        nextMap = Reader_ReadByte(msg);
        nextMapEntryPoint = Reader_ReadByte(msg);
#endif

#if __JDOOM__ || __JDOOM64__
        WI_Init(&wmInfo);
#elif __JHERETIC__
        IN_Init(&wmInfo);
#elif __JHEXEN__
        IN_Init();
#endif
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
#if __JDOOM__ || __JDOOM64__
        WI_End();
#elif __JHERETIC__ || __JHEXEN__
        IN_Stop();
#endif
    }

    if(flags & IMF_STATE)
    {
#if __JDOOM__ || __JDOOM64__
        WI_SetState(Reader_ReadInt16(msg));
#elif __JHERETIC__ || __JHEXEN__
        interState = Reader_ReadInt16(msg);
#endif
    }

#if __JHERETIC__
    if(flags & IMF_TIME)
        interTime = Reader_ReadUInt16(msg);
#endif
}

#if 0 // MOVED INTO THE ENGINE
/**
 * This is where clients start their InFine interludes.
 */
void NetCl_Finale(int packetType, Reader* msg)
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

/**
 * Clients have other players' info, but it's only "FYI"; they don't really need it.
 */
void NetCl_UpdatePlayerInfo(Reader *msg)
{
    int num;

    num = Reader_ReadByte(msg);
    cfg.playerColor[num] = Reader_ReadByte(msg);
    players[num].colorMap = cfg.playerColor[num];
#if __JHEXEN__ || __JHERETIC__
    cfg.playerClass[num] = Reader_ReadByte(msg);
    players[num].class_ = cfg.playerClass[num];
#endif

#if __JDOOM__ || __JDOOM64__
    Con_Message("NetCl_UpdatePlayerInfo: pl=%i color=%i", num, cfg.playerColor[num]);
#else
    Con_Message("NetCl_UpdatePlayerInfo: pl=%i color=%i class=%i", num, cfg.playerColor[num], cfg.playerClass[num]);
#endif
}

/**
 * Send CONSOLEPLAYER's settings to the server.
 */
void NetCl_SendPlayerInfo()
{
    Writer* msg;

    if(!IS_CLIENT)
        return;

    msg = D_NetWrite();

    Writer_WriteByte(msg, cfg.netColor);
#ifdef __JHEXEN__
    Writer_WriteByte(msg, cfg.netClass);
#else
    Writer_WriteByte(msg, PCLASS_PLAYER);
#endif

    Net_SendPacket(0, GPT_PLAYER_INFO, Writer_Data(msg), Writer_Size(msg));
}

void NetCl_SaveGame(Reader* msg)
{
#if __JHEXEN__
    DENG_UNUSED(msg);
#endif

    if(Get(DD_PLAYBACK)) return;

#if !__JHEXEN__
    SV_SaveGameClient(Reader_ReadUInt32(msg));
#endif
#if __JDOOM__ || __JDOOM64__
    P_SetMessage(&players[CONSOLEPLAYER], LMF_NO_HIDE, TXT_GAMESAVED);
#endif
}

void NetCl_LoadGame(Reader* msg)
{
#if __JHEXEN__
    DENG_UNUSED(msg);
#endif

    if(!IS_CLIENT || Get(DD_PLAYBACK)) return;

#if !__JHEXEN__
    SV_LoadGameClient(Reader_ReadUInt32(msg));
#endif
#if __JDOOM__ || __JDOOM64__
    P_SetMessage(&players[CONSOLEPLAYER], 0, GET_TXT(TXT_CLNETLOAD));
#endif
}

/**
 * Pause or unpause the game.
 */
void NetCl_Paused(Reader* msg)
{
    DD_SetInteger(DD_CLIENT_PAUSED, Reader_ReadByte(msg));
}

/**
 * Send a GPT_CHEAT_REQUEST packet to the server. If the server is allowing
 * netgame cheating, the cheat will be executed on the server.
 */
void NetCl_CheatRequest(const char *command)
{
    Writer* msg = D_NetWrite();

    Writer_WriteUInt16(msg, strlen(command));
    Writer_Write(msg, command, strlen(command));

    if(IS_CLIENT)
        Net_SendPacket(0, GPT_CHEAT_REQUEST, Writer_Data(msg), Writer_Size(msg));
    else
        NetSv_ExecuteCheat(CONSOLEPLAYER, command);
}

/**
 * Set the jump power used in client mode.
 */
void NetCl_UpdateJumpPower(Reader* msg)
{
    netJumpPower = Reader_ReadFloat(msg);
#ifdef _DEBUG
    Con_Message("NetCl_UpdateJumpPower: %g", netJumpPower);
#endif
}

void NetCl_FloorHitRequest(player_t* player)
{
    Writer* msg;
    mobj_t* mo;

    if(!IS_CLIENT || !player->plr->mo)
        return;

    mo = player->plr->mo;
    msg = D_NetWrite();

#ifdef _DEBUG
    Con_Message("NetCl_FloorHitRequest: Player %i.", (int)(player - players));
#endif

    // Include the position and momentum of the hit.
    Writer_WriteFloat(msg, mo->origin[VX]);
    Writer_WriteFloat(msg, mo->origin[VY]);
    Writer_WriteFloat(msg, mo->origin[VZ]);
    Writer_WriteFloat(msg, mo->mom[MX]);
    Writer_WriteFloat(msg, mo->mom[MY]);
    Writer_WriteFloat(msg, mo->mom[MZ]);

    Net_SendPacket(0, GPT_FLOOR_HIT_REQUEST, Writer_Data(msg), Writer_Size(msg));
}

/**
 * Sends a player action request. The server will execute the action.
 * This is more reliable than sending via the ticcmds, as the client will
 * determine exactly when and where the action takes place. On serverside,
 * the clients position and angle may not be up to date when a ticcmd
 * arrives.
 */
void NetCl_PlayerActionRequest(player_t *player, int actionType, int actionParam)
{
    Writer* msg;

    if(!IS_CLIENT)
        return;

    msg = D_NetWrite();

#ifdef _DEBUG
    Con_Message("NetCl_PlayerActionRequest: Player %i, action %i.",
                (int)(player - players), actionType);
#endif

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

    if(actionType == GPA_CHANGE_WEAPON || actionType == GPA_USE_FROM_INVENTORY)
    {
        Writer_WriteInt32(msg, actionParam);
    }
    else
    {
        // Currently active weapon.
        Writer_WriteInt32(msg, player->readyWeapon);
    }

    Net_SendPacket(0, GPT_ACTION_REQUEST, Writer_Data(msg), Writer_Size(msg));
}

void NetCl_LocalMobjState(Reader* msg)
{
    thid_t mobjId = Reader_ReadUInt16(msg);
    thid_t targetId = Reader_ReadUInt16(msg);
    int newState = 0;
    int special1 = 0;
    mobj_t* mo = 0;
    ddstring_t* stateName = Str_New();

    Str_Read(stateName, msg);
    newState = Def_Get(DD_DEF_STATE, Str_Text(stateName), 0);
    Str_Delete(stateName);

    special1 = Reader_ReadInt32(msg);

    if(!(mo = ClMobj_Find(mobjId)))
    {
#ifdef _DEBUG
        Con_Message("NetCl_LocalMobjState: ClMobj %i not found.", mobjId);
        return;
#endif
    }

    // Let it run the sequence locally.
    ClMobj_EnableLocalActions(mo, true);

#ifdef _DEBUG
    Con_Message("NetCl_LocalMobjState: ClMobj %i => state %i (target:%i, special1:%i)",
                mobjId, newState, targetId, special1);
#endif

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
    P_MobjChangeState(mo, newState);
}

void NetCl_DamageRequest(mobj_t* target, mobj_t* inflictor, mobj_t* source, int damage)
{
    Writer* msg;

    if(!IS_CLIENT || !target) return;

    msg = D_NetWrite();

#ifdef _DEBUG
    Con_Message("NetCl_DamageRequest: Damage %i on target=%i via inflictor=%i by source=%i.",
                damage, target->thinker.id, inflictor? inflictor->thinker.id : 0,
                source? source->thinker.id : 0);
#endif

    // Amount of damage.
    Writer_WriteInt32(msg, damage);

    // Mobjs.
    Writer_WriteUInt16(msg, target->thinker.id);
    Writer_WriteUInt16(msg, inflictor? inflictor->thinker.id : 0);
    Writer_WriteUInt16(msg, source? source->thinker.id : 0);

    Net_SendPacket(0, GPT_DAMAGE_REQUEST, Writer_Data(msg), Writer_Size(msg));
}
