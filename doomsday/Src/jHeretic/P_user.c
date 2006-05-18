/*
 * Player related stuff.
 * Bobbing POV/weapon, movement.
 * Pending weapon.
 */

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/Doomdef.h"
#include "jHeretic/h_config.h"
#include "jHeretic/h_event.h"
#include "jHeretic/P_local.h"
#include "jHeretic/h_stat.h"
#include "jHeretic/Sounds.h"

#include "Common/p_view.h"
#include "Common/r_common.h"
#include "Common/d_net.h"
#include "Common/p_player.h"

// MACROS ------------------------------------------------------------------

// 16 pixels of bob
#define MAXBOB  0x100000

#define ANG5    (ANG90/18)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    P_PlayerNextArtifact(player_t *player);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean onground;
int     newtorch;               // used in the torch flicker effect.
int     newtorchdelta;

classinfo_t classInfo[NUMCLASSES] = {
    {   // Player
        S_PLAY,
        S_PLAY_RUN1,
        S_PLAY_ATK1,
        S_PLAY_ATK2,
        20,
        0x3C,
        {0x19, 0x32},
        {0x18, 0x28},
    },
    {   // Chicken
        S_CHICPLAY,
        S_CHICPLAY_RUN1,
        S_CHICPLAY_ATK1,
        S_CHICPLAY_ATK1,
        20,
        0x3C,
        {0x19, 0x32},
        {0x18, 0x28},
    },
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Moves the given origin along a given angle.
 */
void P_Thrust(player_t *player, angle_t angle, fixed_t move)
{
    mobj_t *plrmo = player->plr->mo;
    sector_t* sector = P_GetPtrp(plrmo->subsector, DMU_SECTOR);

    angle >>= ANGLETOFINESHIFT;
    if(player->powers[pw_flight] && !(plrmo->pos[VZ] <= plrmo->floorz))
    {
        plrmo->momx += FixedMul(move, finecosine[angle]);
        plrmo->momy += FixedMul(move, finesine[angle]);
    }
    else if(P_XSector(sector)->special == 15)    // Friction_Low
    {
        plrmo->momx += FixedMul(move >> 2, finecosine[angle]);
        plrmo->momy += FixedMul(move >> 2, finesine[angle]);
    }
    else
    {
        int     mul = XS_ThrustMul(sector);

        if(mul != FRACUNIT)
            move = FixedMul(move, mul);
        plrmo->momx += FixedMul(move, finecosine[angle]);
        plrmo->momy += FixedMul(move, finesine[angle]);
    }
}

/*
 * Returns true if the player is currently standing on ground
 * or on top of another mobj.
 */
boolean P_IsPlayerOnGround(player_t *player)
{
    boolean onground = (player->plr->mo->pos[VZ] <= player->plr->mo->floorz);

    if(player->plr->mo->onmobj && !onground && !(player->plr->mo->flags2 & MF2_FLY))
    {
        mobj_t *on = player->plr->mo->onmobj;

        onground = (player->plr->mo->pos[VZ] <= on->pos[VZ] + on->height);
    }
    return onground;
}

/*
 * Will make the player jump if the latest command so instructs,
 * providing that jumping is possible.
 */
void P_CheckPlayerJump(player_t *player)
{
    ticcmd_t *cmd = &player->cmd;

    if(cfg.jumpEnabled && (!IS_CLIENT || netJumpPower > 0) &&
       P_IsPlayerOnGround(player) && cmd->jump &&
       player->jumptics <= 0)
    {
        // Jump, then!
        player->plr->mo->momz =
            FRACUNIT * (IS_CLIENT ? netJumpPower : cfg.jumpPower);
        player->jumptics = 24;
    }
}

void P_MovePlayer(player_t *player)
{
    int     fly;
    mobj_t *plrmo = player->plr->mo;
    ticcmd_t *cmd;

    cmd = &player->cmd;

    // Change the angle if possible.
    if(!(player->plr->flags & DDPF_FIXANGLES))
    {
        plrmo->angle = cmd->angle << 16;
        player->plr->lookdir = cmd->pitch / (float) DDMAXSHORT *110;
    }

    // Do not let the player control movement if not onground.
    onground =  P_IsPlayerOnGround(player);
    if(player->plr->flags & DDPF_CAMERA)    // $democam
    {
        // Cameramen have a 3D thrusters!
        P_Thrust3D(player, player->plr->mo->angle, player->plr->lookdir,
                   cmd->forwardMove * 2048, cmd->sideMove * 2048);
        return;
    }
    else
    {
#if __JHERETIC__
        if(player->morphTics)
        {                           // Chicken speed
            if(cmd->forwardMove && (onground || plrmo->flags2 & MF2_FLY))
                P_Thrust(player, plrmo->angle, cmd->forwardMove * 2500);
            if(cmd->sideMove && (onground || plrmo->flags2 & MF2_FLY))
                P_Thrust(player, plrmo->angle - ANG90, cmd->sideMove * 2500);
        }
        else
#endif
        {
            // 'Move while in air' hack (server doesn't know about this!!).
            // Movement while in air traditionally disabled.
            int     movemul = (onground || plrmo->flags2 & MF2_FLY) ? 2048 :
                        cfg.airborneMovement ? cfg.airborneMovement * 64 : 0;

            if(cmd->forwardMove && movemul)
                P_Thrust(player, plrmo->angle, cmd->forwardMove * movemul);

            if(cmd->sideMove && movemul)
                P_Thrust(player, plrmo->angle - ANG90, cmd->sideMove * movemul);
        }

        if((cmd->forwardMove || cmd->sideMove) &&
           player->plr->mo->state == &states[PCLASS_INFO(player->class)->normalstate])
        {
            P_SetMobjState(plrmo, PCLASS_INFO(player->class)->runstate);
        }

        fly = cmd->fly; //lookfly >> 4;
    /*  if(fly > 7)
            fly -= 16;
    */
        if(fly && player->powers[pw_flight])
        {
            if(fly != TOCENTER)
            {
                player->flyheight = fly * 2;
                if(!(plrmo->flags2 & MF2_FLY))
                {
                    plrmo->flags2 |= MF2_FLY;
                    plrmo->flags |= MF_NOGRAVITY;
                }
            }
            else
            {
                plrmo->flags2 &= ~MF2_FLY;
                plrmo->flags &= ~MF_NOGRAVITY;
            }
        }
        else if(fly > 0)
        {
            P_PlayerUseArtifact(player, arti_fly);
        }
        if(plrmo->flags2 & MF2_FLY)
        {
            plrmo->momz = player->flyheight * FRACUNIT;
            if(player->flyheight)
            {
                player->flyheight /= 2;
            }
        }

        P_CheckPlayerJump(player);
    }
}


void P_DeathThink(player_t *player)
{
    angle_t angle, delta;
    extern int inv_ptr;
    extern int curpos;
    int     lookDelta;
    mobj_t *plrmo = player->plr->mo;

    P_MovePsprites(player);

    onground = P_IsPlayerOnGround(player);
    if(plrmo->type == MT_BLOODYSKULL)
    {                           // Flying bloody skull
        player->plr->viewheight = 6 * FRACUNIT;
        player->plr->deltaviewheight = 0;
        //player->damagecount = 20;
        if(onground)
        {
            if(player->plr->lookdir < 60)
            {
                lookDelta = (60 - player->plr->lookdir) / 8;
                if(lookDelta < 1 && (leveltime & 1))
                {
                    lookDelta = 1;
                }
                else if(lookDelta > 6)
                {
                    lookDelta = 6;
                }
                player->plr->lookdir += lookDelta;
            }
        }
    }
    else
    {                           // Fall to ground
        player->plr->deltaviewheight = 0;
        if(player->plr->viewheight > 6 * FRACUNIT)
            player->plr->viewheight -= FRACUNIT;
        if(player->plr->viewheight < 6 * FRACUNIT)
            player->plr->viewheight = 6 * FRACUNIT;
        if(player->plr->lookdir > 0)
        {
            player->plr->lookdir -= 6;
        }
        else if(player->plr->lookdir < 0)
        {
            player->plr->lookdir += 6;
        }
        if(abs(player->plr->lookdir) < 6)
        {
            player->plr->lookdir = 0;
        }
    }

    player->plr->flags |= DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
    P_CalcHeight(player);

    if(!IS_NETGAME && player->attacker && player->attacker != plrmo)
    {
        angle =
            R_PointToAngle2(plrmo->pos[VX], plrmo->pos[VY],
                            player->attacker->pos[VX], player->attacker->pos[VY]);
        delta = angle - plrmo->angle;
        if(delta < ANG5 || delta > (unsigned) -ANG5)
        {                       // Looking at killer, so fade damage flash down
            plrmo->angle = angle;
            if(player->damagecount)
            {
                player->damagecount--;
            }
        }
        else if(delta < ANG180)
            plrmo->angle += ANG5;
        else
            plrmo->angle -= ANG5;
    }
    else if(player->damagecount)
    {
        player->damagecount--;
    }

    if(player->cmd.use)
    {
        if(player == &players[consoleplayer])
        {
            //          I_SetPalette((byte *)W_CacheLumpName("PLAYPAL", PU_CACHE));
            H_SetFilter(0);
            inv_ptr = 0;
            curpos = 0;
            newtorch = 0;
            newtorchdelta = 0;
        }
        player->playerstate = PST_REBORN;
        // Let the mobj know the player has entered the reborn state.  Some
        // mobjs need to know when it's ok to remove themselves.
        plrmo->special2 = 666;
    }
}

void P_MorphPlayerThink(player_t *player)
{
    mobj_t *pmo;

    if(player->health > 0)
    {                           // Handle beak movement
        P_UpdateBeak(player, &player->psprites[ps_weapon]);
    }
    if(IS_CLIENT || player->morphTics & 15)
    {
        return;
    }
    pmo = player->plr->mo;
    if(!(pmo->momx + pmo->momy) && P_Random() < 160)
    {                           // Twitch view angle
        pmo->angle += (P_Random() - P_Random()) << 19;
    }
    if((pmo->pos[VZ] <= pmo->floorz) && (P_Random() < 32))
    {                           // Jump and noise
        pmo->momz += FRACUNIT;
        P_SetMobjState(pmo, S_CHICPLAY_PAIN);
        return;
    }
    if(P_Random() < 48)
    {                           // Just noise
        S_StartSound(sfx_chicact, pmo);
    }
}

boolean P_UndoPlayerMorph(player_t *player)
{
    mobj_t *fog;
    mobj_t *mo;
    mobj_t *pmo;
    fixed_t pos[3];
    angle_t angle;
    int     playerNum;
    weapontype_t weapon;
    int     oldFlags;
    int     oldFlags2;

    player->update |= PSF_MORPH_TIME | PSF_POWERS | PSF_HEALTH;

    pmo = player->plr->mo;
    memcpy(pos, pmo->pos, sizeof(pos));
    angle = pmo->angle;
    weapon = pmo->special1;
    oldFlags = pmo->flags;
    oldFlags2 = pmo->flags2;
    P_SetMobjState(pmo, S_FREETARGMOBJ);
    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PLAYER);
    if(P_TestMobjLocation(mo) == false)
    {                           // Didn't fit
        P_RemoveMobj(mo);
        mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_CHICPLAYER);
        mo->angle = angle;
        mo->health = player->health;
        mo->special1 = weapon;
        mo->player = player;
        mo->dplayer = player->plr;
        mo->flags = oldFlags;
        mo->flags2 = oldFlags2;
        player->plr->mo = mo;
        player->morphTics = 2 * 35;
        return (false);
    }
    playerNum = P_GetPlayerNum(player);
    if(playerNum != 0)
    {                           // Set color translation
        mo->flags |= playerNum << MF_TRANSSHIFT;
    }
    mo->angle = angle;
    mo->player = player;
    mo->dplayer = player->plr;
    mo->reactiontime = 18;
    if(oldFlags2 & MF2_FLY)
    {
        mo->flags2 |= MF2_FLY;
        mo->flags |= MF_NOGRAVITY;
    }
    player->morphTics = 0;
    player->powers[pw_weaponlevel2] = 0;
    player->health = mo->health = MAXHEALTH;
    player->plr->mo = mo;
    player->class = PCLASS_PLAYER;
    player->plr->flags |= DDPF_FIXPOS | DDPF_FIXMOM;
    player->update |= PSF_MORPH_TIME | PSF_HEALTH;
    angle >>= ANGLETOFINESHIFT;
    fog =
        P_SpawnMobj(pos[VX] + 20 * finecosine[angle], pos[VY] + 20 * finesine[angle],
                    pos[VZ] + TELEFOGHEIGHT, MT_TFOG);
    S_StartSound(sfx_telept, fog);
    P_PostMorphWeapon(player, weapon);
    return (true);
}

/*
 * Called once per tick by P_Ticker.
 * This routine does all the thinking for the console player during
 * netgames.
 */
void P_ClientSideThink()
{
    int     i;
    player_t *pl;
    ddplayer_t *dpl;
    mobj_t *mo;
    ticcmd_t *cmd;
    int fly;
#if __JHERETIC__
    sector_t* sector;
#endif

    if(!IS_CLIENT || !Get(DD_GAME_READY))
        return;

    pl = &players[consoleplayer];
    dpl = pl->plr;
    mo = dpl->mo;
    cmd = &pl->cmd; // The latest local command.
    P_CalcHeight(pl);

    if(pl->morphTics)
        P_MorphPlayerThink(pl);

    // Message timer.
    pl->messageTics--;          // Can go negative
    if(!pl->messageTics)
    {                           // Refresh the screen when a message goes away
        //BorderTopRefresh = true;
        GL_Update(DDUF_TOP);
    }

    // Powers tic away.
    for(i = 0; i < NUMPOWERS; i++)
    {
        switch (i)
        {
        case pw_invulnerability:
        case pw_weaponlevel2:
        case pw_invisibility:
        case pw_flight:
        case pw_infrared:
            if(pl->powers[i] > 0)
                pl->powers[i]--;
            else
                pl->powers[i] = 0;
            break;
        }
    }
    if(pl->morphTics > 0)
    {
        pl->morphTics--;
        if(!pl->morphTics)    // Chic mode ends?
            pl->psprites[ps_weapon].sy = WEAPONBOTTOM;
    }
    if(pl->chickenPeck > 0)
        pl->chickenPeck--;

    // Are we dead?
    if(pl->playerstate == PST_DEAD)
    {
        if(dpl->viewheight > 6 * FRACUNIT)
            dpl->viewheight -= FRACUNIT;
        if(dpl->viewheight < 6 * FRACUNIT)
            dpl->viewheight = 6 * FRACUNIT;
    }

    // Jumping.
    if(pl->jumptics)
        pl->jumptics--;
    P_CheckPlayerJump(pl);

#if __JHERETIC__
    // Sector wind thrusts the player around.
    sector = P_GetPtrp(mo->subsector, DMU_SECTOR);
    if(P_XSector(sector)->special)
        P_PlayerInWindSector(pl);
#endif
    // Flying.
    fly = cmd->fly; //lookfly >> 4;
/*  if(fly > 7)
    {
        fly -= 16;
    }
*/
    if(fly && pl->powers[pw_flight])
    {
        if(fly != TOCENTER)
        {
            pl->flyheight = fly * 2;
        }
    }
    // We are flying when the Fly flag is set.
    if(mo->ddflags & DDMF_FLY)
    {
        // If we were on a mobj, we are NOT now.
        if(mo->onmobj)
            mo->onmobj = NULL;

        // Keep the Heretic fly flag in sync.
        mo->flags2 |= MF2_FLY;

        mo->momz = pl->flyheight * FRACUNIT;
        if(pl->flyheight)
            pl->flyheight /= 2;
        // Do some fly-bobbing.
        if(mo->pos[VZ] > mo->floorz && (mo->flags2 & MF2_FLY) &&
           !mo->onmobj && (leveltime & 2))
            mo->pos[VZ] += finesine[(FINEANGLES / 20 * leveltime >> 2) & FINEMASK];
    }

    // Set the proper thrust multiplier. XG gives this quite easily.
    // (The thrust multiplier is used by Cl_MovePlayer, the movement
    // "predictor"; almost all clientside movement is handled by that
    // routine, though.)
#if __JHERETIC__
    // FIXME: Client can't know for sure about sector specials.
    if(P_XSector(sector)->special == 15)
        Set(DD_CPLAYER_THRUST_MUL, FRACUNIT >> 1);  // Friction_Low
    else
#endif
        Set(DD_CPLAYER_THRUST_MUL,
            XS_ThrustMul(P_GetPtrp(mo->subsector, DMU_SECTOR)));

    // Update view angles. The server fixes them if necessary.
    mo->angle = dpl->clAngle;
    dpl->lookdir = dpl->clLookDir;
}

void P_PlayerThink(player_t *player)
{
    ticcmd_t *cmd;
    weapontype_t newweapon, oldweapon;
    mobj_t *plrmo = player->plr->mo;

    // No-clip cheat
    if(player->cheats & CF_NOCLIP)
    {
        plrmo->flags |= MF_NOCLIP;
    }
    else
    {
        plrmo->flags &= ~MF_NOCLIP;
    }

    // Selector 0 = Generic (used by default)
    // Selector 1 = Staff
    // Selector 2 = Goldwand
    // Selector 3 = Crossbow
    // Selector 4 = Blaster
    // Selector 5 = Skullrod
    // Selector 6 = Phoenixrod
    // Selector 7 = Mace
    // Selector 8 = Gauntlets

    if(player->class == PCLASS_CHICKEN)
        plrmo->selector = 9;
    else
    plrmo->selector =
        (plrmo->selector & ~DDMOBJ_SELECTOR_MASK) | (player->readyweapon + 1);

    P_CameraThink(player);      // $democam

    cmd = &player->cmd;
    if(plrmo->flags & MF_JUSTATTACKED)
    {                           // Gauntlets attack auto forward motion
        //cmd->angleturn = 0;
        cmd->angle = plrmo->angle >> 16;    // Don't turn.
        // The client must know of this.
        player->plr->flags |= DDPF_FIXANGLES;
        cmd->forwardMove = 0xc800 / 512;
        cmd->sideMove = 0;
        plrmo->flags &= ~MF_JUSTATTACKED;
    }
    // messageTics is above the rest of the counters so that messages will
    //              go away, even in death.
    player->messageTics--;      // Can go negative
    if(!player->messageTics)
    {                           // Refresh the screen when a message goes away
        //BorderTopRefresh = true;
        GL_Update(DDUF_TOP);
    }
    if(player->playerstate == PST_DEAD)
    {
        P_DeathThink(player);
        return;
    }

    if(player->jumptics)
        player->jumptics--;
    if(player->morphTics)
    {
        P_MorphPlayerThink(player);
    }
    // Handle movement
    if(plrmo->reactiontime)
        plrmo->reactiontime--;
    else
        P_MovePlayer(player);

    P_CalcHeight(player);

    if(P_XSector(P_GetPtrp(plrmo->subsector, DMU_SECTOR))->special)
        P_PlayerInSpecialSector(player);

    if(player->jumptics)
        player->jumptics--;

    if(cmd->arti)
    {                           // Use an artifact
        if(cmd->arti == 0xff)
            P_PlayerNextArtifact(player);
        else
            P_PlayerUseArtifact(player, cmd->arti);
    }

    oldweapon = player->pendingweapon;

    // There might be a special weapon change.
    if(cmd->changeWeapon == TICCMD_NEXT_WEAPON ||
       cmd->changeWeapon == TICCMD_PREV_WEAPON)
    {
        player->pendingweapon =
            P_PlayerFindWeapon(player, cmd->changeWeapon == TICCMD_NEXT_WEAPON);
        cmd->changeWeapon = 0;
    }

    // Check for weapon change
    if(cmd->changeWeapon)
    {
        // The actual changing of the weapon is done when the weapon
        // psprite can do it (A_WeaponReady), so it doesn't happen in
        // the middle of an attack.
        newweapon = cmd->changeWeapon - 1;

        if(player->weaponowned[newweapon] && newweapon != player->readyweapon)
        {
            int lvl = (player->powers[pw_weaponlevel2]? 1 : 0);
            if(weaponinfo[newweapon][player->class].mode[lvl].gamemodebits
                & gamemodebits)
            {
                player->pendingweapon = newweapon;
            }
        }
    }

    if(player->pendingweapon != oldweapon)
        player->update |= PSF_PENDING_WEAPON;

    // Check for use
    if(cmd->use)
    {
        if(!player->usedown)
        {
            P_UseLines(player);
            player->usedown = true;
        }
    }
    else
    {
        player->usedown = false;
    }
    // Morph counter
    if(player->morphTics)
    {
        if(player->chickenPeck)
        {                       // Chicken attack counter
            player->chickenPeck -= 3;
        }
        if(!--player->morphTics)
        {                       // Attempt to undo the chicken
            P_UndoPlayerMorph(player);
        }
    }
    // Cycle psprites
    P_MovePsprites(player);
    // Other Counters
    if(player->powers[pw_invulnerability])
    {
        player->powers[pw_invulnerability]--;
    }
    if(player->powers[pw_invisibility])
    {
        if(!--player->powers[pw_invisibility])
        {
            plrmo->flags &= ~MF_SHADOW;
        }
    }
    if(player->powers[pw_infrared])
    {
        player->powers[pw_infrared]--;
    }
    if(player->powers[pw_flight])
    {
        if(!--player->powers[pw_flight])
        {
            if(plrmo->pos[VZ] != plrmo->floorz)
            {
                player->centering = true;
            }

            plrmo->flags2 &= ~MF2_FLY;
            plrmo->flags &= ~MF_NOGRAVITY;
            //BorderTopRefresh = true; //make sure the sprite's cleared out
            GL_Update(DDUF_TOP);
        }
    }
    if(player->powers[pw_weaponlevel2] && player->class == PCLASS_PLAYER)
    {
        if(!--player->powers[pw_weaponlevel2])
        {
            if((player->readyweapon == WP_SIXTH) &&
               (player->psprites[ps_weapon].state != &states[S_PHOENIXREADY])
               && (player->psprites[ps_weapon].state != &states[S_PHOENIXUP]))
            {
                P_SetPsprite(player, ps_weapon, S_PHOENIXREADY);
                player->ammo[am_phoenixrod] -= USE_PHRD_AMMO_2;
                player->refire = 0;
                player->update |= PSF_AMMO;
            }
            else if((player->readyweapon == WP_EIGHTH) ||
                    (player->readyweapon == WP_FIRST))
            {
                player->pendingweapon = player->readyweapon;
                player->update |= PSF_PENDING_WEAPON;
            }
            //BorderTopRefresh = true;
            GL_Update(DDUF_TOP);
        }
    }
    if(player->damagecount)
    {
        player->damagecount--;
    }
    if(player->bonuscount)
    {
        player->bonuscount--;
    }
    // Colormaps
    if(player->powers[pw_invulnerability])
    {
        /*if(player->powers[pw_invulnerability] > BLINKTHRESHOLD
           || (player->powers[pw_invulnerability]&8))
           {
           player->plr->fixedcolormap = INVERSECOLORMAP;
           }
           else
           {
           player->plr->fixedcolormap = 0;
           } */
    }
    else if(player->powers[pw_infrared])
    {
        if(player->powers[pw_infrared] <= BLINKTHRESHOLD)
        {
            if(player->powers[pw_infrared] & 8)
            {
                player->plr->fixedcolormap = 0;
            }
            else
            {
                player->plr->fixedcolormap = 1;
            }
        }
        else if(!(leveltime & 16) && player == &players[consoleplayer])
        {
            if(newtorch)
            {
                if(player->plr->fixedcolormap + newtorchdelta > 7 ||
                   player->plr->fixedcolormap + newtorchdelta < 1 ||
                   newtorch == player->plr->fixedcolormap)
                {
                    newtorch = 0;
                }
                else
                {
                    player->plr->fixedcolormap += newtorchdelta;
                }
            }
            else
            {
                newtorch = (M_Random() & 7) + 1;
                newtorchdelta =
                    (newtorch ==
                     player->plr->fixedcolormap) ? 0 : ((newtorch >
                                                         player->plr->
                                                         fixedcolormap) ? 1 :
                                                        -1);
            }
        }
    }
    else
    {
        player->plr->fixedcolormap = 0;
    }
}

void P_ArtiTele(player_t *player)
{
    int     i;
    int     selections;
    fixed_t destX;
    fixed_t destY;
    angle_t destAngle;

    if(deathmatch)
    {
        selections = deathmatch_p - deathmatchstarts;
        i = P_Random() % selections;
        destX = deathmatchstarts[i].x << FRACBITS;
        destY = deathmatchstarts[i].y << FRACBITS;
        destAngle = ANG45 * (deathmatchstarts[i].angle / 45);
    }
    else
    {
        // FIXME?: DJS - this doesn't seem right...
        destX = playerstarts[0].x << FRACBITS;
        destY = playerstarts[0].y << FRACBITS;
        destAngle = ANG45 * (playerstarts[0].angle / 45);
    }
    P_Teleport(player->plr->mo, destX, destY, destAngle);
    /*S_StartSound(sfx_wpnup, NULL); // Full volume laugh
       NetSv_Sound(NULL, sfx_wpnup, player-players); */
    S_StartSound(sfx_wpnup, NULL);
}

void P_CheckReadyArtifact()
{
    player_t *player = &players[consoleplayer];
    extern int inv_ptr;
    extern int curpos;

    if(!player->inventory[inv_ptr].count)
    {
        // Set position markers and get next readyArtifact
        inv_ptr--;
        if(inv_ptr < 6)
        {
            curpos--;
            if(curpos < 0)
            {
                curpos = 0;
            }
        }
        if(inv_ptr >= player->inventorySlotNum)
        {
            inv_ptr = player->inventorySlotNum - 1;
        }
        if(inv_ptr < 0)
        {
            inv_ptr = 0;
        }
        player->readyArtifact = player->inventory[inv_ptr].type;

        if(!player->inventorySlotNum)
            player->readyArtifact = arti_none;
    }
}

void P_PlayerNextArtifact(player_t *player)
{
    extern int inv_ptr;
    extern int curpos;

    if(player == &players[consoleplayer])
    {
        inv_ptr--;
        if(inv_ptr < 6)
        {
            curpos--;
            if(curpos < 0)
            {
                curpos = 0;
            }
        }
        if(inv_ptr < 0)
        {
            inv_ptr = player->inventorySlotNum - 1;
            if(inv_ptr < 6)
            {
                curpos = inv_ptr;
            }
            else
            {
                curpos = 6;
            }
        }
        player->readyArtifact = player->inventory[inv_ptr].type;
    }
}

void P_PlayerRemoveArtifact(player_t *player, int slot)
{
    int     i;
    extern int inv_ptr;
    extern int curpos;

    player->update |= PSF_INVENTORY;
    player->artifactCount--;
    if(!(--player->inventory[slot].count))
    {                           // Used last of a type - compact the artifact list
        player->readyArtifact = arti_none;
        player->inventory[slot].type = arti_none;
        for(i = slot + 1; i < player->inventorySlotNum; i++)
        {
            player->inventory[i - 1] = player->inventory[i];
        }
        player->inventorySlotNum--;
        if(player == &players[consoleplayer])
        {                       // Set position markers and get next readyArtifact
            inv_ptr--;
            if(inv_ptr < 6)
            {
                curpos--;
                if(curpos < 0)
                {
                    curpos = 0;
                }
            }
            if(inv_ptr >= player->inventorySlotNum)
            {
                inv_ptr = player->inventorySlotNum - 1;
            }
            if(inv_ptr < 0)
            {
                inv_ptr = 0;
            }
            player->readyArtifact = player->inventory[inv_ptr].type;
        }
    }
}

void P_PlayerUseArtifact(player_t *player, artitype_t arti)
{
    int     i;
    boolean play_sound = false;

    for(i = 0; i < player->inventorySlotNum; i++)
    {
        if(arti == NUMARTIFACTS)    // Use everything in panic?
        {
            if(P_UseArtifact(player, player->inventory[i].type))
            {                   // Artifact was used - remove it from inventory
                P_PlayerRemoveArtifact(player, i);
                play_sound = true;
                if(player == &players[consoleplayer])
                    ArtifactFlash = 4;
            }
        }
        else if(player->inventory[i].type == arti)
        {                       // Found match - try to use
            if(P_UseArtifact(player, arti))
            {                   // Artifact was used - remove it from inventory
                P_PlayerRemoveArtifact(player, i);
                play_sound = true;
                if(player == &players[consoleplayer])
                    ArtifactFlash = 4;
            }
            else
            {                   // Unable to use artifact, advance pointer
                P_PlayerNextArtifact(player);
            }
            break;
        }
    }
    if(play_sound)
    {
        S_ConsoleSound(sfx_artiuse, NULL, player - players);
    }
}

boolean P_UseArtifact(player_t *player, artitype_t arti)
{
    mobj_t *mo;
    angle_t angle;

    switch (arti)
    {
    case arti_invulnerability:
        if(!P_GivePower(player, pw_invulnerability))
        {
            return (false);
        }
        break;
    case arti_invisibility:
        if(!P_GivePower(player, pw_invisibility))
        {
            return (false);
        }
        break;
    case arti_health:
        if(!P_GiveBody(player, 25))
        {
            return (false);
        }
        break;
    case arti_superhealth:
        if(!P_GiveBody(player, 100))
        {
            return (false);
        }
        break;
    case arti_tomeofpower:
        if(player->morphTics)
        {                       // Attempt to undo chicken
            if(P_UndoPlayerMorph(player) == false)
            {                   // Failed
                P_DamageMobj(player->plr->mo, NULL, NULL, 10000);
            }
            else
            {                   // Succeeded
                player->morphTics = 0;
                S_StartSound(sfx_wpnup, player->plr->mo);
            }
        }
        else
        {
            if(!P_GivePower(player, pw_weaponlevel2))
            {
                return (false);
            }
            if(player->readyweapon == WP_FIRST)
            {
                P_SetPsprite(player, ps_weapon, S_STAFFREADY2_1);
            }
            else if(player->readyweapon == WP_EIGHTH)
            {
                P_SetPsprite(player, ps_weapon, S_GAUNTLETREADY2_1);
            }
        }
        break;
    case arti_torch:
        if(!P_GivePower(player, pw_infrared))
        {
            return (false);
        }
        break;
    case arti_firebomb:
        angle = player->plr->mo->angle >> ANGLETOFINESHIFT;
        mo = P_SpawnMobj(player->plr->mo->pos[VX] + 24 * finecosine[angle],
                         player->plr->mo->pos[VY] + 24 * finesine[angle],
                         player->plr->mo->pos[VZ] - player->plr->mo->floorclip +
                         15 * FRACUNIT, MT_FIREBOMB);
        mo->target = player->plr->mo;
        break;
    case arti_egg:
        mo = player->plr->mo;
        P_SpawnPlayerMissile(mo, MT_EGGFX);
        P_SPMAngle(mo, MT_EGGFX, mo->angle - (ANG45 / 6));
        P_SPMAngle(mo, MT_EGGFX, mo->angle + (ANG45 / 6));
        P_SPMAngle(mo, MT_EGGFX, mo->angle - (ANG45 / 3));
        P_SPMAngle(mo, MT_EGGFX, mo->angle + (ANG45 / 3));
        break;
    case arti_fly:
        if(!P_GivePower(player, pw_flight))
        {
            return (false);
        }
        break;
    case arti_teleport:
        P_ArtiTele(player);
        break;
    default:
        return (false);
    }
    return (true);
}
