/** @file api_render.cpp  Public API of the renderer.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#define DE_NO_API_MACROS_RENDER

#ifndef __CLIENT__
#  error "api_render.cpp is for the client only"
#endif

#include "de_platform.h"
#include "api_render.h"

#include <de/log.h>
#include <doomsday/console/exec.h>
#include <doomsday/defs/sprite.h>
#include <doomsday/res/sprites.h>
#include <doomsday/world/materials.h>

#include "dd_main.h"  // App_Resources
#include "def_main.h"
#include "sys_system.h"  // novideo
#include "gl/sys_opengl.h"

#include "render/r_main.h"
#include "render/billboard.h"  // Rend_SpriteMaterialSpec
#include "render/rend_model.h"

#include "resource/clientresources.h"
#include "resource/materialvariantspec.h"

using namespace de;

// m_misc.c
#undef M_ScreenShot
DE_EXTERN_C dint M_ScreenShot(const char *name, dint flags);

#undef Models_CacheForState
DE_EXTERN_C void Models_CacheForState(dint stateIndex)
{
    if (FrameModelDef *modelDef = App_Resources().modelDefForState(stateIndex))
    {
        App_Resources().cache(modelDef);
    }
}

// r_draw.cpp
#undef R_SetBorderGfx
DE_EXTERN_C void R_SetBorderGfx(const struct uri_s *const *paths);

#undef Rend_CacheForMobjType
DE_EXTERN_C void Rend_CacheForMobjType(dint num)
{
    LOG_AS("Rend.CacheForMobjType");

    if (::novideo) return;
    if (!((::useModels && ::precacheSkins) || ::precacheSprites)) return;
    if (num < 0 || num >= ::runtimeDefs.mobjInfo.size()) return;

    const de::MaterialVariantSpec &spec = Rend_SpriteMaterialSpec();

    /// @todo Optimize: Traverses the entire state list!
    for (dint i = 0; i < DED_Definitions()->states.size(); ++i)
    {
        if (::runtimeDefs.stateInfo[i].owner != &::runtimeDefs.mobjInfo[num])
            continue;

        Models_CacheForState(i);

        if (::precacheSprites)
        {
            if (state_t *state = Def_GetState(i))
            {
                App_Resources().cache(state->sprite, spec);
            }
        }
        /// @todo What about sounds?
    }
}

#undef R_RenderPlayerView
#undef R_SetViewOrigin
#undef R_SetViewAngle
#undef R_SetViewPitch
#undef R_ViewWindowGeometry
#undef R_ViewWindowOrigin
#undef R_ViewWindowSize
#undef R_SetViewWindowGeometry
#undef R_ViewPortGeometry
#undef R_ViewPortOrigin
#undef R_ViewPortSize
#undef R_SetViewPortPlayer

// r_main.cpp
DE_EXTERN_C void R_RenderPlayerView(dint num);
DE_EXTERN_C void R_SetViewOrigin(dint consoleNum, coord_t const origin[3]);
DE_EXTERN_C void R_SetViewAngle(dint consoleNum, angle_t angle);
DE_EXTERN_C void R_SetViewPitch(dint consoleNum, dfloat pitch);
DE_EXTERN_C dint R_ViewWindowGeometry(dint consoleNum, RectRaw *geometry);
DE_EXTERN_C dint R_ViewWindowOrigin(dint consoleNum, Point2Raw *origin);
DE_EXTERN_C dint R_ViewWindowSize(dint consoleNum, Size2Raw *size);
DE_EXTERN_C void R_SetViewWindowGeometry(dint consoleNum, const RectRaw *geometry, dd_bool interpolate);
DE_EXTERN_C dint R_ViewPortGeometry(dint consoleNum, RectRaw *geometry);
DE_EXTERN_C dint R_ViewPortOrigin(dint consoleNum, Point2Raw *origin);
DE_EXTERN_C dint R_ViewPortSize(dint consoleNum, Size2Raw *size);
DE_EXTERN_C void R_SetViewPortPlayer(dint consoleNum, dint viewPlayer);

// sky.cpp
#undef R_SkyParams
DE_EXTERN_C void R_SkyParams(dint layer, dint param, void *data);

static inline const MaterialVariantSpec &pspriteMaterialSpec_GetSpriteInfo()
{
    return App_Resources().materialSpec(PSpriteContext, 0, 1, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                             0, -2, -1, false, true, true, false);
}

#undef R_GetSpriteInfo
DE_EXTERN_C dd_bool R_GetSpriteInfo(dint id, dint frame, spriteinfo_t *info)
{
    if (!info) return false;

    de::zapPtr(info);

    const auto *sprDef = res::Sprites::get().spritePtr(id, frame);
    if (!sprDef)
    {
        LOG_RES_WARNING("Invalid sprite id:%i and/or frame:%i") << id << frame;
        return false;
    }

    defn::Sprite sprite(*sprDef);
    if (!sprite.hasView(0))
    {
        LOG_RES_WARNING("Sprite id:%i frame:%i has no front view")
            << id << frame;
        return false;
    }

    const defn::Sprite::View spriteView = sprite.view(0);
    info->material = world::Materials::get().materialPtr(*spriteView.material);
    info->flip     = spriteView.mirrorX;

    if (::novideo) return true;  // We can't prepare the material.

    auto *material = reinterpret_cast<world::Material *>(info->material);
    if (auto *clMat = maybeAs<ClientMaterial>(material))
    {
        /// @todo fixme: We should not be using the PSprite spec here. -ds
        MaterialAnimator &matAnimator = clMat->getAnimator(pspriteMaterialSpec_GetSpriteInfo());
        matAnimator.prepare(); // Ensure we have up-to-date info.

        const Vec2ui &  matDimensions = matAnimator.dimensions();
        TextureVariant *tex           = matAnimator.texUnit(MaterialAnimator::TU_LAYER0).texture;
        const Vec2i &   texDimensions = tex->base().origin();
        const dint      texBorder     = tex->spec().variant.border;

        info->geometry.origin.x    = -texDimensions.x + -texBorder;
        info->geometry.origin.y    = -texDimensions.y +  texBorder;
        info->geometry.size.width  = matDimensions.x + texBorder * 2;
        info->geometry.size.height = matDimensions.y + texBorder * 2;

        tex->glCoords(&info->texCoord[0], &info->texCoord[1]);
    }
    else
    {
        // TODO: Figure out the metrics some other way.
        zap(info->geometry);
        zap(info->texCoord);
    }

    return true;
}

#undef R_ChooseAlignModeAndScaleFactor
#undef R_ChooseScaleMode2
#undef R_ChooseScaleMode

// r_util.c
DE_EXTERN_C dd_bool R_ChooseAlignModeAndScaleFactor(dfloat *scale, dint width, dint height, dint availWidth, dint availHeight, scalemode_t scaleMode);
DE_EXTERN_C scalemode_t R_ChooseScaleMode2(dint width, dint height, dint availWidth, dint availHeight, scalemode_t overrideMode, dfloat stretchEpsilon);
DE_EXTERN_C scalemode_t R_ChooseScaleMode(dint width, dint height, dint availWidth, dint availHeight, scalemode_t overrideMode);

#undef R_SetupFog
DE_EXTERN_C void R_SetupFog(dfloat start, dfloat end, dfloat density, const dfloat *rgb)
{
    DE_ASSERT(rgb);
    Con_Execute(CMDS_DDAY, "fog on", true, false);
    Con_Executef(CMDS_DDAY, true, "fog start %f", start);
    Con_Executef(CMDS_DDAY, true, "fog end %f", end);
    Con_Executef(CMDS_DDAY, true, "fog density %f", density);
    Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                 rgb[0] * 255, rgb[1] * 255, rgb[2] * 255);
}

#undef R_SetupFogDefaults
DE_EXTERN_C void R_SetupFogDefaults()
{
    // Go with the defaults.
    Con_Execute(CMDS_DDAY, "fog off", true, false);
}

DE_DECLARE_API(Rend) =
{
    { DE_API_RENDER },
    R_SetupFogDefaults,
    R_SetupFog,
    Rend_CacheForMobjType,
    Models_CacheForState,
    R_RenderPlayerView,
    R_SetViewOrigin,
    R_SetViewAngle,
    R_SetViewPitch,
    R_ViewWindowGeometry,
    R_ViewWindowOrigin,
    R_ViewWindowSize,
    R_SetViewWindowGeometry,
    R_SetBorderGfx,
    R_ViewPortGeometry,
    R_ViewPortOrigin,
    R_ViewPortSize,
    R_SetViewPortPlayer,
    R_ChooseAlignModeAndScaleFactor,
    R_ChooseScaleMode2,
    R_ChooseScaleMode,
    R_GetSpriteInfo,
    R_SkyParams,
    M_ScreenShot
};
