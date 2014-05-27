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
    coord_t _distance;            ///< Vissprites are sorted by distance.
    MaterialVariant *material;
    blendmode_t blendmode;
    struct Vertex {
        de::Vector3f pos;
        de::Vector4f rgba;
    } vertices[4];                ///< [bottom-left, top-left, bottom-right, top-right ]

    de::Vector2d texOffset;
    de::Vector2f texCoord[2];     ///< [top-left, bottom-right]

    DGLuint modTex;               ///< Texture to modulate with.
    de::Vector2f modTexCoord[2];  ///< [top-left, bottom-right]
    de::Vector4f modColor;

public:
    vismaskedwall_t()
        : _distance(0)
        , material (0)
        , blendmode(BM_NORMAL)
        , modTex   (0)
    {}
    virtual ~vismaskedwall_t() {}

    // Implements IVissprite.
    de::ddouble distance() const { return _distance; }
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
    coord_t _distance;          ///< Vissprites are sorted by distance.
    de::Vector3d center;        ///< The real center point.
    de::Vector3d srvo;          ///< Short-range visual offset.
    bool viewAligned;
    BspLeaf *bspLeaf;

// Appearance:
    bool noZWrite;
    blendmode_t blendmode;
    MaterialVariant *material;
    bool matFlip[2];            ///< [S, T] Flip along the specified axis.
    de::Vector4f ambientColor;
    uint vLightListIdx;

public:
    vissprite_t()
        : _distance    (0)
        , viewAligned  (false)
        , bspLeaf      (0)
        , noZWrite     (false)
        , blendmode    (BM_NORMAL)
        , material     (0)
        , vLightListIdx(0)
    {
        de::zap(matFlip);
    }
    virtual ~vissprite_t() {}

    void setup(de::Vector3d const &center, coord_t distToEye, de::Vector3d const &visOffset,
        float secFloor, float secCeil, float floorClip, float top,
        Material &material, bool matFlipS, bool matFlipT, blendmode_t blendmode,
        de::Vector4f const &ambientColor,
        uint vLightListIdx, int tClass, int tMap, BspLeaf *bspLeafAtOrigin,
        bool floorAdjust, bool fitTop, bool fitBottom, bool viewAligned);

    // Implements IVissprite.
    de::ddouble distance() const { return _distance; }
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
    de::Vector3f color;
    byte factor;
    float xOff;
    DGLuint tex;           ///< Flaremap if flareCustom ELSE (flaretexName id. Zero = automatical)
    float mul;             ///< Flare brightness factor.
    bool isDecoration;
    int lumIdx;

public:
    visflare_t()
        : _distance   (0)
        , flags       (0)
        , size        (0)
        , factor      (0)
        , xOff        (0)
        , tex         (0)
        , mul         (0)
        , isDecoration(false)
        , lumIdx      (0)
    {}
    virtual ~visflare_t() {}

    void drawPrimary();
    void drawSecondarys();

    // Implements IVissprite.
    de::ddouble distance() const { return _distance; }
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

    de::Vector4f ambientColor;
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
