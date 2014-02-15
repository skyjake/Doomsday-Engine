/** @file h_main.cpp  Heretic-specific game initialization.
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

#include "jheretic.h"

#include "d_netsv.h"
#include "m_argv.h"
#include "p_map.h"
#include "p_saveg.h"
#include "hereticv13gamestatereader.h"
#include "am_map.h"
#include "g_defs.h"
#include "p_inventory.h"
#include <assert.h>
#include <string.h>

int verbose;

//dd_bool devParm; // checkparm of -devparm
//dd_bool noMonstersParm; // checkparm of -nomonsters
//dd_bool respawnParm; // checkparm of -respawn
//dd_bool fastParm; // checkparm of -fast
//dd_bool turboParm; // checkparm of -turbo
//dd_bool randomClassParm; // checkparm of -randclass

float turboMul; // Multiplier for turbo.

gamemode_t gameMode;
int gameModeBits;

// Default font colours.
float const defFontRGB[]  = { .425f, .986f, .378f };
float const defFontRGB2[] = { 1, .65f, .275f };
float const defFontRGB3[] = { 1.0f, 1.0f, 1.0f };

// The patches used in drawing the view border.
// Percent-encoded.
char* borderGraphics[] = {
    "Flats:FLAT513", // Background.
    "BORDT", // Top.
    "BORDR", // Right.
    "BORDB", // Bottom.
    "BORDL", // Left.
    "BORDTL", // Top left.
    "BORDTR", // Top right.
    "BORDBR", // Bottom right.
    "BORDBL" // Bottom left.
};

/**
 * Get a 32-bit integer value.
 */
int H_GetInteger(int id)
{
    return Common_GetInteger(id);
}

/**
 * Get a pointer to the value of a variable. Added for 64-bit support.
 */
void* H_GetVariable(int id)
{
    static float bob[2];

    switch(id)
    {
    case DD_PLUGIN_NAME:
        return PLUGIN_NAMETEXT;

    case DD_PLUGIN_NICENAME:
        return PLUGIN_NICENAME;

    case DD_PLUGIN_VERSION_SHORT:
        return PLUGIN_VERSION_TEXT;

    case DD_PLUGIN_VERSION_LONG:
        return PLUGIN_VERSION_TEXTLONG "\n" PLUGIN_DETAILS;

    case DD_PLUGIN_HOMEURL:
        return PLUGIN_HOMEURL;

    case DD_PLUGIN_DOCSURL:
        return PLUGIN_DOCSURL;

    case DD_GAME_CONFIG:
        return gameConfigString;

    case DD_ACTION_LINK:
        return actionlinks;

    case DD_XGFUNC_LINK:
        return xgClasses;

    case DD_PSPRITE_BOB_X:
        R_GetWeaponBob(DISPLAYPLAYER, &bob[0], NULL);
        return &bob[0];

    case DD_PSPRITE_BOB_Y:
        R_GetWeaponBob(DISPLAYPLAYER, NULL, &bob[1]);
        return &bob[1];

    case DD_TM_FLOOR_Z:
        return (void*) &tmFloorZ;

    case DD_TM_CEILING_Z:
        return (void*) &tmCeilingZ;

    default:
        break;
    }

    // ID not recognized, return NULL.
    return 0;
}

/**
 * Pre Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void H_PreInit(void)
{
    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.povLookAround = true;
    cfg.statusbarScale = 1;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.echoMsg = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;
    cfg.menuPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.menuScale = .9f;
    cfg.menuTextGlitter = 0;
    cfg.menuShadow = 0;
  //cfg.menuQuitSound = true;
    cfg.menuTextFlashColor[0] = .7f;
    cfg.menuTextFlashColor[1] = .9f;
    cfg.menuTextFlashColor[2] = 1;
    cfg.menuTextFlashSpeed = 4;
    cfg.menuCursorRotate = false;

    cfg.inludePatchReplaceMode = PRM_ALLOW_TEXT;

    cfg.hudPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_READYITEM] = true;
    cfg.hudShown[HUD_LOG] = true;
    { int i;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
    }
    cfg.hudScale = .7f;
    cfg.hudColor[0] = .325f;
    cfg.hudColor[1] = .686f;
    cfg.hudColor[2] = .278f;
    cfg.hudColor[3] = 1;
    cfg.hudIconAlpha = 1;
    cfg.xhairAngle = 0;
    cfg.xhairSize = .5f;
    cfg.xhairVitality = false;
    cfg.xhairColor[0] = 1;
    cfg.xhairColor[1] = 1;
    cfg.xhairColor[2] = 1;
    cfg.xhairColor[3] = 1;
    cfg.filterStrength = .8f;
  //cfg.snd_3D = false;
  //cfg.snd_ReverbFactor = 100;
    cfg.moveCheckZ = true;
    cfg.jumpPower = 9;
    cfg.airborneMovement = 1;
    cfg.weaponAutoSwitch = 1; // IF BETTER
    cfg.noWeaponAutoSwitchIfFiring = false;
    cfg.ammoAutoSwitch = 0; // Never.
    cfg.slidingCorpses = false;
    //cfg.fastMonsters = false;
    cfg.secretMsg = true;
    cfg.netJumping = true;
    cfg.netEpisode = 0;
    cfg.netMap = 0;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 4; // Use the default color by default.
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1; // Use map default.
    cfg.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.mapTitle = true;
    cfg.automapTitleAtBottom = true;
    cfg.hideIWADAuthor = true;
    cfg.menuTextColors[0][0] = defFontRGB[0];
    cfg.menuTextColors[0][1] = defFontRGB[1];
    cfg.menuTextColors[0][2] = defFontRGB[2];
    cfg.menuTextColors[1][0] = defFontRGB2[0];
    cfg.menuTextColors[1][1] = defFontRGB2[1];
    cfg.menuTextColors[1][2] = defFontRGB2[2];
    cfg.menuTextColors[2][0] = defFontRGB3[0];
    cfg.menuTextColors[2][1] = defFontRGB3[1];
    cfg.menuTextColors[2][2] = defFontRGB3[2];
    cfg.menuTextColors[3][0] = defFontRGB3[0];
    cfg.menuTextColors[3][1] = defFontRGB3[1];
    cfg.menuTextColors[3][2] = defFontRGB3[2];
    cfg.menuSlam = true;
    cfg.menuShortcutsEnabled = true;
    cfg.menuGameSaveSuggestName = true;

    cfg.confirmQuickGameSave = true;
    cfg.confirmRebornLoad = true;
    cfg.loadAutoSaveOnReborn = false;
    cfg.loadLastSaveOnReborn = false;

    cfg.monstersStuckInDoors = false;
    cfg.avoidDropoffs = true;
    cfg.moveBlock = false;
    cfg.fallOff = true;
    cfg.fixFloorFire = false;
    cfg.fixPlaneScrollMaterialsEastOnly = true;

    cfg.statusbarOpacity = 1;
    cfg.statusbarCounterAlpha = 1;

    cfg.automapCustomColors = 0; // Never.
    cfg.automapL0[0] = .455f; // Unseen areas.
    cfg.automapL0[1] = .482f;
    cfg.automapL0[2] = .439f;

    cfg.automapL1[0] = .292f; // onesided lines
    cfg.automapL1[1] = .195f;
    cfg.automapL1[2] = .062f;

    cfg.automapL2[0] = .812f; // floor height change lines
    cfg.automapL2[1] = .687f;
    cfg.automapL2[2] = .519f;

    cfg.automapL3[0] = .402f; // ceiling change lines
    cfg.automapL3[1] = .230f;
    cfg.automapL3[2] = .121f;

    cfg.automapMobj[0] = .093f;
    cfg.automapMobj[1] = .093f;
    cfg.automapMobj[2] = .093f;

    cfg.automapBack[0] = 1.0f;
    cfg.automapBack[1] = 1.0f;
    cfg.automapBack[2] = 1.0f;
    cfg.automapOpacity = 1.0f;
    cfg.automapLineAlpha = 1.0f;
    cfg.automapLineWidth = 1.1f;
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = true;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.automapOpenSeconds = AUTOMAP_OPEN_SECONDS;

    cfg.hudCheatCounterScale = .7f;
    cfg.hudCheatCounterShowWithAutomap = true;

    cfg.msgCount = 4;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5;
    cfg.msgAlign = 1; // Center.
    cfg.msgBlink = 5;

    cfg.msgColor[0] = defFontRGB3[0];
    cfg.msgColor[1] = defFontRGB3[1];
    cfg.msgColor[2] = defFontRGB3[2];

    cfg.inventoryTimer = 5;
    cfg.inventoryWrap = false;
    cfg.inventoryUseNext = true;
    cfg.inventoryUseImmediate = false;
    cfg.inventorySlotMaxVis = 7;
    cfg.inventorySlotShowEmpty = true;
    cfg.inventorySelectMode = 0; // Cursor select.

    cfg.chatBeep = true;

  //cfg.killMessages = true;
    cfg.bobView = 1;
    cfg.bobWeapon = 1;
    cfg.bobWeaponLower = true;
    cfg.cameraNoClip = true;
    cfg.respawnMonstersNightmare = false;

    cfg.weaponOrder[0] = WT_SEVENTH;    // mace \ beak
    cfg.weaponOrder[1] = WT_SIXTH;      // phoenixrod \ beak
    cfg.weaponOrder[2] = WT_FIFTH;      // skullrod \ beak
    cfg.weaponOrder[3] = WT_FOURTH;     // blaster \ beak
    cfg.weaponOrder[4] = WT_THIRD;      // crossbow \ beak
    cfg.weaponOrder[5] = WT_SECOND;     // goldwand \ beak
    cfg.weaponOrder[6] = WT_EIGHTH;     // gauntlets \ beak
    cfg.weaponOrder[7] = WT_FIRST;      // staff \ beak

    cfg.weaponCycleSequential = true;

    cfg.menuEffectFlags = MEF_TEXT_SHADOW;
    cfg.hudFog = 5;

    cfg.ringFilter = 1;
    cfg.tomeCounter = 10;
    cfg.tomeSound = 3;

    // Use the crossfade transition by default.
    Con_SetInteger("con-transition", 0);

    // Heretic's torch light does not attenuate with distance.
    DD_SetInteger(DD_FIXEDCOLORMAP_ATTENUATE, 0);

    // Do the common pre init routine;
    G_CommonPreInit();

    // Declare the Heretic V13 game state reader/interpreter.
    SV_DeclareGameStateReader(&HereticV13GameStateReader::recognize, &HereticV13GameStateReader::make);
}

/**
 * Post Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void H_PostInit(void)
{
    dd_bool autoStart = false;
    Uri *startMapUri = 0;
    AutoStr *path;
    int p;

    /// @todo Kludge: Shareware WAD has different border background.
    /// @todo Do this properly!
    if(gameMode == heretic_shareware)
        borderGraphics[0] = "Flats:FLOOR04";
    else
        borderGraphics[0] = "Flats:FLAT513";

    // Common post init routine.
    G_CommonPostInit();

    // Initialize weapon info using definitions.
    P_InitWeaponInfo();

    // Game parameters.
    monsterInfight = GetDefInt("AI|Infight", 0);

    // Defaults for skill, episode and map.
    gameRules.skill = /*startSkill =*/ SM_MEDIUM;

    // Game mode specific settings.
    /* None */

    if(CommandLine_Check("-deathmatch"))
    {
        cfg.netDeathmatch = true;
    }

    // Apply these game rules.
    gameRules.noMonsters      = CommandLine_Exists("-nomonsters")? true : false;
    gameRules.respawnMonsters = CommandLine_Check("-respawn")? true : false;

    // turbo option.
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
        int const slotNumber = SaveSlots_ParseSlotIdentifier(saveSlots, CommandLine_At(p + 1));
        if(SaveSlots_SlotIsUserWritable(saveSlots, slotNumber) && G_LoadGame(slotNumber))
        {
            // No further initialization is to be done.
            return;
        }
    }

    p = CommandLine_Check("-skill");
    if(p && p < myargc - 1)
    {
        int skillNumber = atoi(CommandLine_At(p + 1));
        gameRules.skill = (skillmode_t)(skillNumber > 0? skillNumber - 1 : skillNumber);
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
    if(p && p < myargc - 2)
    {
        int episodeNumber = atoi(CommandLine_At(p + 1));
        int mapNumber     = atoi(CommandLine_At(p + 2));

        startMapUri = G_ComposeMapUri(episodeNumber > 0? episodeNumber - 1 : episodeNumber,
                                      mapNumber > 0? mapNumber - 1 : mapNumber);
        autoStart = true;
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
                              gameRules.skill);
    }

    // Validate episode and map.
    path = Uri_Compose(startMapUri);
    if((autoStart || IS_NETGAME) && P_MapExists(Str_Text(path)))
    {
        G_DeferredNewGame(startMapUri, 0/*default*/, &gameRules);
    }
    else
    {
        G_StartTitle(); // Start up intro loop.
    }

    Uri_Delete(startMapUri);
}

void H_Shutdown(void)
{
    P_ShutdownInventory();
    G_CommonShutdown();
}
