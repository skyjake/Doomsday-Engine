/** @file server_dummies.c Dummy functions for the server.
 * @ingroup server
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

#include "server_dummies.h"
#include "ui/nativeui.h"
#include "ui/finaleinterpreter.h"
#include "resource/texturevariantspecification.h"
#include "resource/texturevariant.h"
#include "resource/texture.h"

void DD_ReadMouse(void)
{}

void DD_ReadJoystick(void)
{}

/*
void ClMobj_EnableLocalActions(struct mobj_s *mo, boolean enable)
{
    // No clmobjs on server.
    DENG_UNUSED(mo);
    DENG_UNUSED(enable);
}

boolean ClMobj_LocalActionsEnabled(struct mobj_s *mo)
{
    DENG_UNUSED(mo);
    return false;
}

struct mobj_s* ClMobj_Find(thid_t id)
{
    // No clmobjs on server.
    DENG_UNUSED(id);
    return 0;
}

boolean ClMobj_IsValid(struct mobj_s* mo)
{
    DENG_UNUSED(mo);
    return true;
}

struct mobj_s* ClPlayer_ClMobj(int plrNum)
{
    DENG_UNUSED(plrNum);
    return 0;
}
*/

void GameMap_ClMobjReset(GameMap* map)
{
    DENG_UNUSED(map);
}

void Con_TransitionRegister()
{}

void Con_TransitionTicker(timespan_t t)
{
    DENG_UNUSED(t);
}

void GL_Shutdown()
{}

void GL_ReleaseGLTexturesByTexture(struct texture_s *tex)
{
    DENG_UNUSED(tex);
}

void GL_EarlyInitTextureManager()
{}

void GL_PruneTextureVariantSpecifications()
{}

void GL_SetFilter(int f)
{
    DENG_UNUSED(f);
}

void GL_SetFilterColor(float r, float g, float b, float a)
{
    DENG_UNUSED(r);
    DENG_UNUSED(g);
    DENG_UNUSED(b);
    DENG_UNUSED(a);
}

void GL_UseFog(int yes)
{
    DENG_UNUSED(yes);
}

void GL_ConfigureBorderedProjection(dgl_borderedprojectionstate_t* bp, int flags, int width, int height, int availWidth, int availHeight, scalemode_t overrideMode)
{
    DENG_UNUSED(bp);
    DENG_UNUSED(flags);
    DENG_UNUSED(width);
    DENG_UNUSED(height);
    DENG_UNUSED(bp);
    DENG_UNUSED(availWidth);
    DENG_UNUSED(availHeight);
    DENG_UNUSED(overrideMode);
}

void GL_ConfigureBorderedProjection2(dgl_borderedprojectionstate_t* bp, int flags,
                                     int width, int height, int availWidth, int availHeight, scalemode_t overrideMode,
                                     float stretchEpsilon)
{
    DENG_UNUSED(bp);
    DENG_UNUSED(flags);
    DENG_UNUSED(width);
    DENG_UNUSED(height);
    DENG_UNUSED(bp);
    DENG_UNUSED(availWidth);
    DENG_UNUSED(availHeight);
    DENG_UNUSED(overrideMode);
    DENG_UNUSED(stretchEpsilon);
}

void GL_BeginBorderedProjection(dgl_borderedprojectionstate_t* bp)
{
    DENG_UNUSED(bp);
}

void GL_EndBorderedProjection(dgl_borderedprojectionstate_t* bp)
{
    DENG_UNUSED(bp);
}

/*
void GL_DrawSvg3(svgid_t svgId, const Point2Rawf* origin, float scale, float angle)
{
    DENG_UNUSED(svgId);
    DENG_UNUSED(origin);
    DENG_UNUSED(scale);
    DENG_UNUSED(angle);
}

void GL_DrawSvg2(svgid_t id, const Point2Rawf* origin, float scale)
{}

void GL_DrawSvg(svgid_t id, const Point2Rawf* origin)
{}
*/

void GL_PrepareLSTexture(void)
{}

void GL_PrintTextureVariantSpecification(texturevariantspecification_t const *baseSpec)
{}

void GL_ReleaseTexturesByColorPalette(colorpaletteid_t paletteId)
{
    DENG_UNUSED(paletteId);
}

struct texturevariant_s* GL_PreparePatchTexture(struct texture_s* tex)
{
    return 0;
}

texturevariantspecification_t *GL_TextureVariantSpecificationForContext(
    texturevariantusagecontext_t tc, int flags, byte border, int tClass, int tMap,
    int wrapS, int wrapT, int minFilter, int magFilter, int anisoFilter,
    boolean mipmapped, boolean gammaCorrection, boolean noStretch, boolean toAlpha)
{
    return 0;
}

void FR_Init(void)
{}

/*
void FR_SetFont(fontid_t font)
{
    DENG_UNUSED(font);
}

void FR_PushAttrib(void)
{}

void FR_PopAttrib(void)
{}

void FR_LoadDefaultAttrib(void)
{}

float FR_Leading(void)
{
    return 0;
}

void FR_SetLeading(float value)
{
    DENG_UNUSED(value);
}

int FR_Tracking(void)
{
    return 0;
}

void FR_SetTracking(int value)
{
    DENG_UNUSED(value);
}

void FR_SetColor(float red, float green, float blue)
{
    DENG_UNUSED(red);
    DENG_UNUSED(green);
    DENG_UNUSED(blue);
}

void FR_SetColorv(const float rgb[3])
{
    DENG_UNUSED(rgb);
}

void FR_SetColorAndAlpha(float red, float green, float blue, float alpha)
{
    DENG_UNUSED(red);
    DENG_UNUSED(green);
    DENG_UNUSED(blue);
    DENG_UNUSED(alpha);
}

void FR_SetColorAndAlphav(const float rgba[4])
{
    DENG_UNUSED(rgba);
}

void FR_SetColorRed(float value)
{
    DENG_UNUSED(value);
}

void FR_SetColorGreen(float value)
{
    DENG_UNUSED(value);
}

void FR_SetColorBlue(float value)
{
    DENG_UNUSED(value);
}

float FR_Alpha(void)
{
    return 0;
}

float FR_ColorRed(void)
{
    return 0;
}

float FR_ColorGreen(void)
{
    return 0;
}

float FR_ColorBlue(void)
{
    return 0;
}

void FR_ShadowOffset(int* offsetX, int* offsetY)
{
    DENG_UNUSED(offsetX);
    DENG_UNUSED(offsetY);
}

float FR_ShadowStrength(void)
{
    return 0;
}

float FR_GlitterStrength(void)
{
    return 0;
}

boolean FR_CaseScale(void)
{
    return false;
}

void FR_CharSize(Size2Raw* size, unsigned char ch)
{
    DENG_UNUSED(size);
    DENG_UNUSED(ch);
}

fontid_t FR_Font(void)
{
    return 0;
}

void FR_ColorAndAlpha(float rgba[4])
{
    DENG_UNUSED(rgba);
}

void FR_SetAlpha(float value)
{
    DENG_UNUSED(value);
}

void FR_SetShadowOffset(int offsetX, int offsetY)
{
    DENG_UNUSED(offsetX);
    DENG_UNUSED(offsetY);
}

void FR_SetShadowStrength(float value)
{
    DENG_UNUSED(value);
}

void FR_SetGlitterStrength(float value)
{
    DENG_UNUSED(value);
}

void FR_SetCaseScale(boolean value)
{
    DENG_UNUSED(value);
}

void FR_DrawText(const char* text, const Point2Raw* origin)
{
    DENG_UNUSED(text);
    DENG_UNUSED(origin);
}

void FR_DrawText2(const char* text, const Point2Raw* origin, int alignFlags)
{
    DENG_UNUSED(text);
    DENG_UNUSED(origin);
    DENG_UNUSED(alignFlags);
}

void FR_DrawText3(const char* text, const Point2Raw* _origin, int alignFlags, short _textFlags)
{
    DENG_UNUSED(text);
    DENG_UNUSED(_origin);
    DENG_UNUSED(alignFlags);
    DENG_UNUSED(_textFlags);
}

void FR_DrawTextXY3(const char* text, int x, int y, int alignFlags, short flags)
{
    DENG_UNUSED(text);
    DENG_UNUSED(x);
    DENG_UNUSED(y);
    DENG_UNUSED(alignFlags);
    DENG_UNUSED(flags);
}

void FR_DrawTextXY2(const char* text, int x, int y, int alignFlags)
{
    DENG_UNUSED(text);
    DENG_UNUSED(x);
    DENG_UNUSED(y);
    DENG_UNUSED(alignFlags);
}

void FR_DrawTextXY(const char* text, int x, int y)
{
    DENG_UNUSED(text);
    DENG_UNUSED(x);
    DENG_UNUSED(y);
}

int FR_CharWidth(unsigned char ch)
{
    DENG_UNUSED(ch);
    return 0;
}

int FR_CharHeight(unsigned char ch)
{
    DENG_UNUSED(ch);
    return 0;
}

void FR_DrawChar3(unsigned char ch, const Point2Raw* origin, int alignFlags, short textFlags)
{
    DENG_UNUSED(ch);
    DENG_UNUSED(origin);
    DENG_UNUSED(alignFlags);
    DENG_UNUSED(textFlags);
}

void FR_DrawChar2(unsigned char ch, const Point2Raw* origin, int alignFlags)
{
    DENG_UNUSED(ch);
    DENG_UNUSED(origin);
    DENG_UNUSED(alignFlags);
}

void FR_DrawChar(unsigned char ch, const Point2Raw* origin)
{
    DENG_UNUSED(ch);
    DENG_UNUSED(origin);
}

void FR_DrawCharXY3(unsigned char ch, int x, int y, int alignFlags, short textFlags)
{
    DENG_UNUSED(ch);
    DENG_UNUSED(x);
    DENG_UNUSED(y);
    DENG_UNUSED(alignFlags);
    DENG_UNUSED(textFlags);
}

void FR_DrawCharXY2(unsigned char ch, int x, int y, int alignFlags)
{
    DENG_UNUSED(ch);
    DENG_UNUSED(x);
    DENG_UNUSED(y);
    DENG_UNUSED(alignFlags);
}

void FR_DrawCharXY(unsigned char ch, int x, int y)
{
    DENG_UNUSED(ch);
    DENG_UNUSED(x);
    DENG_UNUSED(y);
}

void FR_ResetTypeinTimer(void)
{}

void FR_TextSize(Size2Raw* size, const char* text)
{
    DENG_UNUSED(size);
    DENG_UNUSED(text);
}

int FR_TextWidth(const char* text)
{
    DENG_UNUSED(text);
    return 0;
}

int FR_TextHeight(const char* text)
{
    DENG_UNUSED(text);
    return 0;
}
*/

void Fonts_Init(void)
{}

fontschemeid_t Fonts_ParseScheme(const char* str)
{
    DENG_UNUSED(str);
    return 0;
}

fontid_t Fonts_ResolveUri(Uri const *uri)
{
    DENG_UNUSED(uri);
    return 0;
}

fontid_t Fonts_ResolveUriCString(const char* uri)
{
    DENG_UNUSED(uri);
    return 0;
}

void Fonts_ClearDefinitionLinks(void)
{}

void Fonts_ClearRuntime(void)
{}

void R_InitViewWindow(void)
{}

void R_RenderPlayerView(int num)
{
    DENG_UNUSED(num);
}

void R_SetBorderGfx(Uri const *const *paths)
{
    DENG_UNUSED(paths);
}

void R_SkyParams(int layer, int param, void *data)
{
    DENG_UNUSED(layer);
    DENG_UNUSED(param);
    DENG_UNUSED(data);
}

void R_InitSvgs(void)
{}

void R_ShutdownSvgs(void)
{}

/*
void R_NewSvg(svgid_t id, const def_svgline_t* lines, uint numLines)
{
    DENG_UNUSED(id);
    DENG_UNUSED(lines);
    DENG_UNUSED(numLines);
}
*/

struct font_s* R_CreateFontFromDef(ded_compositefont_t* def)
{
    DENG_UNUSED(def);
    return 0;
}

void Rend_Init(void)
{}

void Rend_DecorInit()
{}

void Rend_CacheForMap()
{}

void Rend_CacheForMobjType(int num)
{
    DENG_UNUSED(num);
}

void Rend_CalcLightModRange()
{}

void Rend_ConsoleInit()
{}

void Rend_ConsoleOpen(int yes)
{
    DENG_UNUSED(yes);
}

void Rend_ConsoleMove(int y)
{
    DENG_UNUSED(y);
}

void Rend_ConsoleResize(int force)
{
    DENG_UNUSED(force);
}

void Rend_ConsoleToggleFullscreen()
{}

void Rend_ConsoleCursorResetBlink()
{}

void Sky_Ticker()
{}

void Models_Init()
{}

void Models_Shutdown()
{}

void Models_CacheForState(int stateIndex)
{
    DENG_UNUSED(stateIndex);
}

void SB_InitForMap(const char* uniqueID)
{
    DENG_UNUSED(uniqueID);
}

void SB_SurfaceMoved(biassurface_t* bsuf)
{
    DENG_UNUSED(bsuf);
}

void SB_DestroySurface(struct biassurface_s* bsuf)
{
    DENG_UNUSED(bsuf);
};

void LG_SectorChanged(Sector* sector)
{
    DENG_UNUSED(sector);
}

void Cl_InitPlayers(void)
{}

void UI_Ticker(timespan_t t)
{
    DENG_UNUSED(t);
}

/*
int DGL_Enable(int cap)
{
    return 0;
}

void DGL_Disable(int cap)
{}

boolean DGL_GetIntegerv(int name, int* vec)
{
    return false;
}

int DGL_GetInteger(int name)
{
    return 0;
}

boolean DGL_SetInteger(int name, int value)
{
    return false;
}

boolean DGL_GetFloatv(int name, float* vec)
{
    return false;
}

float DGL_GetFloat(int name)
{
    return 0;
}

boolean DGL_SetFloat(int name, float value)
{
    return false;
}

void DGL_Ortho(float left, float top, float right, float bottom, float znear, float zfar)
{}

void DGL_Scissor(RectRaw* rect)
{}

void DGL_SetScissor(const RectRaw* rect)
{}

void DGL_SetScissor2(int x, int y, int width, int height)
{}

void DGL_MatrixMode(int mode)
{}

void DGL_PushMatrix(void)
{}

void DGL_PopMatrix(void)
{}

void DGL_LoadIdentity(void)
{}

void DGL_Translatef(float x, float y, float z)
{}

void DGL_Rotatef(float angle, float x, float y, float z)
{}

void DGL_Scalef(float x, float y, float z)
{}

void DGL_Begin(dglprimtype_t type)
{}

void DGL_End(void)
{}

boolean DGL_NewList(DGLuint list, int mode)
{
    return false;
}

DGLuint DGL_EndList(void)
{
    return 0;
}

void DGL_CallList(DGLuint list)
{}

void DGL_DeleteLists(DGLuint list, int range)
{}

void DGL_SetNoMaterial(void) {}
void DGL_SetMaterialUI(struct material_s* mat, DGLint wrapS, DGLint wrapT) {}
void DGL_SetPatch(patchid_t id, DGLint wrapS, DGLint wrapT) {}
void DGL_SetPSprite(struct material_s* mat) {}
void DGL_SetPSprite2(struct material_s* mat, int tclass, int tmap) {}
void DGL_SetRawImage(lumpnum_t lumpNum, DGLint wrapS, DGLint wrapT) {}

void DGL_BlendOp(int op) {}
void DGL_BlendFunc(int param1, int param2) {}
void DGL_BlendMode(blendmode_t mode) {}

void DGL_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b) {}
void DGL_Color3ubv(const DGLubyte* vec) {}
void DGL_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a) {}
void DGL_Color4ubv(const DGLubyte* vec) {}
void DGL_Color3f(float r, float g, float b) {}
void DGL_Color3fv(const float* vec) {}
void DGL_Color4f(float r, float g, float b, float a) {}
void DGL_Color4fv(const float* vec) {}

void DGL_TexCoord2f(byte target, float s, float t) {}
void DGL_TexCoord2fv(byte target, float* vec) {}

void DGL_Vertex2f(float x, float y) {}
void DGL_Vertex2fv(const float* vec) {}
void DGL_Vertex3f(float x, float y, float z) {}
void DGL_Vertex3fv(const float* vec) {}
void DGL_Vertices2ftv(int num, const dgl_ft2vertex_t* vec) {}
void DGL_Vertices3ftv(int num, const dgl_ft3vertex_t* vec) {}
void DGL_Vertices3fctv(int num, const dgl_fct3vertex_t* vec) {}

void DGL_DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a) {}

void DGL_DrawRect(const RectRaw* rect) {}

void DGL_DrawRect2(int x, int y, int w, int h) {}

void DGL_DrawRectf(const RectRawf* rect) {}

void DGL_DrawRectf2(double x, double y, double w, double h) {}

void DGL_DrawRectf2Color(double x, double y, double w, double h, float r, float g, float b, float a) {}

void DGL_DrawRectf2Tiled(double x, double y, double w, double h, int tw, int th) {}

void DGL_DrawCutRectfTiled(const RectRawf* rect, int tw, int th, int txoff, int tyoff, const RectRawf* cutRect) {}

void DGL_DrawCutRectf2Tiled(double x, double y, double w, double h, int tw, int th, int txoff, int tyoff,
    double cx, double cy, double cw, double ch) {}

void DGL_DrawQuadOutline(const Point2Raw* tl, const Point2Raw* tr, const Point2Raw* br,
    const Point2Raw* bl, const float color[4]) {}

void DGL_DrawQuad2Outline(int tlX, int tlY, int trX, int trY, int brX, int brY, int blX, int blY,
    const float color[4]) {}

DGLuint DGL_NewTextureWithParams(dgltexformat_t format, int width, int height,
    const uint8_t* pixels, int flags, int minFilter, int magFilter,
    int anisoFilter, int wrapS, int wrapT)
{
    return 0;
}

int DGL_Bind(DGLuint texture)
{
    return 0;
}

void DGL_DeleteTextures(int num, const DGLuint* names)
{
    DENG_UNUSED(num);
    DENG_UNUSED(names);
}
*/

void Sys_MessageBox(messageboxtype_t type, const char* title, const char* msg, const char* detailedMsg)
{
    DENG_UNUSED(type);
    DENG_UNUSED(title);
    DENG_UNUSED(msg);
    DENG_UNUSED(detailedMsg);
}

void Sys_MessageBox2(messageboxtype_t type, const char *title, const char *msg, const char *informativeMsg, const char *detailedMsg)
{
}

void Sys_MessageBoxf(messageboxtype_t type, const char* title, const char* format, ...)
{
}

int Sys_MessageBoxWithButtons(messageboxtype_t type, const char* title, const char* msg,
                              const char* informativeMsg, const char** buttons)
{
    return 0;
}

void Sys_MessageBoxWithDetailsFromFile(messageboxtype_t type, const char* title, const char* msg,
                                       const char* informativeMsg, const char* detailsFileName)
{
}

coord_t R_VisualRadius(struct mobj_s *mo)
{
    DENG_UNUSED(mo);
    return 0;
}

void R_ProjectSprite(struct mobj_s *mo)
{
    DENG_UNUSED(mo);
}

void Rend_ApplyLightAdaptation(float* val)
{
    DENG_UNUSED(val);
}
