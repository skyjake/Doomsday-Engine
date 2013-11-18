#define DENG_NO_API_MACROS_RENDER

#include "de_base.h"
#include "de_console.h"

#include "api_render.h"

#include "resource/sprites.h"
#ifdef __CLIENT__
#  include "MaterialSnapshot"
#endif

// m_misc.c
DENG_EXTERN_C int M_ScreenShot(const char* name, int bits);

// models.cpp
DENG_EXTERN_C void Models_CacheForState(int stateIndex);

// r_draw.cpp
DENG_EXTERN_C void R_SetBorderGfx(struct uri_s const *const *paths);

// r_main.cpp
DENG_EXTERN_C void Rend_CacheForMobjType(int mobjtypeNum);
DENG_EXTERN_C void R_RenderPlayerView(int num);
DENG_EXTERN_C void R_SetViewOrigin(int consoleNum, coord_t const origin[3]);
DENG_EXTERN_C void R_SetViewAngle(int consoleNum, angle_t angle);
DENG_EXTERN_C void R_SetViewPitch(int consoleNum, float pitch);
DENG_EXTERN_C int R_ViewWindowGeometry(int consoleNum, RectRaw *geometry);
DENG_EXTERN_C int R_ViewWindowOrigin(int consoleNum, Point2Raw *origin);
DENG_EXTERN_C int R_ViewWindowSize(int consoleNum, Size2Raw *size);
DENG_EXTERN_C void R_SetViewWindowGeometry(int consoleNum, RectRaw const *geometry, boolean interpolate);
DENG_EXTERN_C int R_ViewPortGeometry(int consoleNum, RectRaw *geometry);
DENG_EXTERN_C int R_ViewPortOrigin(int consoleNum, Point2Raw *origin);
DENG_EXTERN_C int R_ViewPortSize(int consoleNum, Size2Raw *size);
DENG_EXTERN_C void R_SetViewPortPlayer(int consoleNum, int viewPlayer);

// sky.cpp
DENG_EXTERN_C void R_SkyParams(int layer, int param, void *data);

#undef R_GetSpriteInfo
DENG_EXTERN_C boolean R_GetSpriteInfo(int spriteId, int frame, spriteinfo_t *info)
{
    LOG_AS("R_GetSpriteInfo");

    if(!info) return false;

    de::zapPtr(info);

    Sprite *sprite = R_SpritePtr(spriteId, frame);
    if(!sprite)
    {
        LOG_WARNING("Invalid sprite id (%i) and/or frame index (%i).")
            << spriteId << frame;
        return false;
    }

    de::zapPtr(info);
    info->flip = sprite->_flip[0];

    if(novideo)
    {
        // We can't prepare the material.
        return true;
    }

    info->material = sprite->_mats[0];

#ifdef __CLIENT__
    /// @todo fixme: We should not be using the PSprite spec here. -ds
    de::MaterialVariantSpec const &spec =
            App_Materials().variantSpec(PSpriteContext, 0, 1, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                        0, 1, -1, false, true, true, false);
    de::MaterialSnapshot const &ms = info->material->prepare(spec);

    de::Texture &tex = ms.texture(MTU_PRIMARY).generalCase();
    variantspecification_t const &texSpec = TS_GENERAL(ms.texture(MTU_PRIMARY).spec());

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
DENG_EXTERN_C boolean R_ChooseAlignModeAndScaleFactor(float* scale, int width, int height, int availWidth, int availHeight, scalemode_t scaleMode);
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
