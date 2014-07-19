/** @file h_console.cpp  Heretic specific console settings and commands.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
#include "hu_menu.h"
#include "hu_stuff.h"
#include "p_inventory.h"

using namespace common;

D_CMD(Cheat);
D_CMD(CheatGive);
D_CMD(CheatGod);
D_CMD(CheatLeaveMap);
D_CMD(CheatMassacre);
D_CMD(CheatMorph);
D_CMD(CheatNoClip);
D_CMD(CheatReveal);
D_CMD(CycleSpy);
D_CMD(CheatSuicide);
D_CMD(CheatWhere);
D_CMD(MakeLocal);
D_CMD(PlayDemo);
D_CMD(PrintPlayerCoords);
D_CMD(RecordDemo);
D_CMD(SetCamera);
D_CMD(SetViewLock);
D_CMD(SetViewMode);
D_CMD(SpawnMobj);
D_CMD(StopDemo);

/**
 * Called when the player-eyeheight cvar is changed.
 */
static void updateEyeHeight()
{
    player_t *plr = &players[CONSOLEPLAYER];
    if(!(plr->plr->flags & DDPF_CAMERA))
    {
        plr->viewHeight = (float) cfg.plrViewHeight;
    }
}

D_CMD(ScreenShot)
{
    DENG2_UNUSED3(src, argc, argv);
    G_SetGameAction(GA_SCREENSHOT);
    return true;
}

static void viewResizeAudioFeedback()
{
    if(Hu_MenuIsActive())
    {
        // The menu slider plays its own audio feedback.
        return;
    }
    S_LocalSound(SFX_KEYUP, 0);
}

void G_ConsoleRegistration()
{
    // View/Refresh
    C_VAR_INT2 ("view-size",                            &cfg.setBlocks,                     0, 3, 13, viewResizeAudioFeedback);
    C_VAR_BYTE ("hud-title",                            &cfg.mapTitle,                      0, 0, 1);
    C_VAR_BYTE ("hud-title-author-noiwad",              &cfg.hideIWADAuthor,                0, 0, 1);

    C_VAR_FLOAT("view-bob-height",                      &cfg.bobView,                       0, 0, 1);
    C_VAR_FLOAT("view-bob-weapon",                      &cfg.bobWeapon,                     0, 0, 1);
    C_VAR_BYTE ("view-bob-weapon-switch-lower",         &cfg.bobWeaponLower,                0, 0, 1);
    C_VAR_FLOAT("view-filter-strength",                 &cfg.filterStrength,                0, 0, 1);
    C_VAR_INT  ("view-ringfilter",                      &cfg.ringFilter,                    0, 1, 2);

    // Server-side options
    // Game state
    C_VAR_BYTE ("server-game-skill",                    &cfg.netSkill,                      0, 0, 4);
    C_VAR_BYTE ("server-game-map",                      &cfg.netMap,                        CVF_NO_MAX, 0, 0);
    C_VAR_BYTE ("server-game-episode",                  &cfg.netEpisode,                    CVF_NO_MAX, 0, 0);
    C_VAR_BYTE ("server-game-deathmatch",               &cfg.netDeathmatch,                 0, 0, 1); /* jHeretic only has one deathmatch mode */

    // Modifiers
    C_VAR_BYTE ("server-game-mod-damage",               &cfg.netMobDamageModifier,          0, 1, 100);
    C_VAR_BYTE ("server-game-mod-health",               &cfg.netMobHealthModifier,          0, 1, 20);
    C_VAR_INT  ("server-game-mod-gravity",              &cfg.netGravity,                    0, -1, 100);

    // Gameplay options
    C_VAR_BYTE ("server-game-jump",                     &cfg.netJumping,                    0, 0, 1);
    C_VAR_BYTE ("server-game-nomonsters",               &cfg.netNoMonsters,                 0, 0, 1);
    C_VAR_BYTE ("server-game-respawn",                  &cfg.netRespawn,                    0, 0, 1);
    C_VAR_BYTE ("server-game-respawn-monsters-nightmare", &cfg.respawnMonstersNightmare,    0, 0, 1);
    C_VAR_BYTE ("server-game-radiusattack-nomaxz",      &cfg.netNoMaxZRadiusAttack,         0, 0, 1);
    C_VAR_BYTE ("server-game-monster-meleeattack-nomaxz", &cfg.netNoMaxZMonsterMeleeAttack, 0, 0, 1);

    C_VAR_BYTE ("server-game-coop-nodamage",            &cfg.noCoopDamage,                  0, 0, 1);
    C_VAR_BYTE ("server-game-noteamdamage",             &cfg.noTeamDamage,                  0, 0, 1);

    // Misc
    C_VAR_BYTE ("server-game-announce-secret",          &cfg.secretMsg,                     0, 0, 1);

    // Player
    // Player data
    C_VAR_BYTE ("player-color",                         &cfg.netColor,                      0, 0, 4);
    C_VAR_INT2 ("player-eyeheight",                     &cfg.plrViewHeight,                 0, 41, 54, updateEyeHeight);

    // Movment
    C_VAR_FLOAT("player-move-speed",                    &cfg.playerMoveSpeed,               0, 0, 1);
    C_VAR_INT  ("player-jump",                          &cfg.jumpEnabled,                   0, 0, 1);
    C_VAR_FLOAT("player-jump-power",                    &cfg.jumpPower,                     0, 0, 100);
    C_VAR_BYTE ("player-air-movement",                  &cfg.airborneMovement,              0, 0, 32);

    // Weapon switch preferences
    C_VAR_BYTE ("player-autoswitch",                    &cfg.weaponAutoSwitch,              0, 0, 2);
    C_VAR_BYTE ("player-autoswitch-ammo",               &cfg.ammoAutoSwitch,                0, 0, 2);
    C_VAR_BYTE ("player-autoswitch-notfiring",          &cfg.noWeaponAutoSwitchIfFiring,    0, 0, 1);

    // Weapon Order preferences
    C_VAR_INT  ("player-weapon-order0",                 &cfg.weaponOrder[0],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order1",                 &cfg.weaponOrder[1],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order2",                 &cfg.weaponOrder[2],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order3",                 &cfg.weaponOrder[3],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order4",                 &cfg.weaponOrder[4],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order5",                 &cfg.weaponOrder[5],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order6",                 &cfg.weaponOrder[6],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order7",                 &cfg.weaponOrder[7],                0, 0, NUM_WEAPON_TYPES);

    C_VAR_BYTE ("player-weapon-nextmode",               &cfg.weaponNextMode,                0, 0, 1);
    C_VAR_BYTE ("player-weapon-cycle-sequential",       &cfg.weaponCycleSequential,         0, 0, 1);

    // Misc
    C_VAR_INT  ("player-camera-noclip",                 &cfg.cameraNoClip,                  0, 0, 1);

    // Compatibility options
    C_VAR_BYTE ("game-monsters-stuckindoors",           &cfg.monstersStuckInDoors,          0, 0, 1);
    C_VAR_BYTE ("game-objects-neverhangoverledges",     &cfg.avoidDropoffs,                 0, 0, 1);
    C_VAR_BYTE ("game-objects-clipping",                &cfg.moveBlock,                     0, 0, 1);
    C_VAR_BYTE ("game-player-wallrun-northonly",        &cfg.wallRunNorthOnly,              0, 0, 1);
    C_VAR_BYTE ("game-objects-falloff",                 &cfg.fallOff,                       0, 0, 1);
    C_VAR_BYTE ("game-zclip",                           &cfg.moveCheckZ,                    0, 0, 1);
    C_VAR_BYTE ("game-monsters-floatoverblocking",      &cfg.allowMonsterFloatOverBlocking, 0, 0, 1);
    C_VAR_BYTE ("game-corpse-sliding",                  &cfg.slidingCorpses,                0, 0, 1);
    C_VAR_BYTE ("server-game-maulotaur-fixfloorfire",   &cfg.fixFloorFire,                  0, 0, 1);
    C_VAR_BYTE ("server-game-plane-fixmaterialscroll",  &cfg.fixPlaneScrollMaterialsEastOnly, 0, 0, 1);

    // Game state
    //C_VAR_BYTE ("game-fastmonsters",                    &cfg.fastMonsters,                  0, 0, 1);

    // Gameplay
    C_VAR_INT  ("game-corpse-time",                     &cfg.corpseTime,                    CVF_NO_MAX, 0, 0);

    // Misc
    C_VAR_BYTE ("msg-echo",                             &cfg.echoMsg,                       0, 0, 1);


    C_CMD("spy",        "",      CycleSpy);
    C_CMD("screenshot", "",      ScreenShot);

    C_CMD("cheat",       "s",    Cheat);
    C_CMD("god",         NULL,   CheatGod);
    C_CMD("noclip",      NULL,   CheatNoClip);
    C_CMD("reveal",      "i",    CheatReveal);
    C_CMD("give",        NULL,   CheatGive);
    C_CMD("kill",        "",     CheatMassacre);
    C_CMD("leavemap",    "",     CheatLeaveMap);
    C_CMD("suicide",     NULL,   CheatSuicide);
    C_CMD("where",       "",     CheatWhere);

    C_CMD("spawnmobj",   NULL,   SpawnMobj);
    C_CMD("coord",       "",     PrintPlayerCoords);

    C_CMD("makelocp",    "i",    MakeLocal);
    C_CMD("makecam",     "i",    SetCamera);
    C_CMD("setlock",     NULL,   SetViewLock);
    C_CMD("lockmode",    "i",    SetViewLock);
    C_CMD("viewmode",    NULL,   SetViewMode);

    C_CMD("chicken",     NULL,   CheatMorph);
}
