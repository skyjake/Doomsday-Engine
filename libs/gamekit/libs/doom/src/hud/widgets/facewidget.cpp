/** @file ammowidget.h  GUI widget for visualizing high-level player status.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "hud/widgets/facewidget.h"

#include "gl_drawpatch.h"
#include "player.h"

using namespace de;

#define FACE_STRAIGHT_COUNT         ( 3 )
#define FACE_TURN_COUNT             ( 2 )
#define FACE_SPECIAL_COUNT          ( 3 )
#define FACE_PAIN_COUNT             ( 5 )
#define FACE_EXTRA_COUNT            ( 2 )

#define FACE_STRIDE                 ( FACE_STRAIGHT_COUNT + FACE_TURN_COUNT + FACE_SPECIAL_COUNT )
#define FACE_COUNT                  ( FACE_STRIDE * FACE_PAIN_COUNT + FACE_EXTRA_COUNT )

#define FACE_TURN_FIRST             ( FACE_STRAIGHT_COUNT )
#define FACE_OUCH_FIRST             ( FACE_TURN_FIRST + FACE_TURN_COUNT )
#define FACE_GRIN_FIRST             ( FACE_OUCH_FIRST + 1 )
#define FACE_RAMPAGE_FIRST          ( FACE_GRIN_FIRST + 1 )
#define FACE_GOD_FIRST              ( FACE_PAIN_COUNT * FACE_STRIDE )
#define FACE_DEAD_FIRST             ( FACE_GOD_FIRST + 1 )

#define FACE_STRAIGHT_TICS          ( TICRATE / 2 )
#define FACE_TURN_TICS              ( 1 * TICRATE )
#define FACE_OUCH_TICS              ( 1 * TICRATE )
#define FACE_GRIN_TICS              ( 2 * TICRATE )
#define FACE_RAMPAGE_TICS           ( 2 * TICRATE )

#define FACE_PAIN_THRESHOLD         ( 20 )

static patchid_t pFaces[FACE_COUNT];
static patchid_t pBackground[NUMTEAMS];

DE_PIMPL_NOREF(guidata_face_t)
{
    dint faceTicks = 0;  ///< Count until face changes.
    dint faceIndex = 0;  ///< Current face index, used by wFaces.
    dint priority  = 0;

    dint oldHealth = 0;
    bool oldWeaponsOwned[NUM_WEAPON_TYPES];
    dint lastAttackDown = 0;

    Impl() { de::zap(oldWeaponsOwned); }

    dint painOffset(dint player) const
    {
        const player_t *plr = &::players[player];
        return FACE_STRIDE * (((100 - de::min(100, plr->health)) * FACE_PAIN_COUNT) / 101);
    }
};

guidata_face_t::guidata_face_t(void (*updateGeometry) (HudWidget *wi),
                               void (*drawer) (HudWidget *wi, const Point2Raw *offset),
                               dint player)
    : HudWidget(updateGeometry,
                drawer,
                player)
    , d(new Impl)
{}

guidata_face_t::~guidata_face_t()
{}

void guidata_face_t::reset()
{
    player_t *plr = &::players[player()];

    d->faceTicks      = 0;
    d->faceIndex      = 0;
    d->priority       = 0;
    d->lastAttackDown = -1;
    d->oldHealth      = -1;
    for(dint i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        d->oldWeaponsOwned[i] = CPP_BOOL(plr->weapons[i].owned);
    }
}

/**
 * This not-very-pretty routine handles face animation states and timing thereof.
 * The precedence of expressions is:
 *
 * dead > evil grin > turned head > straight ahead
 */
void guidata_face_t::tick(timespan_t /*elapsed*/)
{
    const player_t *plr = &::players[player()];

    if(Pause_IsPaused() || !DD_IsSharpTick())
        return;

    if(d->priority < 10)
    {
        // Dead.
        if(!plr->health)
        {
            d->priority  = 9;
            d->faceIndex = FACE_DEAD_FIRST;
            d->faceTicks = 1;
        }
    }

    if(d->priority < 9)
    {
        if(plr->bonusCount)
        {
            // Picking up a bonus.
            bool doGrin = false;
            for(dint i = 0; i < NUM_WEAPON_TYPES; ++i)
            {
                if(d->oldWeaponsOwned[i] != CPP_BOOL(plr->weapons[i].owned))
                {
                    doGrin = true;
                    d->oldWeaponsOwned[i] = CPP_BOOL(plr->weapons[i].owned);
                }
            }

            if(doGrin)
            {
                // Grin if just picked up weapon.
                d->priority  = 8;
                d->faceTicks = FACE_GRIN_TICS;
                d->faceIndex = FACE_GRIN_FIRST + d->painOffset(player());
            }
        }
    }

    if(d->priority < 8)
    {
        if(plr->damageCount && plr->attacker && plr->attacker != plr->plr->mo)
        {
            // Being attacked.
            d->priority = 7;

            // DOOM BUG
            // This test was inversed, thereby the OUCH face was NEVER used
            // in normal gameplay as it requires the player recieving damage
            // to end up with MORE health than he started with.
            // Also, priority was not changed which would have resulted in a
            // frame duration of only 1 tic.
            // if(plr->health - oldHealth > ST_MUCHPAIN)

            if((::cfg.fixOuchFace?
               (d->oldHealth - plr->health) :
               (plr->health - d->oldHealth)) > FACE_PAIN_THRESHOLD)
            {
                d->faceTicks = FACE_TURN_TICS;
                d->faceIndex = FACE_OUCH_FIRST + d->painOffset(player());
                if(::cfg.fixOuchFace)
                {
                    d->priority = 8; // Added to fix 1 tic issue.
                }
            }
            else
            {
                angle_t badGuyAngle = M_PointToAngle2(plr->plr->mo->origin, plr->attacker->origin);
                angle_t diffAng;
                dint i;

                if(badGuyAngle > plr->plr->mo->angle)
                {
                    // Whether right or left.
                    diffAng = badGuyAngle - plr->plr->mo->angle;
                    i = diffAng > ANG180;
                }
                else
                {
                    // Whether left or right.
                    diffAng = plr->plr->mo->angle - badGuyAngle;
                    i = diffAng <= ANG180;
                }

                d->faceTicks = FACE_TURN_TICS;
                d->faceIndex = d->painOffset(player());

                if(diffAng < ANG45)
                {
                    // Head-on.
                    d->faceIndex += FACE_RAMPAGE_FIRST;
                }
                else if(i)
                {
                    // Turn face right.
                    d->faceIndex += FACE_TURN_FIRST;
                }
                else
                {
                    // Turn face left.
                    d->faceIndex += FACE_TURN_FIRST + 1;
                }
            }
        }
    }

    if(d->priority < 7)
    {
        // Getting hurt because of your own damn stupidity.
        if(plr->damageCount)
        {
            // DOOM BUG
            // This test was inversed, thereby the OUCH face was NEVER used
            // in normal gameplay as it requires the player recieving damage
            // to end up with MORE health than he started with.
            // if(plr->health - oldHealth > ST_MUCHPAIN)

            if((::cfg.fixOuchFace?
               (d->oldHealth - plr->health) :
               (plr->health - d->oldHealth)) > FACE_PAIN_THRESHOLD)
            {
                d->priority  = 7;
                d->faceTicks = FACE_TURN_TICS;
                d->faceIndex = FACE_OUCH_FIRST + d->painOffset(player());
            }
            else
            {
                d->priority  = 6;
                d->faceTicks = FACE_TURN_TICS;
                d->faceIndex = FACE_RAMPAGE_FIRST + d->painOffset(player());
            }
        }
    }

    if(d->priority < 6)
    {
        // Rapid firing.
        if(plr->attackDown)
        {
            if(d->lastAttackDown == -1)
            {
                d->lastAttackDown = FACE_RAMPAGE_TICS;
            }
            else if(!--d->lastAttackDown)
            {
                d->lastAttackDown = 1;

                d->priority  = 5;
                d->faceTicks = 1;
                d->faceIndex = FACE_RAMPAGE_FIRST + d->painOffset(player());
            }
        }
        else
        {
            d->lastAttackDown = -1;
        }
    }

    if(d->priority < 5)
    {
        // Invulnerability.
        if((P_GetPlayerCheats(plr) & CF_GODMODE) || plr->powers[PT_INVULNERABILITY])
        {
            d->priority  = 4;
            d->faceTicks = 1;
            d->faceIndex = FACE_GOD_FIRST;
        }
    }

    // Look left or look right if the facecount has timed out.
    if(!d->faceTicks)
    {
        d->priority  = 0;
        d->faceTicks = FACE_STRAIGHT_TICS;
        d->faceIndex = d->painOffset(player()) + (M_Random() % 3);
    }
    d->oldHealth = plr->health;

    d->faceTicks -= 1;
}

void Face_Drawer(guidata_face_t *face, const Point2Raw *offset)
{
#define X_OFFSET                ( 143 )
#define Y_OFFSET                (   0 )
#define SCALE                   ( 0.7 )

    //const dint activeHud = ST_ActiveHud(face->player());
    const dfloat iconOpacity = ::uiRendState->pageAlpha * ::cfg.common.hudIconAlpha;

    if(!::cfg.hudShown[HUD_FACE]) return;
    if(ST_AutomapIsOpen(face->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[face->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchid_t pFace = ::pFaces[face->d->faceIndex % FACE_COUNT];
    if(!pFace) return;

    dint x = -(SCREENWIDTH / 2 - X_OFFSET);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(SCALE * ::cfg.common.hudScale, SCALE * ::cfg.common.hudScale, 1);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconOpacity);

    // Draw the backround:
    patchinfo_t bgInfo;
    if(R_GetPatchInfo(::pBackground[::cfg.playerColor[face->player()]], &bgInfo))
    {
        if(IS_NETGAME)
        {
            GL_DrawPatch(bgInfo.id);
        }
        x += bgInfo.geometry.size.width/2;
    }

    // Draw the face:
    GL_DrawPatch(pFace, Vec2i(x, -1));

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef SCALE
#undef Y_OFFSET
#undef X_OFFSET
}

void SBarFace_Drawer(guidata_face_t *face, const Point2Raw *offset)
{
#define X_OFFSET                ( 143 )
#define Y_OFFSET                (   0 )
#define SCALE                   ( 1.0 )

    Vec2i const origin(-ST_WIDTH / 2, -ST_HEIGHT);

    const dint activeHud     = ST_ActiveHud(face->player());
    const dint yOffset       = ST_HEIGHT * (1 - ST_StatusBarShown(face->player()));
    const dfloat iconOpacity = (activeHud == 0? 1 : ::uiRendState->pageAlpha * ::cfg.common.statusbarCounterAlpha);

    if(ST_AutomapIsOpen(face->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[face->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    if(face->d->faceIndex < 0) return;
    patchid_t patchId = ::pFaces[face->d->faceIndex];

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(SCALE * ::cfg.common.statusbarScale, SCALE * ::cfg.common.statusbarScale, 1);
    DGL_Translatef(0, yOffset, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, iconOpacity);

    // Draw the face:
    GL_DrawPatch(patchId, origin + Vec2i(X_OFFSET, Y_OFFSET), ALIGN_TOPLEFT);

    DGL_Disable(DGL_TEXTURE_2D);
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef SCALE
#undef Y_OFFSET
#undef X_OFFSET
}

void Face_UpdateGeometry(guidata_face_t *face)
{
#define SCALE                   ( 0.7 )

    Rect_SetWidthHeight(&face->geometry(), 0, 0);

    if(!::cfg.hudShown[HUD_FACE]) return;

    if(ST_AutomapIsOpen(face->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[face->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t info;
    patchid_t pFace = ::pFaces[face->d->faceIndex % FACE_COUNT];
    if(!pFace) return;

    if(!(R_GetPatchInfo(::pBackground[::cfg.playerColor[face->player()]], &info) ||
         R_GetPatchInfo(pFace, &info))) return;

    Rect_SetWidthHeight(&face->geometry(), info.geometry.size.width  * SCALE * ::cfg.common.hudScale,
                                           info.geometry.size.height * SCALE * ::cfg.common.hudScale);

#undef SCALE
}

void SBarFace_UpdateGeometry(guidata_face_t *face)
{
#define SCALE                   ( 1 )

    Rect_SetWidthHeight(&face->geometry(), 0, 0);

    if(ST_AutomapIsOpen(face->player()) && ::cfg.common.automapHudDisplay == 0) return;
    if(P_MobjIsCamera(::players[face->player()].plr->mo) && Get(DD_PLAYBACK)) return;

    patchinfo_t info;
    patchid_t pFace = ::pFaces[face->d->faceIndex % FACE_COUNT];
    if(!pFace || !R_GetPatchInfo(pFace, &info)) return;

    Rect_SetWidthHeight(&face->geometry(), info.geometry.size.width  * SCALE * ::cfg.common.hudScale,
                                           info.geometry.size.height * SCALE * ::cfg.common.hudScale);

#undef SCALE
}

void guidata_face_t::prepareAssets()  // static
{
    // Backgrounds for each team color.
    for(dint i = 0; i < NUMTEAMS; ++i)
    {
        ::pBackground[i] = R_DeclarePatch(Stringf("STFB%i", i));
    }

    dint idx = 0;
    for(dint i = 0; i < FACE_PAIN_COUNT; ++i)
    {
        for(dint k = 0; k < FACE_STRAIGHT_COUNT; ++k)
        {
            ::pFaces[idx++] = R_DeclarePatch(Stringf("STFST%i%i", i, k));
        }
        ::pFaces[idx++] = R_DeclarePatch(Stringf("STFTR%i0" , i));  // Turn right.
        ::pFaces[idx++] = R_DeclarePatch(Stringf("STFTL%i0" , i));  // Turn left.
        ::pFaces[idx++] = R_DeclarePatch(Stringf("STFOUCH%i", i));  // Ouch.
        ::pFaces[idx++] = R_DeclarePatch(Stringf("STFEVL%i" , i));  // Evil grin.
        ::pFaces[idx++] = R_DeclarePatch(Stringf("STFKILL%i", i));  // Pissed off.
    }
    ::pFaces[idx++] = R_DeclarePatch("STFGOD0");
    ::pFaces[idx++] = R_DeclarePatch("STFDEAD0");

    DE_ASSERT(idx == FACE_COUNT);
}
