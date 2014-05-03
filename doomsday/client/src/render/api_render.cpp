/** @file api_render.cpp  Public API of the renderer.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#define DENG_NO_API_MACROS_RENDER

#include "de_platform.h"
#include "api_render.h"

#include "dd_main.h" // App_ResourceSystem
#include "def_main.h"
#include "sys_system.h" // novideo
#include "gl/sys_opengl.h"

#include "render/r_main.h"
#include "render/billboard.h" // Rend_SpriteMaterialSpec
#include "render/rend_model.h"

#include "resource/resourcesystem.h"
#ifdef __CLIENT__
#  include "MaterialSnapshot"
#  include "MaterialVariantSpec"
#endif
#include <de/Log>
#include <doomsday/console/exec.h>

// m_misc.c
DENG_EXTERN_C int M_ScreenShot(const char* name, int bits);

#undef Models_CacheForState
DENG_EXTERN_C void Models_CacheForState(int stateIndex)
{
#ifdef __CLIENT__
    if(ModelDef *modelDef = App_ResourceSystem().modelDefForState(stateIndex))
    {
        App_ResourceSystem().cache(modelDef);
    }
#endif
}

// r_draw.cpp
DENG_EXTERN_C void R_SetBorderGfx(struct uri_s const *const *paths);

#undef Rend_CacheForMobjType
DENG_EXTERN_C void Rend_CacheForMobjType(int num)
{
    LOG_AS("Rend.CacheForMobjType");

    if(novideo) return;
    if(!((useModels && precacheSkins) || precacheSprites)) return;
    if(num < 0 || num >= defs.count.mobjs.num) return;

    de::MaterialVariantSpec const &spec = Rend_SpriteMaterialSpec();

    /// @todo Optimize: Traverses the entire state list!
    for(int i = 0; i < defs.count.states.num; ++i)
    {
        if(stateOwners[i] != &mobjInfo[num]) continue;

        Models_CacheForState(i);

        if(precacheSprites)
        {
            state_t *state = Def_GetState(i);
            DENG2_ASSERT(state != 0);

            App_ResourceSystem().cache(state->sprite, spec);
        }
        /// @todo What about sounds?
    }
}

// r_main.cpp
DENG_EXTERN_C void R_RenderPlayerView(int num);
DENG_EXTERN_C void R_SetViewOrigin(int consoleNum, coord_t const origin[3]);
DENG_EXTERN_C void R_SetViewAngle(int consoleNum, angle_t angle);
DENG_EXTERN_C void R_SetViewPitch(int consoleNum, float pitch);
DENG_EXTERN_C int R_ViewWindowGeometry(int consoleNum, RectRaw *geometry);
DENG_EXTERN_C int R_ViewWindowOrigin(int consoleNum, Point2Raw *origin);
DENG_EXTERN_C int R_ViewWindowSize(int consoleNum, Size2Raw *size);
DENG_EXTERN_C void R_SetViewWindowGeometry(int consoleNum, RectRaw const *geometry, dd_bool interpolate);
DENG_EXTERN_C int R_ViewPortGeometry(int consoleNum, RectRaw *geometry);
DENG_EXTERN_C int R_ViewPortOrigin(int consoleNum, Point2Raw *origin);
DENG_EXTERN_C int R_ViewPortSize(int consoleNum, Size2Raw *size);
DENG_EXTERN_C void R_SetViewPortPlayer(int consoleNum, int viewPlayer);

// sky.cpp
DENG_EXTERN_C void R_SkyParams(int layer, int param, void *data);

#undef R_GetSpriteInfo
DENG_EXTERN_C dd_bool R_GetSpriteInfo(int spriteId, int frame, spriteinfo_t *info)
{
    LOG_AS("Rend.GetSpriteInfo");

    if(!info) return false;

    de::zapPtr(info);

    if(!App_ResourceSystem().hasSprite(spriteId, frame))
    {
        LOG_RES_WARNING("Invalid sprite id (%i) and/or frame index (%i)")
            << spriteId << frame;
        return false;
    }

    SpriteViewAngle const &sprViewAngle =
        App_ResourceSystem().sprite(spriteId, frame).viewAngle(0);
    info->material = sprViewAngle.material;
    info->flip     = sprViewAngle.mirrorX;

    if(novideo)
    {
        // We can't prepare the material.
        return true;
    }

#ifdef __CLIENT__
    /// @todo fixme: We should not be using the PSprite spec here. -ds
    de::MaterialVariantSpec const &spec =
        App_ResourceSystem().materialSpec(PSpriteContext, 0, 1, 0, 0,
                                          GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                          0, 1, -1, false, true, true, false);
    de::MaterialSnapshot const &ms = info->material->prepare(spec);

    de::Texture &tex = ms.texture(MTU_PRIMARY).generalCase();
    variantspecification_t const &texSpec = ms.texture(MTU_PRIMARY).spec().variant;

    info->geometry.origin.x    = -tex.origin().x + -texSpec.border;
    info->geometry.origin.y    = -tex.origin().y +  texSpec.border;
    info->geometry.size.width  = ms.width()  + texSpec.border * 2;
    info->geometry.size.height = ms.height() + texSpec.border * 2;

    ms.texture(MTU_PRIMARY).glCoords(&info->texCoord[0], &info->texCoord[1]);
#else
    de::Texture &tex = *info->material->layers()[0]->stages()[0]->texture;

    info->geometry.origin.x    = -tex.origin().x;
    info->geometry.origin.y    = -tex.origin().y;
    info->geometry.size.width  = info->material->width();
    info->geometry.size.height = info->material->height();
#endif

    return true;
}

// r_util.c
DENG_EXTERN_C dd_bool R_ChooseAlignModeAndScaleFactor(float* scale, int width, int height, int availWidth, int availHeight, scalemode_t scaleMode);
DENG_EXTERN_C scalemode_t R_ChooseScaleMode2(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode, float stretchEpsilon);
DENG_EXTERN_C scalemode_t R_ChooseScaleMode(int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);

#undef R_SetupFog
DENG_EXTERN_C void R_SetupFog(float start, float end, float density, float *rgb)
{
    Con_Execute(CMDS_DDAY, "fog on", true, false);
    Con_Executef(CMDS_DDAY, true, "fog start %f", start);
    Con_Executef(CMDS_DDAY, true, "fog end %f", end);
    Con_Executef(CMDS_DDAY, true, "fog density %f", density);
    Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                 rgb[0] * 255, rgb[1] * 255, rgb[2] * 255);
}

#undef R_SetupFogDefaults
DENG_EXTERN_C void R_SetupFogDefaults()
{
    // Go with the defaults.
    Con_Execute(CMDS_DDAY,"fog off", true, false);
}

DENG_DECLARE_API(Rend) =
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
