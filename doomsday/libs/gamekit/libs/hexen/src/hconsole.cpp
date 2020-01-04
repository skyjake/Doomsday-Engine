/** @file hconsole.cpp  Hexen specific console settings and commands.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "jhexen.h"
#include "d_net.h"
#include "hu_menu.h"
#include "hu_stuff.h"
#include "g_common.h"
#include "g_controls.h"
#include "p_inventory.h"

using namespace common;

D_CMD(Cheat);
D_CMD(CheatGive);
D_CMD(CheatGod);
D_CMD(CheatNoClip);
D_CMD(CheatMassacre);
D_CMD(CheatMorph);
D_CMD(CheatShadowcaster);
D_CMD(CheatReveal);
D_CMD(CheatRunScript);
D_CMD(CheatSuicide);
D_CMD(CheatWhere);
D_CMD(CycleSpy);
D_CMD(MakeLocal);
D_CMD(MovePlane);
D_CMD(PrintPlayerCoords);
D_CMD(SetCamera);
D_CMD(SetDemoMode);
D_CMD(SetViewLock);
D_CMD(SetViewMode);
D_CMD(SpawnMobj);

static void updateEyeHeight()
{
    player_t *plr = &players[CONSOLEPLAYER];
    if(!(plr->plr->flags & DDPF_CAMERA))
    {
        plr->viewHeight = (float) cfg.common.plrViewHeight;
    }
}

D_CMD(ScreenShot)
{
    DE_UNUSED(src, argc, argv);
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
    S_LocalSound(SFX_PICKUP_KEY, 0);
}

void G_ConsoleRegistration()
{
    Common_Register();

    // View/Refresh
    C_VAR_INT2 ("view-size",                        &cfg.common.setBlocks,                     0, 3, 13, viewResizeAudioFeedback);
    C_VAR_BYTE ("hud-title",                        &cfg.common.mapTitle,                      0, 0, 1);
    C_VAR_BYTE ("hud-title-author-noiwad",          &cfg.common.hideIWADAuthor,                0, 0, 1);

    C_VAR_FLOAT("view-bob-height",                  &cfg.common.bobView,                       0, 0, 1);
    C_VAR_FLOAT("view-bob-weapon",                  &cfg.common.bobWeapon,                     0, 0, 1);
    C_VAR_FLOAT("view-filter-strength",             &cfg.common.filterStrength,                0, 0, 1);

    // Misc
    C_VAR_BYTE ("msg-hub-override",                 &cfg.overrideHubMsg,                0, 0, 2);

    // Player
    // Player data
    C_VAR_BYTE ("player-color",                     &cfg.common.netColor,                      0, 0, 8);
    C_VAR_INT2 ("player-eyeheight",                 &cfg.common.plrViewHeight,                 0, 41, 54, updateEyeHeight);
    C_VAR_BYTE ("player-class",                     &cfg.netClass,                      0, 0, 2);

    // Weapon switch preferences
    C_VAR_BYTE ("player-autoswitch",                &cfg.common.weaponAutoSwitch,              0, 0, 2);
    C_VAR_BYTE ("player-autoswitch-ammo",           &cfg.common.ammoAutoSwitch,                0, 0, 2);
    C_VAR_BYTE ("player-autoswitch-notfiring",      &cfg.common.noWeaponAutoSwitchIfFiring,    0, 0, 1);

    // Weapon Order preferences
    C_VAR_INT  ("player-weapon-order0",             &cfg.common.weaponOrder[0],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order1",             &cfg.common.weaponOrder[1],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order2",             &cfg.common.weaponOrder[2],                0, 0, NUM_WEAPON_TYPES);
    C_VAR_INT  ("player-weapon-order3",             &cfg.common.weaponOrder[3],                0, 0, NUM_WEAPON_TYPES);

    C_VAR_BYTE ("player-weapon-nextmode",           &cfg.common.weaponNextMode,                0, 0, 1);
    C_VAR_BYTE ("player-weapon-cycle-sequential",   &cfg.common.weaponCycleSequential,         0, 0, 1);

    // Misc
    C_VAR_INT  ("player-camera-noclip",             &cfg.common.cameraNoClip,                  0, 0, 1);

    // Compatibility options
    C_VAR_INT  ("game-icecorpse",                   &cfg.translucentIceCorpse,          0, 0, 1);

    // Gameplay
    C_VAR_INT  ("game-maulator-time",               &maulatorSeconds,                   CVF_NO_MAX, 1, 0);
    C_VAR_BYTE ("game-deathkings-respawn-chance",   &cfg.deathkingsAutoRespawnChance,   0, 0, 100);

    // Misc
    C_VAR_BYTE ("msg-echo",                         &cfg.common.echoMsg,                       0, 0, 1);


    C_CMD("spy",         "",     CycleSpy);
    C_CMD("screenshot",  "",     ScreenShot);

    C_CMD("cheat",       "s",    Cheat);
    C_CMD("god",         NULL,   CheatGod);
    C_CMD("noclip",      NULL,   CheatNoClip);
    C_CMD("reveal",      "i",    CheatReveal);
    C_CMD("give",        NULL,   CheatGive);
    C_CMD("kill",        "",     CheatMassacre);
    C_CMD("suicide",     NULL,   CheatSuicide);
    C_CMD("where",       "",     CheatWhere);

    C_CMD("spawnmobj",   NULL,   SpawnMobj);
    C_CMD("coord",       "",     PrintPlayerCoords);

    C_CMD("makelocp",    "i",    MakeLocal);
    C_CMD("makecam",     "i",    SetCamera);
    C_CMD("setlock",     NULL,   SetViewLock);
    C_CMD("lockmode",    "i",    SetViewLock);
    C_CMD("viewmode",    NULL,   SetViewMode);

    C_CMD("pig",         NULL,   CheatMorph);
    C_CMD("runscript",   "i*",   CheatRunScript);
    C_CMD("class",       "i*",   CheatShadowcaster);
}
