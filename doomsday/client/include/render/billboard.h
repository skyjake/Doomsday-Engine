/** @file billboard.h  Rendering billboard "sprites".
 *
 * @ingroup render
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_BILLBOARD_H
#define DENG_CLIENT_RENDER_BILLBOARD_H

#include "dd_types.h"
#include "Material"
#include "MaterialVariantSpec"
#include "render/ivissprite.h"

class BspLeaf;

/**
 * Billboard drawing arguments for a masked wall.
 *
 * A sort of a sprite, I guess... Masked walls must be rendered sorted with
 * sprites, so no artifacts appear when sprites are seen behind masked walls.
 */
struct vismaskedwall_t : public IVissprite
{
    de::Vector3d _origin;
    coord_t _distance;       ///< Vissprites are sorted by distance.
    void *material;          ///< MaterialVariant
    blendmode_t blendMode;   ///< Blendmode to be used when drawing (two sided mid textures only)
    struct wall_vertex_s {
        float pos[3];        ///< x y and z coordinates.
        float color[4];
    } vertices[4];

    double texOffset[2];
    float texCoord[2][2];    ///< u and v coordinates.

    DGLuint modTex;          ///< Texture to modulate with.
    float modTexCoord[2][2]; ///< [top-left, bottom-right][x, y]
    float modColor[4];

public:
    vismaskedwall_t()
        : _origin  (0, 0, 0)
        , _distance(0)
        , material (0)
        , blendMode(BM_NORMAL)
        , modTex   (0)
    {
        de::zap(vertices);
        de::zap(texOffset);
        de::zap(texCoord);
        de::zap(modTexCoord);
        de::zap(modColor);
    }
    virtual ~vismaskedwall_t() {}

    // Implements IVissprite.
    coord_t distance() const { return _distance; }
    de::Vector3d const &origin() const { return _origin; }
    void draw();
    void init() { *this = vismaskedwall_t(); }
};

#define MAX_VISSPRITE_LIGHTS (10)

/**
 * Billboard drawing arguments for a map entity, sprite visualization.
 * Sprites look better with Z buffer writes turned off.
 */
struct vissprite_t : public IVissprite
{
// Position/Orientation/Scale:
    de::Vector3d _origin;
    coord_t _distance;      ///< Vissprites are sorted by distance.
    coord_t center[3];      ///< The real center point.
    coord_t srvo[3];        ///< Short-range visual offset.
    bool viewAligned;
    BspLeaf *bspLeaf;

// Appearance:
    bool noZWrite;
    blendmode_t blendMode;
    void *material;         ///< MaterialVariant
    bool matFlip[2];        ///< [S, T] Flip along the specified axis.
    float ambientColor[4];
    uint vLightListIdx;

public:
    vissprite_t()
        : _origin      (0, 0, 0)
        , _distance    (0)
        , viewAligned  (false)
        , bspLeaf      (0)
        , noZWrite     (false)
        , blendMode    (BM_NORMAL)
        , material     (0)
        , vLightListIdx(0)
    {
        de::zap(center);
        de::zap(srvo);
        de::zap(matFlip);
        de::zap(ambientColor);
    }
    virtual ~vissprite_t() {}

    void setup(de::Vector3d const &center, coord_t distToEye, de::Vector3d const &visOffset,
        float secFloor, float secCeil, float floorClip, float top,
        Material &material, bool matFlipS, bool matFlipT, blendmode_t blendMode,
        de::Vector4f const &ambientColor,
        uint vLightListIdx, int tClass, int tMap, BspLeaf *bspLeafAtOrigin,
        bool floorAdjust, bool fitTop, bool fitBottom, bool viewAligned);

    // Implements IVissprite.
    coord_t distance() const { return _distance; }
    de::Vector3d const &origin() const { return _origin; }
    void draw();

    void init() { *this = vissprite_t(); }
};

de::MaterialVariantSpec const &Rend_SpriteMaterialSpec(int tclass = 0, int tmap = 0);

/**
 * @defgroup rendFlareFlags  Flare renderer flags
 * @{
 */
#define RFF_NO_PRIMARY      0x1 ///< Do not draw a primary flare (aka halo).
#define RFF_NO_TURN         0x2 ///< Flares do not turn in response to viewangle/viewdir
/**@}*/

/**
 * Billboard drawing arguments for a lens flare.
 *
 * @see H_RenderHalo()
 */
struct visflare_t : public IVissprite
{
    de::Vector3d _origin;
    coord_t _distance;     ///< Vissprites are sorted by distance.
    byte flags;            ///< @ref rendFlareFlags.
    int size;
    float color[3];
    byte factor;
    float xOff;
    DGLuint tex;           ///< Flaremap if flareCustom ELSE (flaretexName id. Zero = automatical)
    float mul;             ///< Flare brightness factor.
    bool isDecoration;
    int lumIdx;

public:
    visflare_t()
        : _origin     (0, 0, 0)
        , _distance   (0)
        , flags       (0)
        , size        (0)
        , factor      (0)
        , xOff        (0)
        , tex         (0)
        , mul         (0)
        , isDecoration(false)
        , lumIdx      (0)
    {
        de::zap(color);
    }
    virtual ~visflare_t() {}

    void drawPrimary();
    void drawSecondarys();

    // Implements IVissprite.
    coord_t distance() const { return _distance; }
    de::Vector3d const &origin() const { return _origin; }
    void draw() { drawPrimary(); }

    void init() { *this = visflare_t(); }
};

/**
 * Billboard drawing arguments for a "player" sprite (HUD sprite).
 */
struct rendpspriteparams_t
{
    float pos[2]; // {X, Y} Screen-space position.
    float width, height;

    Material *mat;
    float texOffset[2];
    dd_bool texFlip[2]; // {X, Y} Flip along the specified axis.

    float ambientColor[4];
    uint vLightListIdx;
};

void Rend_DrawPSprite(rendpspriteparams_t const &parms);

DENG_EXTERN_C int alwaysAlign;
DENG_EXTERN_C int spriteLight, useSpriteAlpha, useSpriteBlend;
DENG_EXTERN_C int noSpriteZWrite;
DENG_EXTERN_C byte noSpriteTrans;
DENG_EXTERN_C byte devNoSprites;

DENG_EXTERN_C void Rend_SpriteRegister();

#endif // DENG_CLIENT_RENDER_BILLBOARD_H
