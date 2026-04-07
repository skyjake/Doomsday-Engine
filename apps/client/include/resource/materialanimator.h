/** @file materialanimator.h  Animator for a draw-context Material variant.
 *
 * @authors Copyright © 2009-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_RESOURCE_MATERIALANIMATOR_H
#define CLIENT_RESOURCE_MATERIALANIMATOR_H

#ifndef __CLIENT__
#  error "MaterialAnimator only exists in the Client"
#endif

#include <de/error.h>
#include <de/observers.h>
#include <de/vector.h>
#include "resource/clienttexture.h"
#include "resource/clientmaterial.h"
#include "render/materialcontext.h"
#include "resource/materialvariantspec.h"
#include "gl/gltextureunit.h"

/**
 * Animator for a Material within a given client side usage context.
 *
 * Each usage context has it's own Material animator for an independent animation
 * timeline. Additionally, contexts define a @ref MaterialVariantSpec which dictates
 * how the various dependent resources are interpreted within that context. (This is
 * necessary because of the quirky behavior of the id Tech 1 map renderer, where the
 * texture dimensions are interpreted differently according to whether it is used on
 * a "floor" or "wall" map surface).
 */
class MaterialAnimator
{
public:
    /// The referenced (GL)texture unit does not exist. @ingroup errors
    DE_ERROR(MissingTextureUnitError);

    /// The referenced decoration does not exist. @ingroup errors
    DE_ERROR(MissingDecorationError);

    /// Notified whenever one or more decoration stage changes occur.
    DE_DEFINE_AUDIENCE(DecorationStageChange, void materialAnimatorDecorationStageChanged(MaterialAnimator &animator))

    /**
     * (GL)Texture unit identifier:
     */
    enum
    {
        TU_DETAIL,
        TU_DETAIL_INTER,
        TU_LAYER0,
        TU_LAYER0_INTER,
        TU_SHINE,
        TU_SHINE_MASK,
        NUM_TEXTUREUNITS
    };

    /**
     * Animated Material::Decoration.
     */
    class Decoration
    {
    public:
        /**
         * @param decor  Material decoration to animate.
         */
        Decoration(MaterialDecoration &decor);

        /**
         * Returns the MaterialDecoration being animated.
         */
        MaterialDecoration &decor() const;

        de::Vec2f origin() const;
        float elevation() const;

        float radius() const;
        de::Vec3f color() const;
        void lightLevels(float &min, float &max) const;

        ClientTexture *tex() const;
        ClientTexture *ceilTex() const;
        ClientTexture *floorTex() const;

        float flareSize() const;
        DGLuint flareTex() const;

        void rewind();
        bool animate();

        void update();
        void reset();

    private:
        DE_PRIVATE(d)
    };

public:
    /**
     * Construct a new MaterialAnimator.
     *
     * @param material  Material to animate.
     * @param spec      Draw-context variant specification.
     */
    MaterialAnimator(ClientMaterial &material, const de::MaterialVariantSpec &variantSpec);

    /**
     * Returns the Material being animated.
     */
    ClientMaterial &material() const;

    /**
     * Returns the MaterialVariantSpec for the associated usage context.
     */
    const de::MaterialVariantSpec &variantSpec() const;

    /**
     * Process a system tick event. If not currently paused and still valid; the material's
     * layers and decorations are animated.
     *
     * @param ticLength  Length of the tick in seconds.
     *
     * @see isPaused()
     */
    void animate(timespan_t ticLength);

    /**
     * Restart the animation, resetting the staged animation point. The state of all layers
     * and decorations will be rewound to the beginning.
     */
    void rewind();

    /**
     * Returns @c true if animation is currently paused (e.g., the driver for the animation
     * is the world map-context, using the game timer and the client has paused the game).
     */
    bool isPaused() const;

    /**
     * Prepare for drawing a frame (uploading GL textures if necessary and perhaps updating
     * the animation state snapshot).
     *
     * @param forceUpdate  @c true= Perform a full update of the state snapshot. The snapshot
     * will be updated automatically when the animator is first asked to prepare assets for
     * drawing a new frame. The only time it is necessary to force an update is if the
     * material state subsequently changes during the same frame.
     *
     * @todo Fully internalize animation state updates and separate GL (de)init logics. -ds
     */
    void prepare(bool forceUpdate = false);

    void cacheAssets();

    /**
     * Returns @c true if the Material is currently thought to be fully "opaque", i.e., the
     * composited layer stack has no translucent gaps.
     */
    bool isOpaque() const;

    /**
     * Returns the current dimension metrics for the animated Material.
     */
    const de::Vec2ui &dimensions() const;

    /**
     * Returns the current glow strength factor for the animated Material.
     */
    float glowStrength() const;

    /**
     * Returns the current shine effect blending mode for the animated Material.
     */
    blendmode_t shineBlendMode() const;

    /**
     * Returns the current shine effect minimum color for the animated Material.
     */
    const de::Vec3f &shineMinColor() const;

    /**
     * Lookup a prepared GLTextureUnit by it's unique @a unitIndex.
     *
     * @see prepare()
     */
    de::GLTextureUnit &texUnit(int unitIndex) const;

    /**
     * Lookup an animated Material Decoration by it's unique @a decorIndex.
     */
    Decoration &decoration(int decorIndex) const;

private:
    DE_PRIVATE(d)
};

#endif  // CLIENT_RESOURCE_MATERIALANIMATOR_H
