/** @file d_main.cpp  Doom-specific game initialization.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#include "jdoom.h"

#include "d_netsv.h"
#include "gamesession.h"
#include "m_argv.h"
#include "p_map.h"
#include "doomv9mapstatereader.h"
#include "am_map.h"
#include "g_defs.h"
#include "saveslots.h"

using namespace common;

int verbose;

float turboMul; // Multiplier for turbo.

gamemode_t gameMode;
int gameModeBits;

// Default font colors.
float defFontRGB[3];
float defFontRGB2[3];
float defFontRGB3[3];

// The patches used in drawing the view border.
// Percent-encoded.
char const *borderGraphics[] = {
    "Flats:FLOOR7_2", // Background.
    "BRDR_T", // Top.
    "BRDR_R", // Right.
    "BRDR_B", // Bottom.
    "BRDR_L", // Left.
    "BRDR_TL", // Top left.
    "BRDR_TR", // Top right.
    "BRDR_BR", // Bottom right.
    "BRDR_BL" // Bottom left.
};

int D_GetInteger(int id)
{
    return Common_GetInteger(id);
}

void *D_GetVariable(int id)
{
    static float bob[2];

    switch(id)
    {
    case DD_PLUGIN_NAME:
        return (void*)PLUGIN_NAMETEXT;

    case DD_PLUGIN_NICENAME:
        return (void*)PLUGIN_NICENAME;

    case DD_PLUGIN_VERSION_SHORT:
        return (void*)PLUGIN_VERSION_TEXT;

    case DD_PLUGIN_VERSION_LONG:
        return (void*)(PLUGIN_VERSION_TEXTLONG "\n" PLUGIN_DETAILS);

    case DD_PLUGIN_HOMEURL:
        return (void*)PLUGIN_HOMEURL;

    case DD_PLUGIN_DOCSURL:
        return (void*)PLUGIN_DOCSURL;

    case DD_GAME_CONFIG:
        return gameConfigString;

    case DD_ACTION_LINK:
        return actionlinks;

    case DD_XGFUNC_LINK:
        return xgClasses;

    case DD_PSPRITE_BOB_X:
        R_GetWeaponBob(DISPLAYPLAYER, &bob[0], 0);
        return &bob[0];

    case DD_PSPRITE_BOB_Y:
        R_GetWeaponBob(DISPLAYPLAYER, 0, &bob[1]);
        return &bob[1];

    case DD_TM_FLOOR_Z:
        return (void*) &tmFloorZ;

    case DD_TM_CEILING_Z:
        return (void*) &tmCeilingZ;

    default:
        break;
    }
    return 0;
}

void D_PreInit()
{
    // Configure default colors:
    switch(gameMode)
    {
    case doom2_hacx:
        defFontRGB[CR] = .85f;
        defFontRGB[CG] = 0;
        defFontRGB[CB] = 0;

        defFontRGB2[CR] = .2f;
        defFontRGB2[CG] = .9f;
        defFontRGB2[CB] = .2f;

        defFontRGB3[CR] = .2f;
        defFontRGB3[CG] = .9f;
        defFontRGB3[CB] = .2f;
        break;

    case doom_chex:
        defFontRGB[CR] = .46f;
        defFontRGB[CG] = 1;
        defFontRGB[CB] = .4f;

        defFontRGB2[CR] = .46f;
        defFontRGB2[CG] = 1;
        defFontRGB2[CB] = .4f;

        defFontRGB3[CR] = 1;
        defFontRGB3[CG] = 1;
        defFontRGB3[CB] = .45f;
        break;

    default:
        defFontRGB[CR] = 1;
        defFontRGB[CG] = 1;
        defFontRGB[CB] = 1;

        defFontRGB2[CR] = .85f;
        defFontRGB2[CG] = 0;
        defFontRGB2[CB] = 0;

        defFontRGB3[CR] = 1;
        defFontRGB3[CG] = .9f;
        defFontRGB3[CB] = .4f;
        break;
    }

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.povLookAround = true;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.echoMsg = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;

    cfg.menuPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.menuScale = .9f;
    cfg.menuTextGlitter = .5f;
    cfg.menuShadow = 0.33f;
    cfg.menuQuitSound = true;
    cfg.menuSlam = false;
    cfg.menuShortcutsEnabled = true;
    cfg.menuGameSaveSuggestDescription = true;
    cfg.menuEffectFlags = MEF_TEXT_TYPEIN|MEF_TEXT_SHADOW|MEF_TEXT_GLITTER;
    cfg.menuTextFlashColor[0] = .7f;
    cfg.menuTextFlashColor[1] = .9f;
    cfg.menuTextFlashColor[2] = 1;
    cfg.menuTextFlashSpeed = 4;
    if(gameMode != doom_chex)
    {
        cfg.menuCursorRotate = true;
    }
    if(gameMode == doom2_hacx)
    {
        cfg.menuTextColors[0][CR] = cfg.menuTextColors[0][CG] = cfg.menuTextColors[0][CB] = 1;
        memcpy(cfg.menuTextColors[1], defFontRGB, sizeof(cfg.menuTextColors[1]));
        cfg.menuTextColors[2][CR] = cfg.menuTextColors[3][CR] = .2f;
        cfg.menuTextColors[2][CG] = cfg.menuTextColors[3][CG] = .2f;
        cfg.menuTextColors[2][CB] = cfg.menuTextColors[3][CB] = .9f;
    }
    else
    {
        memcpy(cfg.menuTextColors[0], defFontRGB2, sizeof(cfg.menuTextColors[0]));
        if(gameMode == doom_chex)
        {
            cfg.menuTextColors[1][CR] = .85f;
            cfg.menuTextColors[1][CG] = .3f;
            cfg.menuTextColors[1][CB] = .3f;
        }
        else
        {
            cfg.menuTextColors[1][CR] = 1.f;
            cfg.menuTextColors[1][CG] = .7f;
            cfg.menuTextColors[1][CB] = .3f;
        }
        memcpy(cfg.menuTextColors[2], defFontRGB,  sizeof(cfg.menuTextColors[2]));
        memcpy(cfg.menuTextColors[3], defFontRGB2, sizeof(cfg.menuTextColors[3]));
    }

    cfg.inludePatchReplaceMode = PRM_ALLOW_TEXT;

    cfg.hudPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.hudKeysCombine = false;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_FRAGS] = true;
    cfg.hudShown[HUD_FACE] = false;
    cfg.hudShown[HUD_LOG] = true;
    for(int i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
    {
        cfg.hudUnHide[i] = 1;
    }
    cfg.hudScale = .6f;

    memcpy(cfg.hudColor, defFontRGB2, sizeof(cfg.hudColor));
    cfg.hudColor[CA] = 1;

    cfg.hudFog = 1;
    cfg.hudIconAlpha = 1;
    cfg.xhairAngle = 0;
    cfg.xhairSize = .5f;
    cfg.xhairVitality = false;
    cfg.xhairColor[0] = 1;
    cfg.xhairColor[1] = 1;
    cfg.xhairColor[2] = 1;
    cfg.xhairColor[3] = 1;

    cfg.filterStrength = .8f;
    cfg.moveCheckZ = true;
    cfg.jumpPower = 9;
    cfg.airborneMovement = 1;
    cfg.weaponAutoSwitch = 1; // if better
    cfg.noWeaponAutoSwitchIfFiring = false;
    cfg.ammoAutoSwitch = 0; // never
    cfg.secretMsg = true;
    cfg.slidingCorpses = false;
    //cfg.fastMonsters = false;
    cfg.netJumping = true;
    cfg.netEpisode = 0;
    cfg.netMap = 0;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 4;
    cfg.netBFGFreeLook = 0;    // allow free-aim 0=none 1=not BFG 2=All
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1;        // use map default
    cfg.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.mapTitle = true;
    cfg.automapTitleAtBottom = true;
    cfg.hideIWADAuthor = true;

    cfg.confirmQuickGameSave = true;
    cfg.confirmRebornLoad = true;
    cfg.loadLastSaveOnReborn = false;

    cfg.maxSkulls = true;
    cfg.allowSkullsInWalls = false;
    cfg.anyBossDeath = false;
    cfg.monstersStuckInDoors = false;
    cfg.avoidDropoffs = true;
    cfg.moveBlock = false;
    cfg.fallOff = true;
    cfg.fixOuchFace = true;
    cfg.fixStatusbarOwnedWeapons = true;

    cfg.statusbarScale = 1;
    cfg.statusbarOpacity = 1;
    cfg.statusbarCounterAlpha = 1;

    cfg.automapCustomColors = 0; // Never.
    cfg.automapL0[0] = .4f; // Unseen areas
    cfg.automapL0[1] = .4f;
    cfg.automapL0[2] = .4f;

    cfg.automapL1[0] = 1.f; // onesided lines
    cfg.automapL1[1] = 0.f;
    cfg.automapL1[2] = 0.f;

    cfg.automapL2[0] = .77f; // floor height change lines
    cfg.automapL2[1] = .6f;
    cfg.automapL2[2] = .325f;

    cfg.automapL3[0] = 1.f; // ceiling change lines
    cfg.automapL3[1] = .95f;
    cfg.automapL3[2] = 0.f;

    cfg.automapMobj[0] = 0.f;
    cfg.automapMobj[1] = 1.f;
    cfg.automapMobj[2] = 0.f;

    cfg.automapBack[0] = 0.f;
    cfg.automapBack[1] = 0.f;
    cfg.automapBack[2] = 0.f;
    cfg.automapOpacity = .7f;
    cfg.automapLineAlpha = .7f;
    cfg.automapLineWidth = 1.1f;
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = false;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.automapOpenSeconds = AUTOMAP_OPEN_SECONDS;

    cfg.hudCheatCounterScale = .7f;
    cfg.hudCheatCounterShowWithAutomap = true;

    if(gameMode == doom_chex)
    {
        cfg.hudKeysCombine = true;
    }

    cfg.msgCount = 4;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5;
    cfg.msgAlign = 0; // Left.
    cfg.msgBlink = 5;

    if(gameMode == doom2_hacx)
    {
        cfg.msgColor[CR] = .2f;
        cfg.msgColor[CG] = .2f;
        cfg.msgColor[CB] = .9f;
    }
    else
    {
        memcpy(cfg.msgColor, defFontRGB2, sizeof(cfg.msgColor));
    }

    cfg.chatBeep = true;

    cfg.killMessages = true;
    cfg.bobWeapon = 1;
    cfg.bobView = 1;
    cfg.bobWeaponLower = true;
    cfg.cameraNoClip = true;
    cfg.respawnMonstersNightmare = true;

    cfg.weaponOrder[0] = WT_SIXTH;
    cfg.weaponOrder[1] = WT_NINETH;
    cfg.weaponOrder[2] = WT_FOURTH;
    cfg.weaponOrder[3] = WT_THIRD;
    cfg.weaponOrder[4] = WT_SECOND;
    cfg.weaponOrder[5] = WT_EIGHTH;
    cfg.weaponOrder[6] = WT_FIFTH;
    cfg.weaponOrder[7] = WT_SEVENTH;
    cfg.weaponOrder[8] = WT_FIRST;

    cfg.weaponCycleSequential = true;
    cfg.berserkAutoSwitch = true;

    // Use the DOOM transition by default.
    Con_SetInteger("con-transition", 1);

    // Do the common pre init routine;
    G_CommonPreInit();

    G_InitSpecialFilter();
}

void D_PostInit()
{
    dd_bool autoStart = false;
    Uri *startMapUri = 0;

    /// @todo Kludge: Border background is different in DOOM2.
    /// @todo Do this properly!
    if(gameModeBits & GM_ANY_DOOM2)
        borderGraphics[0] = "Flats:GRNROCK";
    else
        borderGraphics[0] = "Flats:FLOOR7_2";

    // Common post init routine
    G_CommonPostInit();

    // Initialize ammo info.
    P_InitAmmoInfo();

    // Initialize weapon info.
    P_InitWeaponInfo();

    // Game parameters.
    monsterInfight = GetDefInt("AI|Infight", 0);

    // Get skill / episode / map from parms.
    defaultGameRules.skill = /*startSkill =*/ SM_MEDIUM;

    if(CommandLine_Check("-altdeath"))
        cfg.netDeathmatch = 2;
    else if(CommandLine_Check("-deathmatch"))
        cfg.netDeathmatch = 1;

    // Apply these rules.
    defaultGameRules.noMonsters      = CommandLine_Check("-nomonsters")? true : false;
    defaultGameRules.respawnMonsters = CommandLine_Check("-respawn")? true : false;
    defaultGameRules.fast            = CommandLine_Check("-fast")? true : false;

    int p = CommandLine_Check("-timer");
    if(p && p < myargc - 1 && defaultGameRules.deathmatch)
    {
        int time = atoi(CommandLine_At(p + 1));
        App_Log(DE2_LOG_NOTE, "Maps will end after %d %s", time, time == 1? "minute" : "minutes");
    }

    // Turbo option.
    p = CommandLine_Check("-turbo");
    turboMul = 1.0f;
    if(p)
    {
        int scale = 200;

        if(p < myargc - 1)
            scale = atoi(CommandLine_At(p + 1));
        if(scale < 10)
            scale = 10;
        if(scale > 400)
            scale = 400;

        App_Log(DE2_MAP_NOTE, "Turbo scale: %i%%", scale);
        turboMul = scale / 100.f;
    }

    // Load a saved game?
    p = CommandLine_Check("-loadgame");
    if(p && p < myargc - 1)
    {
        if(SaveSlot *sslot = G_SaveSlots().slotByUserInput(CommandLine_At(p + 1)))
        {
            if(sslot->isUserWritable() && G_SetGameActionLoadSession(sslot->id()))
            {
                // No further initialization is to be done.
                return;
            }
        }
    }

    p = CommandLine_Check("-skill");
    if(p && p < myargc - 1)
    {
        int skillNumber = atoi(CommandLine_At(p + 1));
        defaultGameRules.skill = (skillmode_t)(skillNumber > 0? skillNumber - 1 : skillNumber);
        autoStart = true;
    }

    p = CommandLine_Check("-episode");
    if(p && p < myargc - 1)
    {
        int episodeNumber = atoi(CommandLine_At(p + 1));

        startMapUri = G_ComposeMapUri(episodeNumber > 0? episodeNumber - 1 : episodeNumber, 0);
        autoStart = true;
    }

    p = CommandLine_Check("-warp");
    if(p && p < myargc - 1)
    {
        if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        {
            int mapNumber = atoi(CommandLine_At(p + 1));

            startMapUri = G_ComposeMapUri(0, mapNumber > 0? mapNumber - 1 : mapNumber);
            autoStart = true;
        }
        else if(p < myargc - 2)
        {
            int episodeNumber = atoi(CommandLine_At(p + 1));
            int mapNumber     = atoi(CommandLine_At(p + 2));

            startMapUri = G_ComposeMapUri(episodeNumber > 0? episodeNumber - 1 : episodeNumber,
                                          mapNumber > 0? mapNumber - 1 : mapNumber);
            autoStart = true;
        }
    }

    if(!startMapUri)
    {
        startMapUri = G_ComposeMapUri(0, 0);
    }

    // Are we autostarting?
    if(autoStart)
    {
        App_Log(DE2_LOG_NOTE, "Autostart in Map %s, Skill %d",
                              F_PrettyPath(Str_Text(Uri_ToString(startMapUri))),
                              defaultGameRules.skill);
    }

    // Validate episode and map.
    AutoStr *path = Uri_Compose(startMapUri);
    if((autoStart || IS_NETGAME) && P_MapExists(Str_Text(path)))
    {
        G_SetGameActionNewSession(*startMapUri, 0/*default*/, defaultGameRules);
    }
    else
    {
        COMMON_GAMESESSION->endAndBeginTitle(); // Start up intro loop.
    }

    Uri_Delete(startMapUri);
}

void D_Shutdown()
{
    WI_Shutdown();
    G_CommonShutdown();
}
