/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * doomsday.h: Doomsday Engine API (Routines exported from Doomsday.exe).
 *
 * Games and plugins need to include this to gain access to the engine's
 * features.
 */

#ifndef __DOOMSDAY_EXPORTS_H__
#define __DOOMSDAY_EXPORTS_H__

#include "dd_export.h"

/**
 * Public definitions of the internal map data pointers.  These can be
 * accessed externally, but only as identifiers to data instances.
 * For example, a game could use sector_t to identify to sector to
 * change with the Map Update API.
 *
 * Define __INTERNAL_MAP_DATA_ACCESS__ if access to the internal map data
 * structures is needed.
 */
#ifndef __INTERNAL_MAP_DATA_ACCESS__
    typedef struct node_s { int type; } node_t;
    typedef struct vertex_s {int type; } vertex_t;
    typedef struct linedef_s { int type; } linedef_t;
    typedef struct sidedef_s { int type; } sidedef_t;
    typedef struct seg_s { int type; } seg_t;
    typedef struct subsector_s { int type; } subsector_t;
    typedef struct sector_s { int type; } sector_t;
    typedef struct plane_s { int type; } plane_t;
    typedef struct material_s { int type; } material_t;
#endif

#include "dd_share.h"
#include "dd_plugin.h"

    struct ded_s;
    struct ded_count_s;

#ifdef __INTERNAL_MAP_DATA_ACCESS__
#   include "../portable/include/p_maptypes.h"
#endif

#ifdef __cplusplus
extern          "C" {
#endif

    DENG_API int             DD_Entry(int argc, char *argv[]);
    DENG_API void            DD_Shutdown(void);
    DENG_API int             DD_GameLoop(void);
    
    // Base.
    DENG_API void            DD_AddIWAD(const char* path);
    DENG_API void            DD_AddStartupWAD(const char* file);
    DENG_API void            DD_SetConfigFile(const char* filename);
    DENG_API void            DD_SetDefsFile(const char* file);
    DENG_API int _DECALL     DD_GetInteger(int ddvalue);
    DENG_API void            DD_SetInteger(int ddvalue, int parm);
    DENG_API void            DD_SetVariable(int ddvalue, void* ptr);
    DENG_API void*           DD_GetVariable(int ddvalue);
    DENG_API ddplayer_t*     DD_GetPlayer(int number);

    // Base: Definitions.
    DENG_API int             Def_Get(int type, const char* id, void* out);
    DENG_API int             Def_Set(int type, int index, int value, void* ptr);
    DENG_API int             Def_EvalFlags(char* flags);
	DENG_API int             DED_AddValue(struct ded_s *ded, const char* id);
	DENG_API void*           DED_NewEntries(void** ptr, struct ded_count_s* cnt,
											  size_t elemSize, int count);

    // Base: Input.
    DENG_API void            DD_ClearKeyRepeaters(void);
    DENG_API int             DD_GetKeyCode(const char* name);

    // Base: WAD.
    DENG_API lumpnum_t       W_CheckNumForName(const char* name);
    DENG_API lumpnum_t       W_GetNumForName(const char* name);
    DENG_API size_t          W_LumpLength(lumpnum_t lump);
    DENG_API const char*     W_LumpName(lumpnum_t lump);
    DENG_API void            W_ReadLump(lumpnum_t lump, void* dest);
    DENG_API void            W_ReadLumpSection(lumpnum_t lump, void* dest,
                                      size_t startOffset, size_t length);
    DENG_API const void*     W_CacheLumpNum(lumpnum_t lump, int tag);
    DENG_API const void*     W_CacheLumpName(const char* name, int tag);
    DENG_API void            W_ChangeCacheTag(lumpnum_t lump, int tag);
    DENG_API const char*     W_LumpSourceFile(lumpnum_t lump);
    DENG_API uint            W_CRCNumber(void);
    DENG_API boolean         W_IsFromIWAD(lumpnum_t lump);
    DENG_API lumpnum_t       W_OpenAuxiliary(const char* fileName);

    // Base: Zone.
    DENG_API void* _DECALL   Z_Malloc(size_t size, int tag, void* ptr);
    DENG_API void* _DECALL   Z_Calloc(size_t size, int tag, void* user);
    DENG_API void*           Z_Realloc(void* ptr, size_t n, int mallocTag);
    DENG_API void*           Z_Recalloc(void* ptr, size_t n, int callocTag);
    DENG_API void _DECALL    Z_Free(void* ptr);
    DENG_API void            Z_FreeTags(int lowTag, int highTag);
    DENG_API void            Z_ChangeTag2(void* ptr, int tag);
    DENG_API void            Z_CheckHeap(void);

    // Console.
    DENG_API int             Con_Busy(int flags, const char* taskName,
                             int (*workerFunc)(void*), void* workerData);
    DENG_API void            Con_BusyWorkerEnd(void);
    DENG_API boolean         Con_IsBusy(void);
    DENG_API void            Con_Open(int yes);
    DENG_API void            Con_SetFont(ddfont_t* cfont);
    DENG_API void            Con_AddCommand(ccmd_t* cmd);
    DENG_API void            Con_AddVariable(cvar_t* var);
    DENG_API void            Con_AddCommandList(ccmd_t* cmdlist);
    DENG_API void            Con_AddVariableList(cvar_t* varlist);
    DENG_API cvar_t*         Con_GetVariable(const char* name);
    DENG_API byte            Con_GetByte(const char* name);
    DENG_API int             Con_GetInteger(const char* name);
    DENG_API float           Con_GetFloat(const char* name);
    DENG_API char*           Con_GetString(const char* name);
    DENG_API void            Con_SetInteger(const char* name, int value,
                                   byte override);
    DENG_API void            Con_SetFloat(const char* name, float value,
                                 byte override);
    DENG_API void            Con_SetString(const char* name, char* text,
                                  byte override);
    DENG_API void            Con_Printf(const char* format, ...) PRINTF_F(1,2);
    DENG_API void            Con_FPrintf(int flags, const char* format, ...) PRINTF_F(2,3);
    DENG_API int             DD_Execute(int silent, const char* command);
    DENG_API int             DD_Executef(int silent, const char* command, ...);
    DENG_API void            Con_Message(const char* message, ...) PRINTF_F(1,2);
    DENG_API void            Con_Error(const char* error, ...) PRINTF_F(1,2);

    // Console: Bindings.
    DENG_API void            B_SetContextFallback(const char* name,
                                         int (*responderFunc)(event_t*));
    DENG_API int             B_BindingsForCommand(const char* cmd, char* buf,
                                         size_t bufSize);
    DENG_API int             B_BindingsForControl(int localPlayer,
                                         const char* controlName,
                                         int inverse,
                                         char* buf, size_t bufSize);

    // System.
    DENG_API void            Sys_TicksPerSecond(float num);
    DENG_API int             Sys_GetTime(void);
    DENG_API double          Sys_GetSeconds(void);
    DENG_API uint            Sys_GetRealTime(void);
    DENG_API void            Sys_Sleep(int millisecs);
    DENG_API int             Sys_CriticalMessage(char* msg);
    DENG_API void            Sys_Quit(void);

    // System: Files.
    DENG_API int             F_Access(const char* path);
    DENG_API unsigned int    F_LastModified(const char* fileName);

    // Map building interface.
    DENG_API boolean         MPE_Begin(const char* name);
    DENG_API boolean         MPE_End(void);

    DENG_API uint            MPE_VertexCreate(float x, float y);
    DENG_API boolean         MPE_VertexCreatev(size_t num, float* values, uint* indices);
    DENG_API uint            MPE_SidedefCreate(uint sector, short flags,
                                      materialnum_t topMaterial,
                                      float topOffsetX, float topOffsetY, float topRed,
                                      float topGreen, float topBlue,
                                      materialnum_t middleMaterial,
                                      float middleOffsetX, float middleOffsetY,
                                      float middleRed, float middleGreen,
                                      float middleBlue, float middleAlpha,
                                      materialnum_t bottomMaterial,
                                      float bottomOffsetX, float bottomOffsetY,
                                      float bottomRed, float bottomGreen,
                                      float bottomBlue);
    DENG_API uint            MPE_LinedefCreate(uint v1, uint v2, uint frontSide,
                                      uint backSide, int flags);
    DENG_API uint            MPE_SectorCreate(float lightlevel, float red, float green, float blue);
    DENG_API uint            MPE_PlaneCreate(uint sector, float height,
                                    materialnum_t num,
                                    float matOffsetX, float matOffsetY,
                                    float r, float g, float b, float a,
                                    float normalX, float normalY, float normalZ);
    DENG_API uint            MPE_PolyobjCreate(uint* lines, uint linecount,
                                      int tag, int sequenceType, float anchorX, float anchorY);
    DENG_API boolean         MPE_GameObjProperty(const char* objName, uint idx,
                                        const char* propName, valuetype_t type,
                                        void* data);

    // Custom map object data types.
    DENG_API boolean         P_RegisterMapObj(int identifier, const char* name);
    DENG_API boolean         P_RegisterMapObjProperty(int identifier, int propIdentifier,
                                             const char* propName, valuetype_t type);

    // Network.
    DENG_API void            Net_SendPacket(int to_player, int type, void* data,
                                   size_t length);
    DENG_API int             Net_GetTicCmd(void* command, int player);
    DENG_API const char*     Net_GetPlayerName(int player);
    DENG_API ident_t         Net_GetPlayerID(int player);

    // Play.
    DENG_API float           P_AccurateDistance(float dx, float dy);
    DENG_API float           P_ApproxDistance(float dx, float dy);
    DENG_API float           P_ApproxDistance3(float dx, float dy, float dz);
    DENG_API int             P_PointOnLinedefSide(float x, float y,
                                         const struct linedef_s* line);
    DENG_API int             P_BoxOnLineSide(const float* tmbox, const struct linedef_s* ld);
    DENG_API void            P_MakeDivline(const struct linedef_s* li, divline_t* dl);
    DENG_API int             P_PointOnDivlineSide(float x, float y,
                                         const divline_t* line);
    DENG_API float           P_InterceptVector(const divline_t* v2, const divline_t* v1);
    DENG_API void            P_LineOpening(struct linedef_s* linedef);

    // Object in bounding box iterators.
    DENG_API boolean         P_MobjsBoxIterator(const float box[4],
                                       boolean (*func) (struct mobj_s*, void*),
                                       void* data);
    DENG_API boolean         P_LinesBoxIterator(const float box[4],
                                       boolean (*func) (struct linedef_s*, void*),
                                       void* data);
    DENG_API boolean         P_AllLinesBoxIterator(const float box[4],
                                          boolean (*func) (struct linedef_s*, void*),
                                          void* data);
    DENG_API boolean         P_SubsectorsBoxIterator(const float box[4], sector_t* sector,
                                           boolean (*func) (subsector_t*, void*),
                                           void* data);
    DENG_API boolean         P_PolyobjsBoxIterator(const float box[4],
                                          boolean (*func) (struct polyobj_s*, void*),
                                          void* data);

    // Object type touching mobjs iterators.
    DENG_API boolean         P_LineMobjsIterator(struct linedef_s* line,
                                        boolean (*func) (struct mobj_s*,
                                                         void *), void* data);
    DENG_API boolean         P_SectorTouchingMobjsIterator
                        (sector_t* sector, boolean (*func) (struct mobj_s*, void*),
                         void* data);

    DENG_API boolean         P_PathTraverse(float x1, float y1, float x2, float y2,
                                   int flags,
                                   boolean (*trav) (intercept_t*));
    DENG_API boolean         P_CheckLineSight(const float from[3], const float to[3],
                                     float bottomSlope, float topSlope);

    // Play: Controls.
    DENG_API void            P_NewPlayerControl(int id, controltype_t type, const char* name, const char* bindContext);
    DENG_API void            P_GetControlState(int playerNum, int control, float* pos, float* relativeOffset);
    DENG_API int             P_GetImpulseControlState(int playerNum, int control);

    // Play: Setup.
    DENG_API boolean         P_LoadMap(const char* mapID);

    // Play: Map Data Updates and Information Access.
    DENG_API unsigned int    P_ToIndex(const void* ptr);
    DENG_API void*           P_ToPtr(int type, uint index);
    DENG_API int             P_Callback(int type, uint index, void* context,
                               int (*callback)(void* p, void* ctx));
    DENG_API int             P_Callbackp(int type, void* ptr, void* context,
                                int (*callback)(void* p, void* ctx));
    DENG_API int             P_Iteratep(void *ptr, uint prop, void* context,
                               int (*callback) (void* p, void* ctx));

    /* dummy functions */
    DENG_API void*           P_AllocDummy(int type, void* extraData);
    DENG_API void            P_FreeDummy(void* dummy);
    DENG_API int             P_DummyType(void* dummy);
    DENG_API boolean         P_IsDummy(void* dummy);
    DENG_API void*           P_DummyExtraData(void* dummy);

    /* index-based write functions */
    DENG_API void            P_SetBool(int type, uint index, uint prop, boolean param);
    DENG_API void            P_SetByte(int type, uint index, uint prop, byte param);
    DENG_API void            P_SetInt(int type, uint index, uint prop, int param);
    DENG_API void            P_SetFixed(int type, uint index, uint prop, fixed_t param);
    DENG_API void            P_SetAngle(int type, uint index, uint prop, angle_t param);
    DENG_API void            P_SetFloat(int type, uint index, uint prop, float param);
    DENG_API void            P_SetPtr(int type, uint index, uint prop, void* param);

    DENG_API void            P_SetBoolv(int type, uint index, uint prop, boolean* params);
    DENG_API void            P_SetBytev(int type, uint index, uint prop, byte* params);
    DENG_API void            P_SetIntv(int type, uint index, uint prop, int* params);
    DENG_API void            P_SetFixedv(int type, uint index, uint prop, fixed_t* params);
    DENG_API void            P_SetAnglev(int type, uint index, uint prop, angle_t* params);
    DENG_API void            P_SetFloatv(int type, uint index, uint prop, float* params);
    DENG_API void            P_SetPtrv(int type, uint index, uint prop, void* params);
    
    /* pointer-based write functions */
    DENG_API void            P_SetBoolp(void* ptr, uint prop, boolean param);
    DENG_API void            P_SetBytep(void* ptr, uint prop, byte param);
    DENG_API void            P_SetIntp(void* ptr, uint prop, int param);
    DENG_API void            P_SetFixedp(void* ptr, uint prop, fixed_t param);
    DENG_API void            P_SetAnglep(void* ptr, uint prop, angle_t param);
    DENG_API void            P_SetFloatp(void* ptr, uint prop, float param);
    DENG_API void            P_SetPtrp(void* ptr, uint prop, void* param);

    DENG_API void            P_SetBoolpv(void* ptr, uint prop, boolean* params);
    DENG_API void            P_SetBytepv(void* ptr, uint prop, byte* params);
    DENG_API void            P_SetIntpv(void* ptr, uint prop, int* params);
    DENG_API void            P_SetFixedpv(void* ptr, uint prop, fixed_t* params);
    DENG_API void            P_SetAnglepv(void* ptr, uint prop, angle_t* params);
    DENG_API void            P_SetFloatpv(void* ptr, uint prop, float* params);
    DENG_API void            P_SetPtrpv(void* ptr, uint prop, void* params);

    /* index-based read functions */
    DENG_API boolean         P_GetBool(int type, uint index, uint prop);
    DENG_API byte            P_GetByte(int type, uint index, uint prop);
    DENG_API int             P_GetInt(int type, uint index, uint prop);
    DENG_API fixed_t         P_GetFixed(int type, uint index, uint prop);
    DENG_API angle_t         P_GetAngle(int type, uint index, uint prop);
    DENG_API float           P_GetFloat(int type, uint index, uint prop);
    DENG_API void*           P_GetPtr(int type, uint index, uint prop);

    DENG_API void            P_GetBoolv(int type, uint index, uint prop, boolean* params);
    DENG_API void            P_GetBytev(int type, uint index, uint prop, byte* params);
    DENG_API void            P_GetIntv(int type, uint index, uint prop, int* params);
    DENG_API void            P_GetFixedv(int type, uint index, uint prop, fixed_t* params);
    DENG_API void            P_GetAnglev(int type, uint index, uint prop, angle_t* params);
    DENG_API void            P_GetFloatv(int type, uint index, uint prop, float* params);
    DENG_API void            P_GetPtrv(int type, uint index, uint prop, void* params);

    /* pointer-based read functions */
    DENG_API boolean         P_GetBoolp(void* ptr, uint prop);
    DENG_API byte            P_GetBytep(void* ptr, uint prop);
    DENG_API int             P_GetIntp(void* ptr, uint prop);
    DENG_API fixed_t         P_GetFixedp(void* ptr, uint prop);
    DENG_API angle_t         P_GetAnglep(void* ptr, uint prop);
    DENG_API float           P_GetFloatp(void* ptr, uint prop);
    DENG_API void*           P_GetPtrp(void* ptr, uint prop);

    DENG_API void            P_GetBoolpv(void* ptr, uint prop, boolean* params);
    DENG_API void            P_GetBytepv(void* ptr, uint prop, byte* params);
    DENG_API void            P_GetIntpv(void* ptr, uint prop, int* params);
    DENG_API void            P_GetFixedpv(void* ptr, uint prop, fixed_t* params);
    DENG_API void            P_GetAnglepv(void* ptr, uint prop, angle_t* params);
    DENG_API void            P_GetFloatpv(void* ptr, uint prop, float* params);
    DENG_API void            P_GetPtrpv(void* ptr, uint prop, void* params);

    DENG_API uint            P_CountGameMapObjs(int identifier);
    DENG_API byte            P_GetGMOByte(int identifier, uint elmIdx, int propIdentifier);
    DENG_API short           P_GetGMOShort(int identifier, uint elmIdx, int propIdentifier);
    DENG_API int             P_GetGMOInt(int identifier, uint elmIdx, int propIdentifier);
    DENG_API fixed_t         P_GetGMOFixed(int identifier, uint elmIdx, int propIdentifier);
    DENG_API angle_t         P_GetGMOAngle(int identifier, uint elmIdx, int propIdentifier);
    DENG_API float           P_GetGMOFloat(int identifier, uint elmIdx, int propIdentifier);

    // Play: Misc.
    DENG_API void            P_MergeCommand(ticcmd_t* dest, ticcmd_t* src); // temporary.
    DENG_API void            P_SpawnDamageParticleGen(struct mobj_s* mo,
                                             struct mobj_s* inflictor,
                                             int amount);
    
    // Play: Mobjs.
    DENG_API struct mobj_s*  P_MobjCreate(think_t function, float x, float y, float z,
                                  angle_t angle, float radius, float height, int ddflags);
    DENG_API void            P_MobjDestroy(struct mobj_s* mo);
    DENG_API void            P_MobjSetState(struct mobj_s* mo, int statenum);
    DENG_API void            P_MobjLink(struct mobj_s* mo, byte flags);
    DENG_API int             P_MobjUnlink(struct mobj_s* mo);
    
    // Mobj linked object iterators.
    DENG_API boolean         P_MobjLinesIterator(struct mobj_s* mo,
                                         boolean (*func) (struct linedef_s*,
                                                           void*), void*);
    DENG_API boolean         P_MobjSectorsIterator(struct mobj_s* mo,
                                           boolean (*func) (sector_t*, void*),
                                           void* data);
     
    // Play: Polyobjs.
    DENG_API boolean         P_PolyobjMove(struct polyobj_s* po, float x, float y);
    DENG_API boolean         P_PolyobjRotate(struct polyobj_s* po, angle_t angle);
    DENG_API void            P_PolyobjLink(struct polyobj_s* po);
    DENG_API void            P_PolyobjUnLink(struct polyobj_s* po);
     
    DENG_API struct polyobj_s* P_GetPolyobj(uint num);
    DENG_API void            P_SetPolyobjCallback(void (*func)(struct mobj_s*, void*, void*));
     
    // Play: Materials.
    DENG_API materialnum_t   P_MaterialCheckNumForName(const char* name, material_namespace_t mnamespace);
    DENG_API materialnum_t   P_MaterialNumForName(const char* name, material_namespace_t mnamespace);
    DENG_API materialnum_t   P_MaterialCheckNumForIndex(uint idx, material_namespace_t mnamespace);
    DENG_API materialnum_t   P_MaterialNumForIndex(uint idx, material_namespace_t mnamespace);
    DENG_API const char*     P_GetMaterialName(material_t* mat);
     
    DENG_API void            P_MaterialPrecache(material_t* mat);
        
    // Refresh.
    DENG_API float           DD_GetFrameRate(void);
    DENG_API void            R_SetDataPath(const char* path);
    DENG_API void            R_SetupMap(int mode, int flags);
    DENG_API void            R_PrecacheMap(void);
    DENG_API void            R_PrecacheMobjNum(int mobjtypeNum);
    DENG_API void            R_PrecachePatch(lumpnum_t lump);
    DENG_API void            R_PrecacheSkinsForState(int stateIndex);
    DENG_API void            R_RenderPlayerView(int num);
    DENG_API void            R_SetViewWindow(int x, int y, int w, int h);
    DENG_API int             R_GetViewPort(int player, int* x, int* y, int* w, int* h);
    DENG_API void            R_SetBorderGfx(char* lumps[9]);
    DENG_API boolean         R_GetSpriteInfo(int sprite, int frame,
                                     spriteinfo_t* sprinfo);
    DENG_API boolean         R_GetPatchInfo(lumpnum_t lump, patchinfo_t* info);
    DENG_API int             R_CreateAnimGroup(int flags);
    DENG_API void            R_AddToAnimGroup(int groupNum, materialnum_t num,
                                      int tics, int randomTics);
    DENG_API void            R_HSVToRGB(float* rgb, float h, float s, float v);
    DENG_API angle_t         R_PointToAngle2(float x1, float y1, float x2, float y2);
    DENG_API struct subsector_s* R_PointInSubsector(float x, float y);
     
    DENG_API colorpaletteid_t R_CreateColorPalette(const char* fmt, const char* name,
                                           const byte* data, size_t num);
    DENG_API colorpaletteid_t R_GetColorPaletteNumForName(const char* name);
    DENG_API const char*     R_GetColorPaletteNameForNum(colorpaletteid_t id);
    DENG_API void            R_GetColorPaletteRGBf(colorpaletteid_t id, float rgb[3],
                                           int idx, boolean correctGamma);
     
    // Renderer.
    DENG_API void            Rend_Reset(void);
    DENG_API void            Rend_SkyParams(int layer, int param, void* data);
     
    // Graphics.
    DENG_API void            GL_UseFog(int yes);

	// Returns a pointer to a copy of the screen. The pointer must be
	// deallocated by the caller.
	DENG_API byte*           GL_GrabScreen(void);

	DENG_API DGLuint         GL_LoadGraphics(ddresourceclass_t resClass,
                                      const char* name, gfxmode_t mode,
                                      int useMipmap, boolean clamped,
                                      int otherFlags);
    DENG_API DGLuint         GL_NewTextureWithParams3(int format, int width,
                                              int height, void* pixels,
                                              int flags, int minFilter,
                                              int magFilter, int anisoFilter,
                                              int wrapS, int wrapT);
    DENG_API void            GL_SetFilter(boolean enable);
    DENG_API void            GL_SetFilterColor(float r, float g, float b, float a);
     
    // Graphics: 2D drawing.
    DENG_API void            GL_DrawPatch(int x, int y, lumpnum_t lump);
    DENG_API void            GL_DrawPatch_CS(int x, int y, lumpnum_t lump);
    DENG_API void            GL_DrawPatchLitAlpha(int x, int y, float light,
                                          float alpha, lumpnum_t lump);
    DENG_API void            GL_DrawFuzzPatch(int x, int y, lumpnum_t lump);
    DENG_API void            GL_DrawAltFuzzPatch(int x, int y, lumpnum_t lump);
    DENG_API void            GL_DrawShadowedPatch(int x, int y, lumpnum_t lump);
    DENG_API void            GL_DrawRawScreen(lumpnum_t lump, float offx,
                                      float offy);
    DENG_API void            GL_DrawRawScreen_CS(lumpnum_t lump, float offx,
                                         float offy, float scalex,
                                         float scaley);
     
    // Audio.
    DENG_API void            S_MapChange(void);
    DENG_API int             S_LocalSoundAtVolumeFrom(int sound_id,
                                              struct mobj_s* origin,
                                              float* pos, float volume);
    DENG_API int             S_LocalSoundAtVolume(int soundID, struct mobj_s* origin,
                                          float volume);
    DENG_API int             S_LocalSound(int soundID, struct mobj_s* origin);
    DENG_API int             S_LocalSoundFrom(int soundID, float* fixedpos);
    DENG_API int             S_StartSound(int soundId, struct mobj_s* origin);
    DENG_API int             S_StartSoundEx(int soundId, struct mobj_s* origin);
    DENG_API int             S_StartSoundAtVolume(int soundID, struct mobj_s* origin,
                                          float volume);
    DENG_API int             S_ConsoleSound(int soundID, struct mobj_s* origin,
                                    int targetConsole);
    DENG_API void            S_StopSound(int soundID, struct mobj_s* origin);
    DENG_API int             S_IsPlaying(int soundID, struct mobj_s* origin);
    DENG_API int             S_StartMusic(const char* musicID, boolean looped);
    DENG_API int             S_StartMusicNum(int id, boolean looped);
    DENG_API void            S_StopMusic(void);
     
    // Miscellaneous.
    DENG_API size_t          M_ReadFile(const char* name, byte** buffer);
    DENG_API size_t          M_ReadFileCLib(const char* name, byte** buffer);
    DENG_API boolean         M_WriteFile(const char* name, void* source,
                                 size_t length);
    DENG_API void            M_ExtractFileBase(char* dest, const char* path,
                                       size_t len);
    DENG_API char*           M_FindFileExtension(char* path);
    DENG_API boolean         M_CheckPath(const char* path);
    DENG_API int             M_FileExists(const char* file);
    DENG_API void            M_TranslatePath(char* translated, const char* path,
                                     size_t len);
    DENG_API const char*     M_PrettyPath(const char* path);
    DENG_API char*           M_SkipWhite(char* str);
    DENG_API char*           M_FindWhite(char* str);
    DENG_API char*           M_StrCatQuoted(char* dest, const char* src, size_t len);
    DENG_API byte            RNG_RandByte(void);
    DENG_API float           RNG_RandFloat(void);
    DENG_API void            M_ClearBox(fixed_t* box);
    DENG_API void            M_AddToBox(fixed_t* box, fixed_t x, fixed_t y);
    DENG_API int             M_ScreenShot(const char* filename, int bits);
    DENG_API int             M_CeilPow2(int num);
     
    // Miscellaneous: Time utilities.
    DENG_API boolean         M_RunTrigger(trigger_t* trigger, timespan_t advanceTime);
    DENG_API boolean         M_CheckTrigger(const trigger_t* trigger, timespan_t advanceTime);
     
    // Miscellaneous: Math.
    DENG_API void            V2_Rotate(float vec[2], float radians);
    DENG_API float           M_PointLineDistance(const float* a, const float* b, const float* c);
    DENG_API float           M_ProjectPointOnLine(const float* point, const float* linepoint,
                                          const float* delta, float gap, float* result);
    DENG_API binangle_t      bamsAtan2(int y, int x);
     
    // Miscellaneous: Command line.
    DENG_API void _DECALL    ArgAbbreviate(const char* longName, const char* shortName);
    DENG_API int _DECALL     Argc(void);
    DENG_API const char* _DECALL Argv(int i);
    DENG_API const char** _DECALL ArgvPtr(int i);
    DENG_API const char* _DECALL ArgNext(void);
    DENG_API int _DECALL     ArgCheck(const char* check);
    DENG_API int _DECALL     ArgCheckWith(const char* check, int num);
    DENG_API int _DECALL     ArgExists(const char* check);
    DENG_API int _DECALL     ArgIsOption(int i);

#ifdef WIN32
	DENG_API int				snprintf(char* str, size_t size, const char* format, ...);
#endif

#ifdef __cplusplus
}    
#endif
#endif
