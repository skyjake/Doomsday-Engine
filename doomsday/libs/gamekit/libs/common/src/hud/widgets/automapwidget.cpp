/** @file automapwidget.cpp  GUI widget for the automap.
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
#include "hud/widgets/automapwidget.h"

#include <de/logbuffer.h>
#include <de/dscript.h>
#include <de/vector.h>

#include "dmu_lib.h"
#include "g_common.h"
#include "gamesession.h"
#include "hu_stuff.h"
#include "hud/automapstyle.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "r_common.h"
#if __JDOOM64__
#  include "p_inventory.h"
#endif

#define UIAUTOMAP_BORDER        4  ///< In fixed 320x200 pixels.

using namespace de;
using namespace res;

static void AutomapWidget_UpdateGeometry(AutomapWidget *amap)
{
    DE_ASSERT(amap);
    amap->updateGeometry();
}

static void AutomapWidget_Draw(AutomapWidget *amap, const Point2Raw *offset)
{
    DE_ASSERT(amap);
    amap->draw(offset? Vec2i(offset->xy) : Vec2i());
}

struct uiautomap_rendstate_t
{
    player_t *plr;
    dint obType;         ///< The type of object to draw. @c -1= only line specials.
    dd_bool glowOnly;
    dglprimtype_t primType;
};
static uiautomap_rendstate_t rs;
static dbyte freezeMapRLs;

// if -1 no background image will be drawn.
#if __JDOOM__ || __JDOOM64__
static dint autopageLumpNum = -1;
#elif __JHERETIC__
static dint autopageLumpNum = 1;
#else
static dint autopageLumpNum = 1;
#endif

static DGLuint amMaskTexture;  // Used to mask the map primitives.

namespace internal
{
    static Vec2d rotate(const Vec2d &point, ddouble radian)
    {
        const ddouble c = std::cos(radian);
        const ddouble s = std::sin(radian);
        return Vec2d(c * point.x - s * point.y, s * point.x + c * point.y);
    }

    static void initAABB(coord_t aabb[4], const Vec2d &point)
    {
        DE_ASSERT(aabb);
        aabb[BOXLEFT] = aabb[BOXRIGHT ] = point.x;
        aabb[BOXTOP ] = aabb[BOXBOTTOM] = point.y;
    }

    static void addToAABB(coord_t aabb[4], const Vec2d &point)
    {
        DE_ASSERT(aabb);
        if     (point.x < aabb[BOXLEFT  ]) aabb[BOXLEFT  ] = point.x;
        else if (point.x > aabb[BOXRIGHT ]) aabb[BOXRIGHT ] = point.x;

        if     (point.y < aabb[BOXBOTTOM]) aabb[BOXBOTTOM] = point.y;
        else if (point.y > aabb[BOXTOP   ]) aabb[BOXTOP   ] = point.y;
    }

    static dd_bool interceptEdge(coord_t point[2], coord_t const startA[2],
        coord_t const endA[2], coord_t const startB[2], coord_t const endB[2])
    {
        coord_t directionA[2];
        V2d_Subtract(directionA, endA, startA);
        if (V2d_PointOnLineSide(point, startA, directionA) >= 0)
        {
            coord_t directionB[2];
            V2d_Subtract(directionB, endB, startB);
            V2d_Intersection(startA, directionA, startB, directionB, point);
            return true;
        }
        return false;
    }

    static Vec2d fitPointInRectangle(const Vec2d &point, const Vec2d &topLeft,
        const Vec2d &topRight, const Vec2d &bottomRight,
        const Vec2d &bottomLeft, const Vec2d &viewPoint)
    {
        ddouble pointV1[2];
        point.decompose(pointV1);

        ddouble topLeftV1[2];
        topLeft.decompose(topLeftV1);

        ddouble topRightV1[2];
        topRight.decompose(topRightV1);

        ddouble bottomRightV1[2];
        bottomRight.decompose(bottomRightV1);

        ddouble bottomLeftV1[2];
        bottomLeft.decompose(bottomLeftV1);

        ddouble viewPointV1[2];
        viewPoint.decompose(viewPointV1);

        // Trace a vector from the view location to the marked point and intercept
        // vs the edges of the rotated view window.
        if (!interceptEdge(pointV1, topLeftV1, bottomLeftV1, viewPointV1, pointV1))
            interceptEdge(pointV1, bottomRightV1, topRightV1, viewPointV1, pointV1);
        if (!interceptEdge(pointV1, topRightV1, topLeftV1, viewPointV1, pointV1))
            interceptEdge(pointV1, bottomLeftV1, bottomRightV1, viewPointV1, pointV1);

        return Vec2d(pointV1);
    }

    static void drawVectorGraphic(svgid_t vgId, const Vec2d &origin, dfloat angle,
        dfloat scale, const Vec3f &color, dfloat opacity, blendmode_t blendmode)
    {
        opacity = de::clamp(0.f, opacity, 1.f);

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PushMatrix();
        DGL_Translatef(origin.x, origin.y, 1);

        DGL_Color4f(color.x, color.y, color.z, opacity);
        DGL_BlendMode(blendmode);

        Point2Rawf originp(origin.x, origin.y);
        GL_DrawSvg3(vgId, &originp, scale, angle);

        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PopMatrix();
    }

    static dint playerPaletteColor(dint consoleNum)
    {
        static dint playerColors[NUMPLAYERCOLORS] = {
            AM_PLR1_COLOR, AM_PLR2_COLOR, AM_PLR3_COLOR, AM_PLR4_COLOR,
#if __JHEXEN__
            AM_PLR5_COLOR, AM_PLR6_COLOR, AM_PLR7_COLOR, AM_PLR8_COLOR
#endif
        };

        if (!IS_NETGAME) return WHITE;

        return playerColors[cfg.playerColor[de::max(0, consoleNum) % MAXPLAYERS]];
    }

    static void drawPlayerMarker(dint consoleNum, AutomapStyle *style)
    {
        DE_ASSERT(consoleNum >= 0 && consoleNum < MAXPLAYERS);
        player_t *player = &players[consoleNum];
        if (!player->plr->inGame) return;

        mobj_t *plrMob = player->plr->mo;
        if (!plrMob) return;

        coord_t origin[3]; //Mobj_OriginSmoothed(plrMob, origin);
        memcpy(origin, plrMob->origin, sizeof(origin));
        const float angle = plrMob->angle /*Mobj_AngleSmoothed(plrMob)*/ / (float) ANGLE_MAX * 360; /* $unifiedangles */

        dfloat color[3]; R_GetColorPaletteRGBf(0, playerPaletteColor(consoleNum), color, false);

        dfloat opacity = cfg.common.automapLineAlpha * uiRendState->pageAlpha;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        if (player->powers[PT_INVISIBILITY])
            opacity *= .125f;
#endif

        drawVectorGraphic(style->objectSvg(AMO_THINGPLAYER), Vec2d(origin),
                          angle, PLAYERRADIUS, Vec3f(color), opacity, BM_NORMAL);
    }

}  // namespace internal
using namespace ::internal;

DE_PIMPL(AutomapWidget)
{
    AutomapStyle *style = nullptr;

    float pixelRatio = 1.f; // DisplayMode.PIXEL_RATIO

    //DGLuint lists[NUM_MAP_OBJECTLISTS];  ///< Each list contains one or more of given type of automap wi.
    bool needBuildLists = false;         ///< @c true= force a rebuild of all lists.

    dint flags = 0;
    bool open     = false;       ///< @c true= currently active.
    bool revealed = false;
    bool follow   = true;        ///< @c true= camera position tracks followed player.
    bool rotate   = false;

    bool forceMaxScale = false;  ///< If the map is currently in forced max zoom mode.
    dfloat priorToMaxScale = 0;  ///< Viewer scale before entering maxScale mode.

    dfloat minScale  = 1.f;
    dfloat scaleMTOF = 1.f;      ///< Used by MTOF to scale from map-to-frame-buffer coords.
    dfloat scaleFTOM = 1.f;      ///< Used by FTOM to scale from frame-buffer-to-map coords (=1/scaleMTOF).

    coord_t bounds[4];           ///< Map space bounds:

    // Visual properties:
    dfloat opacity = 0, targetOpacity = 0, oldOpacity = 0;
    dfloat opacityTimer = 0;

    // Viewer location on the map:
    Vec2d view, targetView, oldView;
    dfloat viewTimer = 0;

    coord_t maxViewPositionDelta = 128;
//    Vec2d viewPL;  // For the parallax layer.

    // View frame scale:
    dfloat viewScale = 1, targetViewScale = 1, oldViewScale = 1;
    dfloat viewScaleTimer = 0;

    bool needViewScaleUpdate = false;
    dfloat minScaleMTOF = 0;
    dfloat maxScaleMTOF = 0;

    // View frame rotation:
    dfloat angle = 0, targetAngle = 0, oldAngle = 0;
    dfloat angleTimer = 0;

    // Bounding box of the actual visible area in map coordinates.
    Vec2d topLeft, bottomRight, topRight, bottomLeft;

    // Axis-aligned bounding box of the potentially visible area
    // (rotation-aware) in map coordinates.
    coord_t viewAABB[4];

    // Misc:
    List<MarkedPoint *> points;  ///< Player-marked points of interest.
    dint followPlayer = 0;       ///< Player being followed.

    Impl(Public *i) : Base(i)
    {
        using namespace de;

        zap(bounds);
        zap(viewAABB);

        auto &ds = ScriptSystem::get();

        if (ds.nativeModuleExists("DisplayMode"))
        {
            pixelRatio = ds["DisplayMode"].getf("PIXEL_RATIO");
        }
    }

    ~Impl()
    {
        clearPoints();
    }

    void clearPoints()
    {
        deleteAll(points); points.clear();
    }

    void setMinScale(dfloat newMinScale)
    {
        minScale = de::max(1.f, newMinScale);
        needViewScaleUpdate = true;
    }

    /**
     * Calculate the min/max scaling factors.
     *
     * Take the distance from the bottom left to the top right corners and choose a max scaling
     * factor such that this distance is short than both the automap window width and height.
     */
    void updateViewScale()
    {
        const float oldMinScale = minScaleMTOF;

        Vec2d const topRight  (bounds[BOXRIGHT], bounds[BOXTOP   ]);
        Vec2d const bottomLeft(bounds[BOXLEFT ], bounds[BOXBOTTOM]);
        const coord_t dist = de::abs((topRight - bottomLeft).length());

        Vec2f const dimensions(Rect_Width (&self().geometry()),
                                  Rect_Height(&self().geometry()));
        const Vec2f scale = dimensions / dist;

        minScaleMTOF = (scale.x < scale.y ? scale.x : scale.y);
        maxScaleMTOF = dimensions.y / minScale;

        LOG_AS("AutomapWidget");
        LOGDEV_XVERBOSE("updateViewScale: delta:%s dimensions:%s dist:%f scale:%s minmtof:%f",
                   (topRight - bottomLeft).asText()
                << dimensions.asText() << dist
                << scale.asText() << minScaleMTOF);

        // Update previously set view scale accordingly.
        /// @todo  The view scale factor needs to be resolution independent!
        targetViewScale = viewScale = minScaleMTOF / oldMinScale * targetViewScale;
        needViewScaleUpdate = false;
    }

    static void drawLine2(const Vec2d &from, const Vec2d &to,
        const Vec3f &color, dfloat opacity, glowtype_t glowType, dfloat glowStrength,
        dfloat glowSize, dd_bool glowOnly, dd_bool scaleGlowWithView, dd_bool caps,
        /*blendmode_t blend, */dd_bool drawNormal)
    {
        opacity *= uiRendState->pageAlpha;

        const Vec2d unit = (to - from).normalize();
        Vec2d const normal(unit.y, -unit.x);

        if (de::abs(unit.length()) <= 0) return;

        //DGL_BlendMode(blend);

        // Is this a glowing line?
        if (glowOnly && glowType != GLOW_NONE)
        {
            //const dint tex = Get(DD_DYNLIGHT_TEXTURE);

            // Scale line thickness relative to zoom level?
            dfloat thickness;
            if (scaleGlowWithView)
                thickness = cfg.common.automapDoorGlow * 2.5f + 3;
            else
                thickness = glowSize;

            DE_ASSERT(rs.primType == DGL_QUADS);

            // Draw a "cap" at the start of the line?
            if (caps)
            {
                const Vec2f v1 = from -   unit * thickness + normal * thickness;
                const Vec2f v2 = from + normal * thickness;
                const Vec2f v3 = from - normal * thickness;
                const Vec2f v4 = from -   unit * thickness - normal * thickness;

                //if (!addToLists)
                {
                    //DGL_Bind(tex);

                    DGL_Color4f(color.x, color.y, color.z, glowStrength * opacity);
                    //DGL_BlendMode(blend);
                }

//                DGL_Begin(DGL_QUADS);
                DGL_TexCoord2f(0, 0, 0);
                DGL_TexCoord2f(1, v1.x, v1.y);
                DGL_Vertex2f(v1.x, v1.y);

                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v2.x, v2.y);
                DGL_Vertex2f(v2.x, v2.y);

                DGL_TexCoord2f(0, .5f, 1);
                DGL_TexCoord2f(1, v3.x, v3.y);
                DGL_Vertex2f(v3.x, v3.y);

                DGL_TexCoord2f(0, 0, 1);
                DGL_TexCoord2f(1, v4.x, v4.y);
                DGL_Vertex2f(v4.x, v4.y);
//                DGL_End();

                //if (!addToLists)
                    //DGL_BlendMode(BM_NORMAL);
            }

            // The middle part of the line.
            switch (glowType)
            {
            case GLOW_BOTH: {
                const Vec2f v1 = from + normal * thickness;
                const Vec2f v2 =   to + normal * thickness;
                const Vec2f v3 =   to - normal * thickness;
                const Vec2f v4 = from - normal * thickness;

                //if (!addToLists)
                {
                    //DGL_Bind(tex);

                    DGL_Color4f(color.x, color.y, color.z, glowStrength * opacity);
                    //DGL_BlendMode(blend);
                }

//                DGL_Begin(DGL_QUADS);
                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v1.x, v1.y);
                DGL_Vertex2f(v1.x, v1.y);

                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v2.x, v2.y);
                DGL_Vertex2f(v2.x, v2.y);

                DGL_TexCoord2f(0, .5f, 1);
                DGL_TexCoord2f(1, v3.x, v3.y);
                DGL_Vertex2f(v3.x, v3.y);

                DGL_TexCoord2f(0, .5f, 1);
                DGL_TexCoord2f(1, v4.x, v4.y);
                DGL_Vertex2f(v4.x, v4.y);
//                DGL_End();

                //if (!addToLists)
                    //DGL_BlendMode(BM_NORMAL);
                break; }

            case GLOW_BACK: {
                const Vec2f v1 = from + normal * thickness;
                const Vec2f v2 =   to + normal * thickness;

                //if (!addToLists)
                {
                    //DGL_Bind(tex);

                    DGL_Color4f(color.x, color.y, color.z, glowStrength * opacity);
//                    DGL_BlendMode(blend);
                }

//                DGL_Begin(DGL_QUADS);
                DGL_TexCoord2f(0, 0, .25f);
                DGL_TexCoord2f(1, v1.x, v1.y);
                DGL_Vertex2f(v1.x, v1.y);

                DGL_TexCoord2f(0, 0, .25f);
                DGL_TexCoord2f(1, v2.x, v2.y);
                DGL_Vertex2f(v2.x, v2.y);

                DGL_TexCoord2f(0, .5f, .25f);
                DGL_TexCoord2f(1, to.x, to.y);
                DGL_Vertex2f(to.x, to.y);

                DGL_TexCoord2f(0, .5f, .25f);
                DGL_TexCoord2f(1, from.x, from.y);
                DGL_Vertex2f(from.x, from.y);
//                DGL_End();

                //if (!addToLists)
//                    DGL_BlendMode(BM_NORMAL);
                break; }

            case GLOW_FRONT: {
                const Vec2f v3 =   to - normal * thickness;
                const Vec2f v4 = from - normal * thickness;

                //if (!addToLists)
                {
                    //DGL_Bind(tex);

                    DGL_Color4f(color.x, color.y, color.z, glowStrength * opacity);
//                    DGL_BlendMode(blend);
                }

//                DGL_Begin(DGL_QUADS);
                DGL_TexCoord2f(0, .75f, .5f);
                DGL_TexCoord2f(1, from.x, from.y);
                DGL_Vertex2f(from.x, from.y);

                DGL_TexCoord2f(0, .75f, .5f);
                DGL_TexCoord2f(1, to.x, to.y);
                DGL_Vertex2f(to.x, to.y);

                DGL_TexCoord2f(0, .75f, 1);
                DGL_TexCoord2f(1, v3.x, v3.y);
                DGL_Vertex2f(v3.x, v3.y);

                DGL_TexCoord2f(0, .75f, 1);
                DGL_TexCoord2f(1, v4.x, v4.y);
                DGL_Vertex2f(v4.x, v4.y);
//                DGL_End();

                //if (!addToLists)
//                    DGL_BlendMode(BM_NORMAL);
                break; }

            default:
                DE_ASSERT_FAIL("Unknown glowtype");
                break;
            }

            if (caps)
            {
                const Vec2f v1 = to + normal * thickness;
                const Vec2f v2 = to +   unit * thickness + normal * thickness;
                const Vec2f v3 = to +   unit * thickness - normal * thickness;
                const Vec2f v4 = to - normal * thickness;

                //if (!addToLists)
                {
                    //DGL_Bind(tex);

                    DGL_Color4f(color.x, color.y, color.z, glowStrength * opacity);
//                    DGL_BlendMode(blend);
                }

//                DGL_Begin(DGL_QUADS);
                DGL_TexCoord2f(0, .5f, 0);
                DGL_TexCoord2f(1, v1.x, v1.y);
                DGL_Vertex2f(v1.x, v1.y);

                DGL_TexCoord2f(0, 1, 0);
                DGL_TexCoord2f(1, v2.x, v2.y);
                DGL_Vertex2f(v2.x, v2.y);

                DGL_TexCoord2f(0, 1, 1);
                DGL_TexCoord2f(1, v3.x, v3.y);
                DGL_Vertex2f(v3.x, v3.y);

                DGL_TexCoord2f(0, .5, 1);
                DGL_TexCoord2f(1, v4.x, v4.y);
                DGL_Vertex2f(v4.x, v4.y);
//                DGL_End();

                //if (!addToLists)
//                    DGL_BlendMode(BM_NORMAL);
            }
        }
        else if (!glowOnly)
        {
            DE_ASSERT(rs.primType == DGL_LINES);
            //if (!addToLists)
            {
                DGL_Color4f(color.x, color.y, color.z, opacity);
//                DGL_BlendMode(blend);
            }

            //DGL_Begin(DGL_LINES);
            DGL_TexCoord2f(0, from[0], from[1]);
            DGL_Vertex2f(from[0], from[1]);
            DGL_TexCoord2f(0, to[0], to[1]);
            DGL_Vertex2f(to[0], to[1]);
            //DGL_End();

            //if (!addToLists)
//                DGL_BlendMode(BM_NORMAL);
            if (drawNormal)
            {
#define NORMTAIL_LENGTH         8

                const Vec2f v1 = (from + to) / 2;
                const Vec2d v2 = v1 + normal * NORMTAIL_LENGTH;

                //if (!addToLists)
                {
                    DGL_Color4f(color.x, color.y, color.z, opacity);
                    //                DGL_BlendMode(blend);
                }

//                DGL_Begin(DGL_LINES);
                DGL_TexCoord2f(0, v1.x, v1.y);
                DGL_Vertex2f(v1.x, v1.y);

                DGL_TexCoord2f(0, v2.x, v2.y);
                DGL_Vertex2f(v2.x, v2.y);
//                DGL_End();

                //if (!addToLists)
                //                DGL_BlendMode(BM_NORMAL);

#undef NORMTAIL_LENGTH
            }
        }

            //DGL_BlendMode(BM_NORMAL);
    }

    void drawLine(Line *line) const
    {
        DE_ASSERT(line);

        xline_t *xline = P_ToXLine(line);

        // Already drawn once?
        if (xline->validCount == VALIDCOUNT)
            return;

        // Is this line being drawn?
        if ((xline->flags & ML_DONTDRAW) && !(flags & AWF_SHOW_ALLLINES))
            return;

        // We only want to draw twosided lines once.
        auto *frontSector = (Sector *)P_GetPtrp(line, DMU_FRONT_SECTOR);
        if (frontSector && frontSector != (Sector *)P_GetPtrp(line, DMU_FRONT_SECTOR))
        {
            return;
        }

        const automapcfg_lineinfo_t *info = nullptr;
        if ((flags & AWF_SHOW_ALLLINES) || xline->mapped[rs.plr - players])
        {
            auto *backSector = reinterpret_cast<Sector *>(P_GetPtrp(line, DMU_BACK_SECTOR));

            // Perhaps this is a specially colored line?
            info = style->tryFindLineInfo_special(xline->special, xline->flags,
                                                  frontSector, backSector, flags);
            if (rs.obType != -1 && !info)
            {
                // Perhaps a default colored line?
                /// @todo Implement an option which changes the vanilla behavior of always
                ///       coloring non-secret lines with the solid-wall color to instead
                ///       use whichever color it would be if not flagged secret.
                if (!backSector || !P_GetPtrp(line, DMU_BACK) || (xline->flags & ML_SECRET))
                {
                    // solid wall (well probably anyway...)
                    info = style->tryFindLineInfo(AMO_SINGLESIDEDLINE);
                }
                else
                {
                    if (!de::fequal(P_GetDoublep(backSector,  DMU_FLOOR_HEIGHT),
                                    P_GetDoublep(frontSector, DMU_FLOOR_HEIGHT)))
                    {
                        // Floor level change.
                        info = style->tryFindLineInfo(AMO_FLOORCHANGELINE);
                    }
                    else if (!de::fequal(P_GetDoublep(backSector,  DMU_CEILING_HEIGHT),
                                         P_GetDoublep(frontSector, DMU_CEILING_HEIGHT)))
                    {
                        // Ceiling level change.
                        info = style->tryFindLineInfo(AMO_CEILINGCHANGELINE);
                    }
                    else if (flags & AWF_SHOW_ALLLINES)
                    {
                        info = style->tryFindLineInfo(AMO_UNSEENLINE);
                    }
                }
            }
        }
        else if (rs.obType != -1 && revealed)
        {
            if (!(xline->flags & ML_DONTDRAW))
            {
                // An as yet, unseen line.
                info = style->tryFindLineInfo(AMO_UNSEENLINE);
            }
        }

        if (info && (rs.obType == -1 || info == &style->lineInfo(rs.obType)))
        {
            ddouble from[2]; P_GetDoublepv(P_GetPtrp(line, DMU_VERTEX0), DMU_XY, from);
            ddouble to  [2]; P_GetDoublepv(P_GetPtrp(line, DMU_VERTEX1), DMU_XY, to);

            drawLine2(Vec2d(from), Vec2d(to), Vec3f(info->rgba), info->rgba[3],
                      (xline->special && !cfg.common.automapShowDoors ? GLOW_NONE : info->glow),
                      info->glowStrength,
                      info->glowSize, rs.glowOnly, info->scaleWithView,
                      (info->glow && !(xline->special && !cfg.common.automapShowDoors)),
                      //(xline->special && !cfg.common.automapShowDoors ? BM_NORMAL : info->blendMode),
                      (flags & AWF_SHOW_LINE_NORMALS));

            xline->validCount = VALIDCOUNT; // Mark as drawn this frame.
        }
    }

    static int drawLineWorker(void *line, void *context)
    {
        static_cast<Impl *>(context)->drawLine((Line *)line);
        return false;  // Continue iteration.
    }

    static int drawLinesForSubspaceWorker(ConvexSubspace *subspace, void *context)
    {
        return P_Iteratep(subspace, DMU_LINE, drawLineWorker, context);
    }

    /**
     * Determines visible lines, draws them.
     *
     * @params objType  Type of map object being drawn.
     */
    void drawAllLines(dint obType, bool glowOnly = false) const
    {
        // VALIDCOUNT is used to track which lines have been drawn this frame.
        VALIDCOUNT++;

        // Configure render state:
        rs.obType   = obType;
        rs.glowOnly = glowOnly;

        if (glowOnly)
        {
            rs.primType = DGL_QUADS;
            DGL_Enable(DGL_TEXTURE0);
            DGL_Bind(DGLuint(Get(DD_DYNLIGHT_TEXTURE)));
        }
        else
        {
            rs.primType = DGL_LINES;
            if (amMaskTexture)
            {
                DGL_Enable(DGL_TEXTURE0);
                DGL_Bind(amMaskTexture);
            }
        }

        DGL_Begin(rs.primType);

        // Can we use the automap's in-view bounding box to cull out of view objects?
        //if (!addToLists)
        {
            AABoxd aaBox;
            self().pvisibleBounds(&aaBox.minX, &aaBox.maxX, &aaBox.minY, &aaBox.maxY);
            Subspace_BoxIterator(&aaBox, drawLinesForSubspaceWorker, const_cast<Impl *>(this));
        }
        /*else
        {
            // No. As the map lists are considered static we want them to contain all
            // walls, not just those visible *now* (note rotation).
            const dint numSubspaces = P_Count(DMU_SUBSPACE);
            for (dint i = 0; i < numSubspaces; ++i)
            {
                P_Iteratep(P_ToPtr(DMU_SUBSPACE, i), DMU_LINE, drawLineWorker, const_cast<Impl *>(this));
            }
        }*/

        DGL_End();
        DGL_Enable(DGL_TEXTURE0);
    }

    static void drawLine(Line *line, const Vec3f &color, dfloat opacity,
                         /*blendmode_t blendMode, */bool showNormal)
    {
        dfloat length = P_GetFloatp(line, DMU_LENGTH);

        if (length > 0)
        {
            dfloat v1[2]; P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX0), DMU_XY, v1);
            dfloat v2[2]; P_GetFloatpv(P_GetPtrp(line, DMU_VERTEX1), DMU_XY, v2);

            //DGL_BlendMode(blendMode);
            DGL_Color4f(color.x, color.y, color.z, opacity);

            DE_ASSERT(rs.primType == DGL_LINES);

            //DGL_Begin(DGL_LINES);
            DGL_TexCoord2f(0, v1[0], v1[1]);
            DGL_Vertex2f(v1[0], v1[1]);

            DGL_TexCoord2f(0, v2[0], v2[1]);
            DGL_Vertex2f(v2[0], v2[1]);
            //DGL_End();

            if (showNormal)
            {
#define NORMTAIL_LENGTH         8

                dfloat d1[2]; P_GetFloatpv(line, DMU_DXY, d1);

                dfloat const unit  [] = { d1[0] / length, d1[1] / length };
                dfloat const normal[] = { unit[1], -unit[0] };

                // The center of the line.
                v1[0] += (length / 2) * unit[0];
                v1[1] += (length / 2) * unit[1];

                // Outside point.
                v2[0] = v1[0] + normal[0] * NORMTAIL_LENGTH;
                v2[1] = v1[1] + normal[1] * NORMTAIL_LENGTH;

                //DGL_Begin(DGL_LINES);
                DGL_TexCoord2f(0, v1[0], v1[1]);
                DGL_Vertex2f(v1[0], v1[1]);

                DGL_TexCoord2f(0, v2[0], v2[1]);
                DGL_Vertex2f(v2[0], v2[1]);
                //DGL_End();

#undef NORMTAIL_LENGTH
            }

            //DGL_BlendMode(BM_NORMAL);
        }
    }

    static int drawLine_polyob(Line *line, void *context)
    {
        const auto *inst = static_cast<Impl *>(context);
        DE_ASSERT(inst);

        const dfloat opacity = uiRendState->pageAlpha;

        xline_t *xline = P_ToXLine(line);
        if (!xline) return false;

        // Already processed this frame?
        if (xline->validCount == VALIDCOUNT) return false;

        if ((xline->flags & ML_DONTDRAW) && !(inst->flags & AWF_SHOW_ALLLINES))
        {
            return false;
        }

        automapcfg_objectname_t amo = AMO_NONE;
        if ((inst->flags & AWF_SHOW_ALLLINES) || xline->mapped[rs.plr - players])
        {
            amo = AMO_SINGLESIDEDLINE;
        }
        else if (rs.obType != -1 && inst->revealed)
        {
            if (!(xline->flags & ML_DONTDRAW))
            {
                // An as yet, unseen line.
                amo = AMO_UNSEENLINE;
            }
        }

        if (const automapcfg_lineinfo_t *info = inst->style->tryFindLineInfo(amo))
        {
            drawLine(line, Vec3f(info->rgba), info->rgba[3] * cfg.common.automapLineAlpha * opacity,
                     /*info->blendMode, */(inst->flags & AWF_SHOW_LINE_NORMALS));
        }

        xline->validCount = VALIDCOUNT;  // Mark as processed this frame.

        return false;  // Continue iteration.
    }

    void drawAllPolyobs() const
    {
        VALIDCOUNT++;  // Used to track which lines have been drawn this frame.

        // Configure render state:
        rs.obType = MOL_LINEDEF;
        rs.primType = DGL_LINES;

        DGL_Begin(rs.primType);
        {
            // Draw any polyobjects in view.
            AABoxd aaBox;
            self().pvisibleBounds(&aaBox.minX, &aaBox.maxX, &aaBox.minY, &aaBox.maxY);
            Line_BoxIterator(&aaBox, LIF_POLYOBJ, drawLine_polyob, const_cast<Impl *>(this));
        }
        DGL_End();
    }

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    static int drawLine_xg(Line *line, void *context)
    {
        const auto *inst = static_cast<Impl *>(context);
        DE_ASSERT(line && inst);

        xline_t *xline = P_ToXLine(line);
        if (!xline) return false;

        if (xline->validCount == VALIDCOUNT) return false;

        if (!(inst->flags & AWF_SHOW_ALLLINES))
        {
            if (xline->flags & ML_DONTDRAW) return false;
        }

        // Only active XG lines.
        if (!xline->xg || !xline->xg->active) return false;

        // XG lines blink.
        if (!(mapTime & 4)) return false;

        drawLine(line, Vec3f(.8f, 0, .8f), 1, (inst->flags & AWF_SHOW_LINE_NORMALS));
        xline->validCount = VALIDCOUNT;  // Mark as processed this frame.

        return false;  // Continue iteration.
    }
#endif

    void drawAllLines_xg() const
    {
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        if (!(flags & AWF_SHOW_SPECIALLINES))
            return;

        // VALIDCOUNT is used to track which lines have been drawn this frame.
        VALIDCOUNT++;

        // Configure render state:
        rs.glowOnly = true;
        rs.obType   = -1;
        rs.primType = DGL_LINES;

        DGL_BlendMode(BM_ADD);
        DGL_Begin(rs.primType);
        {
            AABoxd aaBox;
            self().pvisibleBounds(&aaBox.minX, &aaBox.maxX, &aaBox.minY, &aaBox.maxY);
            Line_BoxIterator(&aaBox, LIF_SECTOR, drawLine_xg, const_cast<Impl *>(this));
        }
        DGL_End();
        DGL_BlendMode(BM_NORMAL);
#endif
    }

    /**
     * Visualize all players on the map with SVG markers.
     */
    void drawAllPlayerMarkers() const
    {
        for (dint i = 0; i < MAXPLAYERS; ++i)
        {
            // Do not show markers for other players in deathmatch.
            if (gfw_Rule(deathmatch) && i != self().player())
            {
                continue;
            }

            drawPlayerMarker(i, style);
        }
    }

    static dint thingColorForMobjType(mobjtype_t type)
    {
#if __JHEXEN__
        DE_UNUSED(type);
        return -1;
#else
        struct ThingData { mobjtype_t type; dint palColor; } static const thingData[] = {
#  if __JDOOM__ || __JDOOM64__
            { MT_MISC4, KEY1_COLOR },
            { MT_MISC5, KEY2_COLOR },
            { MT_MISC6, KEY3_COLOR },
            { MT_MISC7, KEY4_COLOR },
            { MT_MISC8, KEY5_COLOR },
            { MT_MISC9, KEY6_COLOR },
#  elif __JHERETIC__
            { MT_CKEY,  KEY1_COLOR },
            { MT_BKYY,  KEY2_COLOR },
            { MT_AKYY,  KEY3_COLOR },
#  endif
        };
        for (const auto &thing : thingData)
        {
            if (thing.type == type) return thing.palColor;
        }
        return -1;  // None.
#endif
    }

    struct drawthingpoint_params_t
    {
        dint flags;      ///< AWF_* flags.
        svgid_t vgId;
        dfloat rgb[3];
        dfloat opacity;
    };

    static int drawThingPoint(mobj_t *mob, void *context)
    {
        auto *p = reinterpret_cast<drawthingpoint_params_t *>(context);

        // Only sector linked mobjs should be visible in the automap.
        if (!(mob->flags & MF_NOSECTOR))
        {
            svgid_t vgId   = p->vgId;
            bool isVisible = false;
            dfloat *color  = p->rgb;

            dfloat angle = 0;
            dfloat keyColorRGB[3];
            if (p->flags & AWF_SHOW_KEYS)
            {
                dint keyColor = thingColorForMobjType(mobjtype_t(mob->type));
                if (keyColor != -1)
                {
                    R_GetColorPaletteRGBf(0, keyColor, keyColorRGB, false);
                    vgId      = VG_KEY;
                    color     = keyColorRGB;
                    isVisible = true;
                }
            }

            // Something else?
            if (!isVisible)
            {
                isVisible = !!(p->flags & AWF_SHOW_THINGS);
                angle = mob->angle /*Mobj_AngleSmoothed(mob)*/ / float(ANGLE_MAX) * 360;  // In degrees.
            }

            if (isVisible)
            {
                /* $unifiedangles */
                coord_t origin[3]; //Mobj_OriginSmoothed(mob, origin);
                memcpy(origin, mob->origin, sizeof(origin));

                drawVectorGraphic(vgId, Vec2d(origin), angle, 16 /*radius*/,
                                  Vec3f(color), p->opacity, BM_NORMAL);
            }
        }

        return false; // Continue iteration.
    }

    void drawAllThings() const
    {
        if (!(flags & (AWF_SHOW_THINGS | AWF_SHOW_KEYS)))
            return;

        const dfloat alpha = uiRendState->pageAlpha;

        drawthingpoint_params_t parm; de::zap(parm);
        parm.flags   = flags;
        parm.vgId    = style->objectSvg(AMO_THING);
        AM_GetMapColor(parm.rgb, cfg.common.automapMobj, THINGCOLORS, customPal);
        parm.opacity = de::clamp(0.f, cfg.common.automapLineAlpha * alpha, 1.f);

        AABoxd aaBox;
        self().pvisibleBounds(&aaBox.minX, &aaBox.maxX, &aaBox.minY, &aaBox.maxY);

        VALIDCOUNT++;
        Mobj_BoxIterator(&aaBox, drawThingPoint, &parm);
    }

    void drawAllPoints(dfloat scale = 1) const
    {
        const dfloat alpha = uiRendState->pageAlpha;

        if (points.isEmpty()) return;

        // Calculate final scale factor.
        scale = self().frameToMap(1) * scale;
#if __JHERETIC__ || __JHEXEN__
        // These games use a larger font, so use a smaller scale.
        scale *= .5f;
#endif

        dint idx = 0;
        const Point2Raw labelOffset{};
        for (const MarkedPoint *point : points)
        {
            const String label    = String::asText(idx++);
            const Vec2d origin = fitPointInRectangle(point->origin(), topLeft, topRight, bottomRight, bottomLeft, view);

            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PushMatrix();
            DGL_Translatef(origin.x, origin.y, 0);
            DGL_Scalef(scale, scale, 1);
            DGL_Rotatef(angle, 0, 0, 1);
            DGL_Scalef(1, -1, 1);
            DGL_Enable(DGL_TEXTURE_2D);

            FR_SetFont(FID(GF_MAPPOINT));
#if __JDOOM__
            if (gameMode == doom2_hacx)
                FR_SetColorAndAlpha(1, 1, 1, alpha);
            else
                FR_SetColorAndAlpha(.22f, .22f, .22f, alpha);
#else
            FR_SetColorAndAlpha(1, 1, 1, alpha);
#endif
            FR_DrawText3(label, &labelOffset, 0, DTF_ONLY_SHADOW);

            DGL_Disable(DGL_TEXTURE_2D);
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PopMatrix();
        }
    }

    /**
     * Sets up the state for automap drawing.
     */
    void setupGLStateForMap() const
    {
        const dfloat alpha = uiRendState->pageAlpha;

        // Store the old scissor state (to clip the map lines and stuff).
        DGL_PushState();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();

        dfloat bgColor[3];
#if __JHERETIC__ || __JHEXEN__
        if (CentralLumpIndex().contains("AUTOPAGE.lmp"))
        {
            bgColor[0] = bgColor[1] = bgColor[2] = 1.f; // use lump colors as-is
        }
        else
        {
            // Automap background lump is missing.
            bgColor[0] = .55f; bgColor[1] = .45f; bgColor[2] = .35f;
        }
#else
        AM_GetMapColor(bgColor, cfg.common.automapBack, BACKGROUND, customPal);
#endif

        RectRaw geom; Rect_Raw(&self().geometry(), &geom);

        // Draw the AUTOPAGE background image (if available).
        if (autopageLumpNum != -1)
        {
            const float autopageWidth       = 320; /// @todo Could be external with different size.
            const float autopageHeight      = 200;
            const float autopageAspectRatio = autopageWidth / autopageHeight;

            const float texScale    = 1.f / 3000.f;
            const float bgScale     = texScale / scaleMTOF;
            const float offsetScale = texScale * autopageAspectRatio;

            // The autopage texture is transformed in texture coordinate space. It is drawn as
            // a single quad covering the entire widget.

            DGL_Enable(DGL_TEXTURE_2D);

            DGL_MatrixMode(DGL_TEXTURE);
            DGL_PushMatrix();
            DGL_LoadIdentity();

            DGL_SetRawImage(autopageLumpNum, DGL_REPEAT, DGL_REPEAT);
            DGL_Color4f(bgColor[0], bgColor[1], bgColor[2], cfg.common.automapOpacity * alpha);

#if 0
            DGL_Translatef(geom.origin.x, geom.origin.y, 0);

            // Apply the parallax scrolling, map rotation and counteract the
            // aspect of the quad (sized to map window dimensions).
            DGL_Translatef(self().mapToFrame(viewPL.x) + .5f,
                           self().mapToFrame(viewPL.y) + .5f, 0);
            DGL_Scalef(1, 1.2f/*aspect correct*/, 1);
            DGL_Rotatef(360 - self().cameraAngle(), 0, 0, 1);
            DGL_Translatef(-(.5f), -(.5f), 0);
#endif

            DE_ASSERT(!std::isnan(view.x));

            DGL_Translatef(offsetScale * view.x, -offsetScale * view.y, 1.f);
            DGL_Scalef(autopageAspectRatio, autopageAspectRatio, 1.f);
            DGL_Rotatef(360.f - self().cameraAngle(), 0, 0, 1);
            DGL_Scalef(bgScale * float(geom.size.width), bgScale * float(geom.size.height), 1.f);
            DGL_Translatef(-.5f, -.5f, 0); // center
            DGL_DrawRectf2(geom.origin.x, geom.origin.y, geom.size.width, geom.size.height);

            DGL_MatrixMode(DGL_TEXTURE);
            DGL_PopMatrix();

            DGL_Disable(DGL_TEXTURE_2D);
        }
        else
        {
            // Nope just a solid color.
            DGL_SetNoMaterial();
            DGL_Color4f(bgColor[0], bgColor[1], bgColor[2], cfg.common.automapOpacity * alpha);
            DGL_DrawRectf2(0, 0, geom.size.width, geom.size.height);
        }

#if __JDOOM64__
        // jd64 > Demon keys
        // If drawn in HUD we don't need them visible in the map too.
        if (!cfg.hudShown[HUD_INVENTORY])
        {
            static inventoryitemtype_t const items[3] = {
                IIT_DEMONKEY1, IIT_DEMONKEY2, IIT_DEMONKEY3
            };

            dint player = self().player();
            dint num = 0;
            for (const inventoryitemtype_t &item : items)
            {
                if (P_InventoryCount(player, item) > 0)
                    num += 1;
            }

            if (num > 0)
            {
                static dint const invItemSprites[NUM_INVENTORYITEM_TYPES] = {
                    SPR_ART1, SPR_ART2, SPR_ART3
                };

                const dfloat iconOpacity = de::clamp(.0f, alpha, .5f);
                const dfloat spacing     = geom.size.height / num;

                spriteinfo_t sprInfo;
                dfloat y = 0;
                for (dint i = 0; i < 3; ++i)
                {
                    if (P_InventoryCount(player, items[i]))
                    {
                        R_GetSpriteInfo(invItemSprites[i], 0, &sprInfo);
                        DGL_SetPSprite(sprInfo.material);
                        DGL_Enable(DGL_TEXTURE_2D);

                        const dfloat scale = geom.size.height / (sprInfo.geometry.size.height * num);
                        const dfloat x     = geom.size.width - sprInfo.geometry.size.width * scale;
                        const dfloat w     = sprInfo.geometry.size.width;
                        const dfloat h     = sprInfo.geometry.size.height;

                        DGL_Color4f(1, 1, 1, iconOpacity);
                        DGL_Begin(DGL_QUADS);
                            DGL_TexCoord2f(0, 0, 0);
                            DGL_Vertex2f(x, y);

                            DGL_TexCoord2f(0, sprInfo.texCoord[0], 0);
                            DGL_Vertex2f(x + w * scale, y);

                            DGL_TexCoord2f(0, sprInfo.texCoord[0], sprInfo.texCoord[1]);
                            DGL_Vertex2f(x + w * scale, y + h * scale);

                            DGL_TexCoord2f(0, 0, sprInfo.texCoord[1]);
                            DGL_Vertex2f(x, y + h * scale);
                        DGL_End();

                        DGL_Disable(DGL_TEXTURE_2D);

                        y += spacing;
                    }
                }
            }
        }
        // < d64tc
#endif

        // Setup the scissor clipper.
        /// @todo Do this in the UI module.
        const dint border = .5f + UIAUTOMAP_BORDER * aspectScale;
        RectRaw clipRegion; Rect_Raw(&self().geometry(), &clipRegion);
        clipRegion.origin.x += border;
        clipRegion.origin.y += border;
        clipRegion.size.width  -= 2 * border;
        clipRegion.size.height -= 2 * border;

        DGL_SetScissor(&clipRegion);
    }

    /**
     * Restores the previous GL draw state
     */
    void restoreGLStateFromMap()
    {
        DGL_PopState();
    }

    void drawAllVertexes()
    {
        if (!(flags & AWF_SHOW_VERTEXES))
            return;

        DGL_Color4f(.2f, .5f, 1, uiRendState->pageAlpha);

        DGL_Enable(DGL_POINT_SMOOTH);
        const dfloat oldPointSize = DGL_GetFloat(DGL_POINT_SIZE);
        DGL_SetFloat(DGL_POINT_SIZE, 4 * aspectScale);

        dfloat v[2];
        DGL_Begin(DGL_POINTS);
        for (dint i = 0; i < numvertexes; ++i)
        {
            P_GetFloatv(DMU_VERTEX, i, DMU_XY, v);
            DGL_TexCoord2f(0, v[0], v[1]);
            DGL_Vertex2f(v[0], v[1]);
        }
        DGL_End();

        DGL_SetFloat(DGL_POINT_SIZE, oldPointSize);
        DGL_Disable(DGL_POINT_SMOOTH);
    }
};

AutomapWidget::AutomapWidget(dint player)
    : HudWidget(function_cast<UpdateGeometryFunc>(AutomapWidget_UpdateGeometry),
                function_cast<DrawFunc>(AutomapWidget_Draw),
                player)
    , d(new Impl(this))
{
    d->style = ST_AutomapStyle();
}

AutomapWidget::~AutomapWidget()
{}

dint AutomapWidget::cameraFollowPlayer() const
{
    return d->followPlayer;
}

void AutomapWidget::setCameraFollowPlayer(dint newPlayer)
{
    d->followPlayer = newPlayer;
}

void AutomapWidget::prepareAssets()  // static
{
    const LumpIndex &lumpIndex = CentralLumpIndex();

    if (autopageLumpNum >= 0)
    {
        autopageLumpNum = lumpIndex.findLast("autopage.lmp");
    }
    if (!amMaskTexture)
    {
        lumpnum_t lumpNum = lumpIndex.findLast("mapmask.lmp");
        if (lumpNum >= 0)
        {
            File1 &file = lumpIndex[lumpNum];
            const uint8_t *pixels = file.cache();

            amMaskTexture = DGL_NewTextureWithParams(DGL_LUMINANCE, 256/*width*/, 256/*height*/,
                                                     pixels, 0x8, DGL_NEAREST, DGL_LINEAR,
                                                     0 /*no anisotropy*/, DGL_REPEAT, DGL_REPEAT);

            file.unlock();
        }
    }
}

void AutomapWidget::releaseAssets()  // static
{
    if (!amMaskTexture) return;
    DGL_DeleteTextures(1, &amMaskTexture);
    amMaskTexture = 0;
}

void AutomapWidget::reset()
{
    d->needBuildLists = true;
    d->rotate         = cfg.common.automapRotate;
}

void AutomapWidget::lineAutomapVisibilityChanged(const Line &)
{
    d->needBuildLists = true;
}

AutomapStyle *AutomapWidget::style() const
{
    return d->style;
}

void AutomapWidget::draw(const Vec2i &offset) const
{
    const float alpha = uiRendState->pageAlpha;
    player_t *plr = &players[player()];

    if (!plr->plr->inGame) return;

    // Configure render state:
    rs.plr = plr;
    const Vec2d viewPoint = cameraOrigin();
    float angle = cameraAngle();
    RectRaw geom; Rect_Raw(&geometry(), &geom);

    // Freeze the lists if the map is fading out from being open, or for debug.
//    static int updateWait = 0;  /// @todo should be an instance var of AutomapWidget
//    if ((++updateWait % 10) && d->needBuildLists && !freezeMapRLs && isOpen())
//    {
//        // Its time to rebuild the automap object display lists.
//        d->buildLists();
//    }

    // Setup for frame.
    d->setupGLStateForMap();

    // Configure the modelview matrix so that we can draw geometry for world
    // objects using their world-space coordinates directly.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(offset.x, offset.y, 0);
    DGL_Translatef(geom.size.width / 2, geom.size.height / 2, 0);
    DGL_Rotatef(angle, 0, 0, 1);
    DGL_Scalef(1, -1, 1); // In the world coordinate space Y+ is up.
    DGL_Scalef(d->scaleMTOF, d->scaleMTOF, 1);
    DGL_Translatef(-viewPoint.x, -viewPoint.y, 0);

    const float oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
    DGL_SetFloat(DGL_LINE_WIDTH, d->pixelRatio * de::clamp(.5f, cfg.common.automapLineWidth, 8.f));

    /*#if _DEBUG
    // Draw the rectangle described by the visible bounds.
    {
        coord_t topLeft[2], bottomRight[2], topRight[2], bottomLeft[2];
        d->visibleBounds(topLeft, bottomRight, topRight, bottomLeft);
        DGL_Color4f(1, 1, 1, alpha);
        DGL_Begin(DGL_LINES);
            DGL_Vertex2f(    topLeft[0],     topLeft[1]);
            DGL_Vertex2f(   topRight[0],    topRight[1]);
            DGL_Vertex2f(   topRight[0],    topRight[1]);
            DGL_Vertex2f(bottomRight[0], bottomRight[1]);
            DGL_Vertex2f(bottomRight[0], bottomRight[1]);
            DGL_Vertex2f( bottomLeft[0],  bottomLeft[1]);
            DGL_Vertex2f( bottomLeft[0],  bottomLeft[1]);
            DGL_Vertex2f(    topLeft[0],     topLeft[1]);
        DGL_End();
    }
#endif*/

    if (amMaskTexture)
    {
        const dint border = .5f + UIAUTOMAP_BORDER * aspectScale;

        DGL_SetInteger(DGL_ACTIVE_TEXTURE, 0);
        DGL_MatrixMode(DGL_TEXTURE);
        DGL_LoadIdentity();

        DGL_PushMatrix();
        DGL_Scalef(1.f / (geom.size.width  - border*2),
                   1.f / (geom.size.height - border*2), 1);
        DGL_Translatef(geom.size.width  /2 - border,
                       geom.size.height /2 - border, 0);
        DGL_Rotatef(-angle, 0, 0, 1);
        DGL_Scalef(d->scaleMTOF, d->scaleMTOF, 1);
        DGL_Translatef(-viewPoint.x, -viewPoint.y, 0);
    }

    // Draw static map geometry.
    for (dint i = NUM_MAP_OBJECTLISTS-1; i >= 0; i--)
    {
        const automapcfg_lineinfo_t &info = d->style->lineInfo(i);
        DGL_Color4f(info.rgba[0], info.rgba[1], info.rgba[2], info.rgba[3] * cfg.common.automapLineAlpha * alpha);
        d->drawAllLines(i);
    }

    // Draw dynamic map geometry.
    d->drawAllLines_xg();
    d->drawAllPolyobs();

    // Restore the previous state.
    DGL_BlendMode(BM_NORMAL);
    DGL_Color4f(1, 1, 1, 1);

    d->drawAllVertexes();
    d->drawAllThings();

    // Sharp player markers.
    DGL_SetFloat(DGL_LINE_WIDTH, 1.f);
    d->drawAllPlayerMarkers();
    DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);

    if (amMaskTexture)
    {
        DGL_Disable(DGL_TEXTURE0);
        DGL_MatrixMode(DGL_TEXTURE);
        DGL_PopMatrix();
    }

    // Draw glows?
    if (cfg.common.automapShowDoors)
    {
        d->drawAllLines(-1, true /*only glows*/);
    }

    d->restoreGLStateFromMap();

    d->drawAllPoints(aspectScale);

    // Return to the normal GL state.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void AutomapWidget::open(bool yes, bool instantly)
{
    if (G_GameState() != GS_MAP && yes) return;

    if (d->open == yes) return;  // No change.

    d->targetOpacity = (yes? 1.f : 0.f);
    if (instantly)
    {
        d->opacity = d->oldOpacity = d->targetOpacity;
    }
    else
    {
        // Reset the timer.
        d->oldOpacity   = d->opacity;
        d->opacityTimer = 0.f;
    }

    d->open = yes;
    if (d->open)
    {
        if (mobj_t *mob = followMobj())
        {
            // The map's target player is available.
            if (d->follow || cfg.common.automapPanResetOnOpen)
            {
                setCameraOrigin(Vec2d(mob->origin));
            }

            if (!d->follow && cfg.common.automapPanResetOnOpen)
            {
                /* $unifiedangles */
                setCameraAngle((d->rotate ? (mob->angle - ANGLE_90) / (float) ANGLE_MAX * 360 : 0));
            }
        }
        else
        {
            // Set viewer target to the center of the map.
            coord_t aabb[4];
            pvisibleBounds(&aabb[BOXLEFT], &aabb[BOXRIGHT], &aabb[BOXBOTTOM], &aabb[BOXTOP]);
            setCameraOrigin(Vec2d(aabb[BOXRIGHT] - aabb[BOXLEFT], aabb[BOXTOP] - aabb[BOXBOTTOM]) / 2);
            setCameraAngle(0);
        }
    }

    if (d->open)
    {
        DD_Execute(true, "activatebcontext map");
        if (!d->follow)
            DD_Execute(true, "activatebcontext map-freepan");
    }
    else
    {
        DD_Execute(true, "deactivatebcontext map");
        DD_Execute(true, "deactivatebcontext map-freepan");
    }
}

void AutomapWidget::tick(timespan_t elapsed)
{
    const dint plrNum = player();
    mobj_t *followMob = followMobj();

    // Check the state of the controls. Done here so that offsets don't accumulate
    // unnecessarily, as they would, if left unread.
    dfloat panX[2]; P_GetControlState(plrNum, CTL_MAP_PAN_X, &panX[0], &panX[1]);
    dfloat panY[2]; P_GetControlState(plrNum, CTL_MAP_PAN_Y, &panY[0], &panY[1]);

    if (G_GameState() != GS_MAP) return;

    // Move towards the target alpha level for the automap.
    if (cfg.common.automapOpenSeconds >= .001f)
    {
        d->opacityTimer += 1.f / cfg.common.automapOpenSeconds * elapsed;
    }
    else
    {
        d->opacityTimer = 1.f; // Instant.
    }

    if (d->opacityTimer >= 1)
    {
        d->opacity = d->targetOpacity;
    }
    else
    {
        d->opacity = de::lerp(d->oldOpacity, d->targetOpacity, d->opacityTimer);
    }

    // Unless open we do nothing further.
    if (!isOpen()) return;

    // Map view zoom contol.
    dfloat zoomSpeed = 1 + (2 * cfg.common.automapZoomSpeed) * elapsed * TICRATE;
    if (players[plrNum].brain.speed)
    {
        zoomSpeed *= 1.5f;
    }

    dfloat zoomVel;
    P_GetControlState(plrNum, CTL_MAP_ZOOM, &zoomVel, nullptr); // ignores rel offset -jk
    if (zoomVel > 0) // zoom in
    {
        setScale(d->viewScale * zoomSpeed);
    }
    else if (zoomVel < 0) // zoom out
    {
        setScale(d->viewScale / zoomSpeed);
    }

    if (!d->follow || !followMob)
    {
        // Camera panning mode.
        dfloat panUnitsPerSecond;

        // DOOM.EXE pans the automap at 140 fixed pixels per second (VGA: 200 pixels tall).
        /// @todo This needs resolution-independent units. (The "frame" units are screen pixels.)
        panUnitsPerSecond = de::max(8.f,
                                    frameToMap(140 * Rect_Height(&geometry()) / 200.f) *
                                        (2 * cfg.common.automapPanSpeed));

        /// @todo Fix sensitivity for relative axes.
        const Vec2d delta = rotate(Vec2d(panX[0], panY[0]) * panUnitsPerSecond * elapsed +
                                          Vec2d(panX[1], panY[1]),
                                      degreeToRadian(d->angle));
        moveCameraOrigin(delta, true /*instant move*/);
    }
    else
    {
        // Camera follow mode.
        const dfloat angle = (d->rotate ? (followMob->angle - ANGLE_90) / (dfloat) ANGLE_MAX * 360
                                        : 0); /* $unifiedangles */
        //coord_t origin[3]; //Mobj_OriginSmoothed(followMob, origin);
        setCameraOrigin(Vec2d(followMob->origin));
        setCameraAngle(angle);
    }

    if (d->needViewScaleUpdate)
        d->updateViewScale();

    // Map viewer location.
    d->viewTimer += dfloat(.4 * elapsed * TICRATE);
    if (d->viewTimer >= 1)
    {
        d->view = d->targetView;
    }
    else
    {
        d->view = de::lerp(d->oldView, d->targetView, d->viewTimer);
    }

    // Move the parallax layer.
//    d->viewPL = d->view / 4000;

    // Map view scale (zoom).
    d->viewScaleTimer += dfloat(.4 * elapsed * TICRATE);
    if (d->viewScaleTimer >= 1)
    {
        d->viewScale = d->targetViewScale;
    }
    else
    {
        d->viewScale = de::lerp(d->oldViewScale, d->targetViewScale, d->viewScaleTimer);
    }

    // Map view rotation.
    d->angleTimer += dfloat(.4 * elapsed * TICRATE);
    if (d->angleTimer >= 1)
    {
        d->angle = d->targetAngle;
    }
    else
    {
        dfloat startAngle = d->oldAngle;
        dfloat endAngle   = d->targetAngle;

        dfloat diff;
        if (endAngle > startAngle)
        {
            diff = endAngle - startAngle;
            if (diff > 180)
                endAngle = startAngle - (360 - diff);
        }
        else
        {
            diff = startAngle - endAngle;
            if (diff > 180)
                endAngle = startAngle + (360 - diff);
        }

        d->angle = de::lerp(startAngle, endAngle, d->angleTimer);
        if (d->angle < 0)        d->angle += 360;
        else if (d->angle > 360) d->angle -= 360;
    }

    //
    // Activate the new scale, position etc.
    //

    // Scaling multipliers.
    d->scaleMTOF = d->viewScale;
    d->scaleFTOM = 1.0f / d->scaleMTOF;

    // Calculate the coordinates of the rotated view window.
    // Determine fixed to screen space scaling factors.
    const dint border         = .5f + UIAUTOMAP_BORDER * aspectScale;

    const ddouble ang         = degreeToRadian(d->angle);
    const Vec2d origin     = cameraOrigin();

    const auto dimensions     = Vec2d(frameToMap(Rect_Width (&geometry())),
                                         frameToMap(Rect_Height(&geometry()))) / 2;

    const auto viewDimensions = Vec2d(frameToMap(Rect_Width (&geometry()) - border * 2),
                                         frameToMap(Rect_Height(&geometry()) - border * 2)) / 2;

    d->topLeft     = origin + rotate(Vec2d(-viewDimensions.x,  viewDimensions.y), ang);
    d->bottomRight = origin + rotate(Vec2d( viewDimensions.x, -viewDimensions.y), ang);
    d->bottomLeft  = origin + rotate(-viewDimensions, ang);
    d->topRight    = origin + rotate( viewDimensions, ang);


    // Calculate the in-view AABB (rotation aware).
    initAABB (d->viewAABB, rotate(-dimensions, ang));
    addToAABB(d->viewAABB, rotate(Vec2d( dimensions.x, -dimensions.y), ang));
    addToAABB(d->viewAABB, rotate(Vec2d(-dimensions.x,  dimensions.y), ang));
    addToAABB(d->viewAABB, rotate( dimensions, ang));

    // Translate to the camera origin.
    d->viewAABB[BOXLEFT  ] += origin.x;
    d->viewAABB[BOXRIGHT ] += origin.x;
    d->viewAABB[BOXTOP   ] += origin.y;
    d->viewAABB[BOXBOTTOM] += origin.y;
}

dfloat AutomapWidget::mapToFrame(dfloat coord) const
{
    return coord * d->scaleMTOF;
}

dfloat AutomapWidget::frameToMap(dfloat coord) const
{
    return coord * d->scaleFTOM;
}

void AutomapWidget::updateGeometry()
{
    // Determine whether the available space has changed and thus whether
    // the position and/or size of the automap must therefore change too.
    RectRaw newGeom;
    R_ViewWindowGeometry(player(), &newGeom);

    if (newGeom.origin.x != Rect_X(&geometry()) ||
        newGeom.origin.y != Rect_Y(&geometry()) ||
        newGeom.size.width != Rect_Width(&geometry()) ||
        newGeom.size.height != Rect_Height(&geometry()))
    {
        Rect_SetXY(&geometry(), newGeom.origin.x, newGeom.origin.y);
        Rect_SetWidthHeight(&geometry(), newGeom.size.width, newGeom.size.height);

        // Now the screen dimensions have changed we have to update scaling
        // factors accordingly.
        d->needViewScaleUpdate = true;
    }
}

dfloat AutomapWidget::cameraAngle() const
{
    return d->angle;
}

void AutomapWidget::setCameraAngle(dfloat newAngle)
{
    // Already at this target?
    newAngle = de::clamp(0.f, newAngle, 359.9999f);
    if (fequal(newAngle, d->targetAngle)) return;

    // Begin animating toward the new target.
    d->oldAngle    = d->angle;
    d->targetAngle = newAngle;
    d->angleTimer  = 0;
}

Vec2d AutomapWidget::cameraOrigin() const
{
    return d->view;
}

void AutomapWidget::setCameraOrigin(const Vec2d &newOrigin, bool instantly)
{
    // Already at this target?
    if (newOrigin == d->targetView)
        return;

    // If the delta is too great - perform the move instantly.
    if (!instantly && d->maxViewPositionDelta > 0)
    {
        const coord_t dist = de::abs((cameraOrigin() - newOrigin).length());
        if (dist > d->maxViewPositionDelta)
        {
            instantly = true;
        }
    }

    // Begin animating toward the new target.
    if (instantly)
    {
        d->view = d->oldView = d->targetView = newOrigin;
    }
    else
    {
        d->oldView    = d->view;
        d->targetView = newOrigin;
        d->viewTimer  = 0;
    }
}

dfloat AutomapWidget::scale() const
{
    return d->targetViewScale;
}

void AutomapWidget::setScale(dfloat newScale)
{
    if (d->needViewScaleUpdate)
        d->updateViewScale();

    newScale = de::clamp(d->minScaleMTOF, newScale, d->maxScaleMTOF);

    // Already at this target?
    if (fequal(newScale, d->targetViewScale))
        return;

    // Begin animating toward the new target.
    d->oldViewScale    = d->viewScale;
    d->viewScaleTimer  = 0;
    d->targetViewScale = newScale;
}

bool AutomapWidget::isOpen() const
{
    return d->open;
}

bool AutomapWidget::isRevealed() const
{
    return d->revealed;
}

void AutomapWidget::reveal(bool yes)
{
    if (d->revealed != yes)
    {
        d->revealed = yes;
        d->needBuildLists = true;
    }
}

void AutomapWidget::pvisibleBounds(coord_t *lowX, coord_t *hiX, coord_t *lowY, coord_t *hiY) const
{
    if (lowX) *lowX = d->viewAABB[BOXLEFT];
    if (hiX)  *hiX  = d->viewAABB[BOXRIGHT];
    if (lowY) *lowY = d->viewAABB[BOXBOTTOM];
    if (hiY)  *hiY  = d->viewAABB[BOXTOP];
}

dint AutomapWidget::pointCount() const
{
    return d->points.count();
}

dint AutomapWidget::addPoint(const Vec3d &origin)
{
    d->points << new MarkedPoint(origin);
    dint pointNum = d->points.count() - 1;  // base 0.
    if (player() >= 0)
    {
        String msg = String(AMSTR_MARKEDSPOT) + " " + String::asText(pointNum);
        P_SetMessageWithFlags(&players[player()], msg, LMF_NO_HIDE);
    }
    return pointNum;
}

bool AutomapWidget::hasPoint(dint index) const
{
    return index >= 0 && index < d->points.count();
}

AutomapWidget::MarkedPoint &AutomapWidget::point(dint index) const
{
    if (hasPoint(index)) return *d->points.at(index);
    /// @throw MissingPointError  Invalid point reference.
    throw MissingPointError("AutomapWidget::point", "Unknown point #" + String::asText(index));
}

LoopResult AutomapWidget::forAllPoints(std::function<LoopResult (MarkedPoint &)> func) const
{
    for (MarkedPoint *point : d->points)
    {
        if (auto result = func(*point)) return result;
    }
    return LoopContinue;
}

void AutomapWidget::clearAllPoints(bool silent)
{
    d->clearPoints();

    if (!silent && player() >= 0)
    {
        P_SetMessageWithFlags(&players[player()], AMSTR_MARKSCLEARED, LMF_NO_HIDE);
    }
}

bool AutomapWidget::cameraZoomMode() const
{
    return d->forceMaxScale;
}

void AutomapWidget::setCameraZoomMode(bool yes)
{
    LOG_AS("AutomapWidget");
    const bool oldZoomMax = d->forceMaxScale;

    if (d->needViewScaleUpdate)
    {
        d->updateViewScale();
    }

    // When switching to max scale mode, store the old scale.
    if (!d->forceMaxScale)
    {
        d->priorToMaxScale = d->viewScale;
    }

    d->forceMaxScale = yes;
    setScale((d->forceMaxScale ? 0 : d->priorToMaxScale));
    if (oldZoomMax != d->forceMaxScale)
    {
        LOGDEV_XVERBOSE("Maximum zoom: ", DE_BOOL_YESNO(cameraZoomMode()));
    }
}

bool AutomapWidget::cameraFollowMode() const
{
    return d->follow;
}

void AutomapWidget::setCameraFollowMode(bool yes)
{
    if (d->follow != yes)
    {
        d->follow = yes;
        if (d->open)
        {
            DD_Executef(true, "%sactivatebcontext map-freepan", d->follow? "de" : "");
            P_SetMessageWithFlags(&players[player()], (d->follow ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF), LMF_NO_HIDE);
        }
    }
}

mobj_t *AutomapWidget::followMobj() const
{
    if (d->followPlayer >= 0)
    {
        player_t *player = &players[d->followPlayer];
        return player->plr->inGame ? player->plr->mo : nullptr;
    }
    return nullptr;
}

bool AutomapWidget::cameraRotationMode() const
{
    return d->rotate;
}

void AutomapWidget::setCameraRotationMode(bool yes)
{
    d->rotate = yes;
}

dfloat AutomapWidget::opacityEX() const
{
    return d->opacity;
}

void AutomapWidget::setOpacityEX(dfloat newOpacity)
{
    newOpacity = de::clamp(0.f, newOpacity, 1.f);
    if (!fequal(newOpacity, d->targetOpacity))
    {
        // Start animating toward the new target.
        d->oldOpacity    = d->opacity;
        d->targetOpacity = newOpacity;
        d->opacityTimer  = 0;
    }
}

dint AutomapWidget::flags() const
{
    return d->flags;
}

void AutomapWidget::setFlags(dint newFlags)
{
    if (d->flags != newFlags)
    {
        d->flags = newFlags;
        // We will need to rebuild one or more display lists.
        d->needBuildLists = true;
    }
}

void AutomapWidget::setMapBounds(coord_t lowX, coord_t hiX, coord_t lowY, coord_t hiY)
{
    d->bounds[BOXLEFT  ] = lowX;
    d->bounds[BOXTOP   ] = hiY;
    d->bounds[BOXRIGHT ] = hiX;
    d->bounds[BOXBOTTOM] = lowY;

    d->updateViewScale();

    setScale(d->minScaleMTOF * 2.4f);  // Default view scale factor.
}

void AutomapWidget::consoleRegister()  // static
{
    C_VAR_FLOAT("map-opacity",              &cfg.common.automapOpacity,        0, 0, 1);
    C_VAR_BYTE ("map-neverobscure",         &cfg.common.automapNeverObscure,   0, 0, 1);
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    C_VAR_BYTE ("map-babykeys",             &cfg.common.automapBabyKeys,       0, 0, 1);
#endif
    C_VAR_FLOAT("map-background-r",         &cfg.common.automapBack[0],        0, 0, 1);
    C_VAR_FLOAT("map-background-g",         &cfg.common.automapBack[1],        0, 0, 1);
    C_VAR_FLOAT("map-background-b",         &cfg.common.automapBack[2],        0, 0, 1);
    C_VAR_INT  ("map-customcolors",         &cfg.common.automapCustomColors,   0, 0, 1);
    C_VAR_FLOAT( "map-line-opacity",        &cfg.common.automapLineAlpha,      0, 0, 1);
    C_VAR_FLOAT("map-line-width",           &cfg.common.automapLineWidth,      0, .5f, 8);
    C_VAR_FLOAT("map-mobj-r",               &cfg.common.automapMobj[0],        0, 0, 1);
    C_VAR_FLOAT("map-mobj-g",               &cfg.common.automapMobj[1],        0, 0, 1);
    C_VAR_FLOAT("map-mobj-b",               &cfg.common.automapMobj[2],        0, 0, 1);
    C_VAR_FLOAT("map-wall-r",               &cfg.common.automapL1[0],          0, 0, 1);
    C_VAR_FLOAT("map-wall-g",               &cfg.common.automapL1[1],          0, 0, 1);
    C_VAR_FLOAT("map-wall-b",               &cfg.common.automapL1[2],          0, 0, 1);
    C_VAR_FLOAT("map-wall-unseen-r",        &cfg.common.automapL0[0],          0, 0, 1);
    C_VAR_FLOAT("map-wall-unseen-g",        &cfg.common.automapL0[1],          0, 0, 1);
    C_VAR_FLOAT("map-wall-unseen-b",        &cfg.common.automapL0[2],          0, 0, 1);
    C_VAR_FLOAT("map-wall-floorchange-r",   &cfg.common.automapL2[0],          0, 0, 1);
    C_VAR_FLOAT("map-wall-floorchange-g",   &cfg.common.automapL2[1],          0, 0, 1);
    C_VAR_FLOAT("map-wall-floorchange-b",   &cfg.common.automapL2[2],          0, 0, 1);
    C_VAR_FLOAT("map-wall-ceilingchange-r", &cfg.common.automapL3[0],          0, 0, 1);
    C_VAR_FLOAT("map-wall-ceilingchange-g", &cfg.common.automapL3[1],          0, 0, 1);
    C_VAR_FLOAT("map-wall-ceilingchange-b", &cfg.common.automapL3[2],          0, 0, 1);
    C_VAR_BYTE ("map-door-colors",          &cfg.common.automapShowDoors,      0, 0, 1);
    C_VAR_FLOAT("map-door-glow",            &cfg.common.automapDoorGlow,       0, 0, 200);
    C_VAR_INT  ("map-huddisplay",           &cfg.common.automapHudDisplay,     0, 0, 2);
    C_VAR_FLOAT("map-pan-speed",            &cfg.common.automapPanSpeed,       0, 0, 1);
    C_VAR_BYTE ("map-pan-resetonopen",      &cfg.common.automapPanResetOnOpen, 0, 0, 1);
    C_VAR_BYTE ("map-rotate",               &cfg.common.automapRotate,         0, 0, 1);
    C_VAR_FLOAT("map-zoom-speed",           &cfg.common.automapZoomSpeed,      0, 0, 1);
    C_VAR_FLOAT("map-open-timer",           &cfg.common.automapOpenSeconds,    CVF_NO_MAX, 0, 0);
    C_VAR_BYTE ("map-title-position",       &cfg.common.automapTitleAtBottom,  0, 0, 1);
    C_VAR_BYTE ("rend-dev-freeze-map",      &freezeMapRLs,                     CVF_NO_ARCHIVE, 0, 1);

    // Aliases for old names:
    C_VAR_FLOAT("map-alpha-lines",          &cfg.common.automapLineAlpha,      0, 0, 1);
}

void G_SetAutomapRotateMode(byte enableRotate)
{
    cfg.common.automapRotate = enableRotate; // Note: this sets the global default.

    for (int i = 0; i < MAXPLAYERS; ++i)
    {
        ST_SetAutomapCameraRotation(i, cfg.common.automapRotate);
        if (players[i].plr->inGame)
        {
            P_SetMessageWithFlags(&players[i],
                                  (cfg.common.automapRotate ? AMSTR_ROTATEON : AMSTR_ROTATEOFF),
                                  LMF_NO_HIDE);
        }
    }
}
