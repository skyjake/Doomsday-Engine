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
    return false;
}

struct mobj_s* ClPlayer_ClMobj(int plrNum)
{
    DENG_UNUSED(plrNum);
    return 0;
}

void Con_TransitionRegister()
{}

void Con_TransitionTicker(timespan_t t)
{
    DENG_UNUSED(t);
}

void GL_Shutdown()
{}

void GL_EarlyInitTextureManager()
{}

void GL_PruneTextureVariantSpecifications()
{}

void GL_SetFilter(int f)
{
    DENG_UNUSED(f);
}

void FR_Init(void)
{}

void Fonts_Init(void)
{}

void R_InitViewWindow(void)
{}

void R_InitSvgs(void)
{}

void R_ShutdownSvgs(void)
{}

void Rend_Init(void)
{}

void Rend_DecorInit()
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

void Models_Init()
{}

void Models_Shutdown()
{}

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

void UI2_Ticker(timespan_t t)
{
    DENG_UNUSED(t);
}

void UI_Init()
{}

void UI_Shutdown()
{}

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
