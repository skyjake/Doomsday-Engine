/** @file server_dummies.h Dummy functions for the server.
 * @ingroup server
 *
 * Empty dummy functions that replace certain client-only functionality on
 * engine-side. Ideally none of these would be needed; each one represents a
 * client-only function call that should not be done in common/shared code.
 * (There should be no shared code outside libdeng1/2.)
 *
 * @todo Add a @c libdeng_gui for UI/graphics code. Many of these belong there
 * instead of being exported out of the client executable for game plugins'
 * needs.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef SERVER_DUMMIES_H
#define SERVER_DUMMIES_H

#include <de/libdeng.h>
#include "map/sector.h"
#include "dd_gl.h"

#ifndef __SERVER__
#  error Attempted to include server's header in a non-server build
#endif

DENG_EXTERN_C void ClMobj_EnableLocalActions(struct mobj_s *mo, boolean enable);
DENG_EXTERN_C boolean ClMobj_LocalActionsEnabled(struct mobj_s *mo);
DENG_EXTERN_C struct mobj_s* ClMobj_Find(thid_t id);
DENG_EXTERN_C boolean ClMobj_IsValid(struct mobj_s* mo);
DENG_EXTERN_C struct mobj_s* ClPlayer_ClMobj(int plrNum);

DENG_EXTERN_C void GameMap_ClMobjReset(GameMap* map);

DENG_EXTERN_C void Con_TransitionRegister();
DENG_EXTERN_C void Con_TransitionTicker(timespan_t t);

DENG_EXTERN_C void GL_Shutdown();
DENG_EXTERN_C void GL_ReleaseGLTexturesByTexture(struct texture_s *tex);
DENG_EXTERN_C void GL_EarlyInitTextureManager();
DENG_EXTERN_C void GL_PruneTextureVariantSpecifications();
DENG_EXTERN_C void GL_SetFilter(int f);
DENG_EXTERN_C void GL_SetFilterColor(float r, float g, float b, float a);
DENG_EXTERN_C void GL_UseFog(int yes);
DENG_EXTERN_C void GL_ConfigureBorderedProjection(borderedprojectionstate_t* bp, int flags, int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);
DENG_EXTERN_C void GL_BeginBorderedProjection(borderedprojectionstate_t* bp);
DENG_EXTERN_C void GL_EndBorderedProjection(borderedprojectionstate_t* bp);
DENG_EXTERN_C void GL_DrawSvg3(svgid_t svgId, const Point2Rawf* origin, float scale, float angle);

DENG_EXTERN_C void R_InitViewWindow(void);
DENG_EXTERN_C void R_RenderPlayerView(int num);
DENG_EXTERN_C void R_SetBorderGfx(Uri const *const *paths);
DENG_EXTERN_C void R_SkyParams(int layer, int param, void *data);
DENG_EXTERN_C void R_InitSvgs(void);
DENG_EXTERN_C void R_ShutdownSvgs(void);
DENG_EXTERN_C void R_NewSvg(svgid_t id, const def_svgline_t* lines, uint numLines);
DENG_EXTERN_C struct font_s* R_CreateFontFromDef(ded_compositefont_t* def);

DENG_EXTERN_C void FR_Init(void);
DENG_EXTERN_C void FR_SetFont(fontid_t font);
DENG_EXTERN_C void FR_PushAttrib(void);
DENG_EXTERN_C void FR_PopAttrib(void);
DENG_EXTERN_C void FR_LoadDefaultAttrib(void);
DENG_EXTERN_C float FR_Leading(void);
DENG_EXTERN_C void FR_SetLeading(float value);
DENG_EXTERN_C int FR_Tracking(void);
DENG_EXTERN_C void FR_SetTracking(int value);
DENG_EXTERN_C void FR_SetColor(float red, float green, float blue);
DENG_EXTERN_C void FR_SetColorv(const float rgb[3]);
DENG_EXTERN_C void FR_SetColorAndAlpha(float red, float green, float blue, float alpha);
DENG_EXTERN_C void FR_SetColorAndAlphav(const float rgba[4]);
DENG_EXTERN_C void FR_SetColorRed(float value);
DENG_EXTERN_C void FR_SetColorGreen(float value);
DENG_EXTERN_C void FR_SetColorBlue(float value);
DENG_EXTERN_C void FR_SetAlpha(float value);
DENG_EXTERN_C void FR_SetShadowOffset(int offsetX, int offsetY);
DENG_EXTERN_C void FR_SetShadowStrength(float value);
DENG_EXTERN_C void FR_SetGlitterStrength(float value);
DENG_EXTERN_C void FR_SetCaseScale(boolean value);
DENG_EXTERN_C void FR_DrawText(const char* text, const Point2Raw* origin);
DENG_EXTERN_C void FR_DrawText2(const char* text, const Point2Raw* origin, int alignFlags);
DENG_EXTERN_C void FR_DrawText3(const char* text, const Point2Raw* _origin, int alignFlags, short _textFlags);
DENG_EXTERN_C void FR_DrawTextXY3(const char* text, int x, int y, int alignFlags, short flags);
DENG_EXTERN_C void FR_DrawTextXY2(const char* text, int x, int y, int alignFlags);
DENG_EXTERN_C void FR_DrawTextXY(const char* text, int x, int y);DENG_EXTERN_C int FR_CharWidth(unsigned char ch);
DENG_EXTERN_C int FR_CharHeight(unsigned char ch);
DENG_EXTERN_C void FR_DrawChar3(unsigned char ch, const Point2Raw* origin, int alignFlags, short textFlags);
DENG_EXTERN_C void FR_DrawChar2(unsigned char ch, const Point2Raw* origin, int alignFlags);
DENG_EXTERN_C void FR_DrawChar(unsigned char ch, const Point2Raw* origin);
DENG_EXTERN_C void FR_DrawCharXY3(unsigned char ch, int x, int y, int alignFlags, short textFlags);
DENG_EXTERN_C void FR_DrawCharXY2(unsigned char ch, int x, int y, int alignFlags);
DENG_EXTERN_C void FR_DrawCharXY(unsigned char ch, int x, int y);
DENG_EXTERN_C void FR_ResetTypeinTimer(void);
DENG_EXTERN_C void FR_TextSize(Size2Raw* size, const char* text);
DENG_EXTERN_C int FR_TextWidth(const char* text);
DENG_EXTERN_C int FR_TextHeight(const char* text);

DENG_EXTERN_C void Fonts_Init(void);
DENG_EXTERN_C fontid_t Fonts_ResolveUri(Uri const *uri);
DENG_EXTERN_C void Fonts_ClearDefinitionLinks(void);
DENG_EXTERN_C void Fonts_ClearRuntime(void);

DENG_EXTERN_C void Rend_Init(void);
DENG_EXTERN_C void Rend_DecorInit();
DENG_EXTERN_C void Rend_CacheForMap();
DENG_EXTERN_C void Rend_CacheForMobjType(int num);
DENG_EXTERN_C void Rend_CalcLightModRange();
DENG_EXTERN_C void Rend_ConsoleInit();
DENG_EXTERN_C void Rend_ConsoleResize(int force);
DENG_EXTERN_C void Rend_ConsoleOpen(int yes);
DENG_EXTERN_C void Rend_ConsoleToggleFullscreen();
DENG_EXTERN_C void Rend_ConsoleMove(int y);
DENG_EXTERN_C void Rend_ConsoleCursorResetBlink();

DENG_EXTERN_C void Sky_Ticker();

DENG_EXTERN_C void Models_Init();
DENG_EXTERN_C void Models_Shutdown();
DENG_EXTERN_C void Models_CacheForState(int stateIndex);

DENG_EXTERN_C void SB_InitForMap(const char* uniqueID);
DENG_EXTERN_C void SB_SurfaceMoved(biassurface_t* bsuf);

DENG_EXTERN_C void LG_SectorChanged(Sector* sector);

DENG_EXTERN_C void Cl_InitPlayers(void);

DENG_EXTERN_C void UI_Init();
DENG_EXTERN_C void UI_Ticker(timespan_t t);
DENG_EXTERN_C void UI2_Ticker(timespan_t t);
DENG_EXTERN_C void UI_Shutdown();

#endif // SERVER_DUMMIES_H
