/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * r_model.c: 3D Model Resources
 *
 * MD2/DMD2 loading and setup.
 * My variable naming convention is a bit incoherent.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

#include <ctype.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_misc.h"

#include "def_main.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
	PROF_GET_MODEL_DEF,
	PROF_DATA_INIT,
	PROF_LOAD_MODEL,
	PROF_REGISTER_SKIN,
	PROF_SCALING,
	PROF_LM_FIND_MODEL,
	PROF_LM_LOADERS,
	PROF_LM_SKINS
END_PROF_TIMERS()

// TYPES -------------------------------------------------------------------

typedef struct {
	float pos[3];
} vector_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float		rModelAspectMod = 1/1.2f; //.833334f;

// The dummy is used for model zero.
model_t		dummy = { true, "Dummy-Dummy" };

model_t		*modellist[MAX_MODELS] = { &dummy };
int			useModels = true;	

modeldef_t	*models = NULL;	// Confusing; should be called modefs.
int			nummodels, maxmodels;

ddstring_t	modelPath;

float avertexnormals[NUMVERTEXNORMALS][3] = {
#include "tab_anorms.h"
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// UnpackVector
//	Packed: pppppppy yyyyyyyy. Yaw is on the XY plane.
//===========================================================================
void UnpackVector(unsigned short packed, float vec[3])
{
	float yaw = (packed & 511)/512.0f * 2*PI;
	float pitch = ((packed >> 9)/127.0f - 0.5f) * PI;
	float cosp = (float) cos(pitch);
	vec[VX] = (float) cos(yaw) * cosp;
	vec[VY] = (float) sin(yaw) * cosp;
	vec[VZ] = (float) sin(pitch);	
}

//===========================================================================
// R_FindModelFor
//	Returns an index if the specified model has already been loaded.
//===========================================================================
int R_FindModelFor(const char *filename)
{
	int		i;

	for(i = 0; i < MAX_MODELS; i++)
		if(modellist[i] && !stricmp(modellist[i]->fileName, filename))
			return i;
	return -1;
}

//===========================================================================
// R_NewModelFor
//	Allocates a new model. Returns the index.
//===========================================================================
int R_NewModelFor(const char *filename)
{
	int		i;

	// Take the first empty spot.
	for(i = 0; i < MAX_MODELS; i++)
		if(!modellist[i])
		{
			modellist[i] = calloc(sizeof(model_t), 1);
			return i;
		}
	// Dang, we're out of models.
	return -1;
}

/*
//===========================================================================
// R_VertexNormals
//	Calculate vertex normals. Only with -renorm.
//===========================================================================
void R_VertexNormals(model_t *mdl)
{
	int tris = mdl->lodInfo[0].numTriangles;
	int verts = mdl->info.numVertices;
	int i, k, j, n, cnt;
	vector_t *normals, norm;
	model_vertex_t *list;
	dmd_triangle_t *tri;
//	float maxprod, diff;
//	int idx;

	if(!ArgCheck("-renorm")) return; // Renormalizing?

	normals = Z_Malloc(sizeof(vector_t) * tris, PU_STATIC, 0);

	// Calculate the normal for each vertex.
	for(i = 0; i < mdl->info.numFrames; i++)
	{
		list = mdl->frames[i].vertices;
		for(k = 0; k < tris; k++)
		{
			tri = mdl->lods[0].triangles + k;
			// First calculate surface normals, combine them to vertex ones.
			M_PointCrossProduct(list[tri->vertexIndices[0]].vertex,
				list[tri->vertexIndices[2]].vertex,
				list[tri->vertexIndices[1]].vertex, normals[k].pos);
			M_Normalize(normals[k].pos);
		}
		for(k = 0; k < verts; k++)
		{
			memset(&norm, 0, sizeof(norm));
			for(j = 0, cnt = 0; j < tris; j++)
			{
				tri = mdl->lods[0].triangles + j;
				for(n = 0; n < 3; n++)
					if(tri->vertexIndices[n] == k)
					{
						cnt++;
						for(n = 0; n < 3; n++)
							norm.pos[n] += normals[j].pos[n];
						break;
					}
			}
			if(!cnt) continue; // Impossible...
			// Calculate the average.
			for(n = 0; n < 3; n++) norm.pos[n] /= cnt;
			// Normalize it.
			M_Normalize(norm.pos);
			memcpy(list[k].normal, norm.pos, sizeof(norm.pos));
		}
	}

	Z_Free(normals);
}
*/

//===========================================================================
// AllocAndLoad
//===========================================================================
static void *AllocAndLoad(DFILE *file, int offset, int len)
{
	void *ptr = malloc(len);
	F_Seek(file, offset, SEEK_SET);
	F_Read(ptr, len, file);
	return ptr;
}

//===========================================================================
// R_ClearModelPath
//===========================================================================
void R_ClearModelPath(void)
{
	// Free the old path.
	Str_Free(&modelPath);
}

//===========================================================================
// R_AddModelPath
//	Appends or prepends a new path to the list of model search paths.
//	-modeldir prepends, DED files append search paths.
//===========================================================================
void R_AddModelPath(char *addPath, boolean append)
{
	// Compile the new search path.
	if(append)
	{
		Str_Append(&modelPath, ";");
		Str_Append(&modelPath, addPath);
	}
	else
	{
		Str_Prepend(&modelPath, ";");
		Str_Prepend(&modelPath, addPath);
	}
}

//===========================================================================
// R_FindModelFile
//	Returns true if the file was found.
//===========================================================================
int R_FindModelFile(const char *filename, char *outfn)
{
	char buf[256], ext[50]; 

	if(!filename || !filename[0]) return false;

	// DMD takes precedence over MD2.
	strcpy(buf, filename);
	M_GetFileExt(buf, ext);
	if(!stricmp(ext, "md2"))
	{
		// Swap temporarily to DMD and see if it exists.
		M_ReplaceFileExt(buf, "dmd");
		if(FH_Find(buf, outfn)) 
			return true;
		M_ReplaceFileExt(buf, "md2");
	}

	// Try finding the given file name.
	if(FH_Find(buf, outfn))
		return true;

	// How about alternative extensions?
	// If DMD is not found, try MD2 instead.
	if(!stricmp(ext, "dmd"))
	{
		M_ReplaceFileExt(buf, "md2");
		if(FH_Find(buf, outfn))
			return true;
	}

	// Try loading using the base path as the starting point.
	if(!Dir_IsAbsolute(filename))
	{
		strcpy(buf, ddBasePath);
		strcat(buf, filename);
		if(F_Access(buf)) 
		{
			VERBOSE2( Con_Printf("R_FindModelFile: "
				"Base path hit: %s\n", buf) );

			strcpy(outfn, buf);
			return true; // Got it.
		}
	}

	// No success.
	return false;
}

//===========================================================================
// R_OpenModelFile
//	Searches the model paths for the file and opens it.
//===========================================================================
DFILE *R_OpenModelFile(char *filename)
{
	char buf[256];

	if(R_FindModelFile(filename, buf)) return F_Open(buf, "rb");
	return NULL;
}

//===========================================================================
// R_MissingModel
//===========================================================================
void R_MissingModel(const char *fn)
{
/*	char *ptr = missing.str, *end;
	char temp[256];

	// Is the filename already listed (ignore case).
	while(*ptr)
	{
		ptr = M_SkipWhite(ptr);
		end = M_FindWhite(ptr);
		strncpy(temp, ptr, end - ptr);
		temp[end - ptr] = 0;
		if(!stricmp(temp, fn)) return; // Already there.
		ptr = end;
	}

	Str_Append(&missing, "  ");
	Str_Append(&missing, fn);
	Str_Append(&missing, "\n");*/

	if(verbose) Con_Printf("  %s not found.\n", fn);
}

//===========================================================================
// R_ExpandSkinName
//===========================================================================
void R_ExpandSkinName(char *skin, char *expanded, const char *modelfn)
{
	directory_t mydir;

	// The "first choice" directory.
	Dir_FileDir(modelfn, &mydir);

	// Try the "first choice" directory first.
	strcpy(expanded, mydir.path);
	strcat(expanded, skin);
	if(!F_Access(expanded))
	{
		// Try the whole model path.
		if(!R_FindModelFile(skin, expanded))
		{
			expanded[0] = 0;
			return;
		}
	}
}

//===========================================================================
// R_RegisterSkin
//	Registers a new skin name.
//===========================================================================
int R_RegisterSkin(char *skin, const char *modelfn, char *fullpath)
{
	char buf[256], *formats[3] = { ".png", ".tga", ".pcx" };
	char fn[256];
	char *ext;
	int i, idx = -1;
	
	// Has a skin name been provided?
	if(!skin[0]) return -1;

	// Find the extension, or if there isn't one, add it.
	strcpy(fn, skin);
	ext = strrchr(fn, '.');
	if(!ext) 
	{
		strcat(fn, ".png");
		ext = strrchr(fn, '.');
	}
	
	// Try PNG, TGA, PCX.
	for(i = 0; i < 3 && idx < 0; i++)
	{
		strcpy(ext, formats[i]);
		R_ExpandSkinName(fn, fullpath? fullpath : buf, modelfn);
		idx = GL_GetSkinTexIndex(fullpath? fullpath : buf);
	}
	return idx;
}

//===========================================================================
// R_LoadModelMD2
//===========================================================================
void R_LoadModelMD2(DFILE *file, model_t *mdl)
{
	md2_header_t oldhd;
	dmd_header_t *hd = &mdl->header;
	dmd_info_t *inf = &mdl->info;
	model_frame_t *frame;
	byte *frames;
	int i, k, c;
	int axis[3] = { 0, 2, 1 };

	// Read the header.
	F_Read(&oldhd, sizeof(oldhd), file);
	
	// Convert it to DMD.
	hd->magic = MD2_MAGIC;
	hd->version = 8;
	hd->flags = 0;
	mdl->vertexUsage = NULL;
	inf->skinWidth = oldhd.skinWidth;
	inf->skinHeight = oldhd.skinHeight;
	inf->frameSize = oldhd.frameSize; 
	inf->numLODs = 1;
	inf->numSkins = oldhd.numSkins;
	inf->numTexCoords = oldhd.numTexCoords;
	inf->numVertices = oldhd.numVertices;
	inf->numFrames = oldhd.numFrames;
	inf->offsetSkins = oldhd.offsetSkins;
	inf->offsetTexCoords = oldhd.offsetTexCoords;
	inf->offsetFrames = oldhd.offsetFrames;
	inf->offsetLODs = oldhd.offsetEnd; // Doesn't exist.
	mdl->lodInfo[0].numTriangles = oldhd.numTriangles;
	mdl->lodInfo[0].numGlCommands = oldhd.numGlCommands;
	mdl->lodInfo[0].offsetTriangles = oldhd.offsetTriangles;
	mdl->lodInfo[0].offsetGlCommands = oldhd.offsetGlCommands;
	inf->offsetEnd = oldhd.offsetEnd;

	// Allocate and load the various arrays.
	/*mdl->texCoords = AllocAndLoad(file, inf->offsetTexCoords, 
		sizeof(md2_textureCoordinate_t) * inf->numTexCoords);*/

	/*mdl->lods[0].triangles = AllocAndLoad(file, 
		mdl->lodInfo[0].offsetTriangles,
		sizeof(md2_triangle_t) * mdl->lodInfo[0].numTriangles);*/

	// The frames need to be unpacked.
	frames = AllocAndLoad(file, inf->offsetFrames, 
		inf->frameSize * inf->numFrames);
	mdl->frames = malloc(sizeof(model_frame_t) * inf->numFrames);
	for(i = 0, frame = mdl->frames; i < inf->numFrames; i++, frame++)
	{
		md2_packedFrame_t *pfr = (md2_packedFrame_t*) 
			(frames + inf->frameSize * i);
		md2_triangleVertex_t *pVtx;

		memcpy(frame->name, pfr->name, sizeof(pfr->name));
		frame->vertices = malloc(sizeof(model_vertex_t) * inf->numVertices); 
		frame->normals = malloc(sizeof(model_vertex_t) * inf->numVertices);

		// Translate each vertex.
		for(k = 0, pVtx = pfr->vertices; k < inf->numVertices; k++, pVtx++)
		{
			memcpy(frame->normals[k].xyz, 
				avertexnormals[pVtx->lightNormalIndex],
				sizeof(float) * 3);

			for(c = 0; c < 3; c++)
			{
				frame->vertices[k].xyz[axis[c]] = pVtx->vertex[c] 
					* pfr->scale[c] + pfr->translate[c];
			}
			// Aspect undoing.
			frame->vertices[k].xyz[VY] *= rModelAspectMod;
		}
	}
	free(frames);

	mdl->lods[0].glCommands = AllocAndLoad(file,
		mdl->lodInfo[0].offsetGlCommands,
		sizeof(int) * mdl->lodInfo[0].numGlCommands);

	// Load skins.
	mdl->skins = calloc(sizeof(dmd_skin_t), inf->numSkins);
	F_Seek(file, inf->offsetSkins, SEEK_SET);
	for(i = 0; i < inf->numSkins; i++)
		F_Read(mdl->skins[i].name, 64, file);
}

//===========================================================================
// R_LoadModelDMD
//===========================================================================
void R_LoadModelDMD(DFILE *file, model_t *mo)
{
	dmd_chunk_t chunk;
	char *temp;
	dmd_info_t *inf = &mo->info;
	model_frame_t *frame;
	int i, k, c;
	dmd_triangle_t *triangles[MAX_LODS];
	int axis[3] = { 0, 2, 1 };
	
	// Read the chunks.
	F_Read(&chunk, sizeof(chunk), file);
	while(chunk.type != DMC_END)
	{
		switch(chunk.type)
		{
		case DMC_INFO: // Standard DMD information chunk.
			F_Read(inf, chunk.length, file);
			break;
			
		default:
			// Just skip all unknown chunks.
			temp = malloc(chunk.length);
			F_Read(temp, chunk.length, file);
			free(temp);
		}
		// Read the next chunk header.
		F_Read(&chunk, sizeof(chunk), file);
	}
	
	// Allocate and load in the data.
	mo->skins = calloc(sizeof(dmd_skin_t), inf->numSkins);
	F_Seek(file, inf->offsetSkins, SEEK_SET);
	for(i = 0; i < inf->numSkins; i++)
		F_Read(mo->skins[i].name, 64, file);

	/*mo->texCoords = AllocAndLoad(file, inf->offsetTexCoords, 
		sizeof(dmd_textureCoordinate_t) * inf->numTexCoords);*/
	
	temp = AllocAndLoad(file, inf->offsetFrames, 
		inf->frameSize * inf->numFrames);
	mo->frames = malloc(sizeof(model_frame_t) * inf->numFrames);
	for(i = 0, frame = mo->frames; i < inf->numFrames; i++, frame++)
	{
		dmd_packedFrame_t *pfr = (dmd_packedFrame_t*) 
			(temp + inf->frameSize * i);
		dmd_packedVertex_t *pVtx;
		
		memcpy(frame->name, pfr->name, sizeof(pfr->name));
		frame->vertices = malloc(sizeof(model_vertex_t) * inf->numVertices);
		frame->normals = malloc(sizeof(model_vertex_t) * inf->numVertices);

		// Translate each vertex.
		for(k = 0, pVtx = pfr->vertices; k < inf->numVertices; k++, pVtx++)
		{
			UnpackVector(pVtx->normal, frame->normals[k].xyz);
			for(c = 0; c < 3; c++)
			{
				frame->vertices[k].xyz[axis[c]] 
					= pVtx->vertex[c] * pfr->scale[c] + pfr->translate[c];
			}
			// Aspect undo.
			frame->vertices[k].xyz[1] *= rModelAspectMod;
		}
	}
	free(temp);

	F_Seek(file, inf->offsetLODs, SEEK_SET);
	F_Read(mo->lodInfo, sizeof(dmd_levelOfDetail_t) * inf->numLODs, file);
	
	for(i = 0; i < inf->numLODs; i++)
	{
		triangles[i] = AllocAndLoad(file, 
			mo->lodInfo[i].offsetTriangles,
			sizeof(dmd_triangle_t) * mo->lodInfo[i].numTriangles);
		mo->lods[i].glCommands = AllocAndLoad(file, 
			mo->lodInfo[i].offsetGlCommands,
			sizeof(int) * mo->lodInfo[i].numGlCommands);
	}

	// Determine vertex usage at each LOD level.
	// This code assumes there will never be more than 8 LOD levels.
	mo->vertexUsage = calloc(inf->numVertices, 1);
	for(i = 0; i < inf->numLODs; i++)
		for(k = 0; k < mo->lodInfo[i].numTriangles; k++)
			for(c = 0; c < 3; c++)
				mo->vertexUsage[triangles[i][k].vertexIndices[c]]
					|= 1 << i;

	// We don't need the triangles any more.
	for(i = 0; i < inf->numLODs; i++)
		free(triangles[i]);
}

//===========================================================================
// R_RegisterModelSkin
//===========================================================================
void R_RegisterModelSkin(model_t *mdl, int index)
{
	filename_t buf;

	memset(buf, 0, sizeof(buf));
	memcpy(buf, mdl->skins[index].name, 64);
	
	mdl->skins[index].id = R_RegisterSkin(buf, mdl->fileName, 
		mdl->skins[index].name);

	if(mdl->skins[index].id < 0)
	{
		// Not found!
		VERBOSE( Con_Printf("  %s (#%i) not found.\n", buf, index) );
	}
}

//===========================================================================
// R_LoadModel
//	Finds the existing model or loads in a new one.
//===========================================================================
int R_LoadModel(char *origfn)
{
	int		i, index;
	model_t	*mdl;
	DFILE	*file = NULL;
	char	filename[256];
		
	if(!origfn[0]) return 0;	// No model specified.

	BEGIN_PROF( PROF_LM_FIND_MODEL );

	if(!R_FindModelFile(origfn, filename))
	{
		END_PROF( PROF_LM_FIND_MODEL );

		R_MissingModel(origfn);
		return 0;
	}

	END_PROF( PROF_LM_FIND_MODEL );

	// Has this been already loaded?
	if((index = R_FindModelFor(filename)) < 0)
	{
		// Not loaded yet, try to open the file. 
		if((file = F_Open(filename, "rb")) == NULL)
		{
			R_MissingModel(filename);
			return 0;
		}
		// Allocate a new model_t.
		if((index = R_NewModelFor(filename)) < 0) 
		{
			F_Close(file);
			return 0;	// Bugger.
		}
	}
	mdl = modellist[index];
	if(mdl->loaded) 
	{
		if(file) F_Close(file);
		return index;	// Already loaded.
	}

	BEGIN_PROF( PROF_LM_LOADERS );
	
	// Now we can load in the data.
	F_Read(&mdl->header, sizeof(mdl->header), file);
	if(mdl->header.magic == MD2_MAGIC)		// Load as MD2?
	{
		F_Rewind(file);
		R_LoadModelMD2(file, mdl);
	}
	else if(mdl->header.magic == DMD_MAGIC) // Load as DMD?
	{
		R_LoadModelDMD(file, mdl);
	}
	else
	{
		// Bad magic! Cancel the loading.
		free(mdl);
		modellist[index] = 0;
		F_Close(file);
		return 0;
	}

	END_PROF( PROF_LM_LOADERS );

	// We're done.
	mdl->loaded = true;
	mdl->allowTexComp = true;
	F_Close(file);
	strcpy(mdl->fileName, filename);

	// Determine the actual (full) paths.
	BEGIN_PROF( PROF_LM_SKINS );
	for(i = 0; i < mdl->info.numSkins; i++)
	{
		R_RegisterModelSkin(mdl, i);
	}
	END_PROF( PROF_LM_SKINS );

	return index;
}

//===========================================================================
// R_ModelFrameNumForName
//===========================================================================
int R_ModelFrameNumForName(int modelnum, char *fname)
{
	int			i;
	model_t		*mdl;

	if(!modelnum) return 0;
	mdl = modellist[modelnum];
	for(i = 0; i < mdl->info.numFrames; i++)
	{
		if(!stricmp(mdl->frames[i].name, fname)) return i;
	}
	return 0;
}

//===========================================================================
// GetStateModel
//	Returns the appropriate modeldef for the given state.
//===========================================================================
static modeldef_t *GetStateModel(state_t *st, int select)
{
	modeldef_t *modef, *iter;
	int mosel;

	if(!st || !st->model) return 0;
	modef = (modeldef_t*) st->model;
	mosel = select & DDMOBJ_SELECTOR_MASK;

	if(select)
	{
		// Choose the correct selector, or selector zero if the given 
		// one not available.
		for(iter = modef; iter; iter = iter->selectnext)
			if(iter->select == mosel)
			{
				modef = iter;
				break;
			}
	}
	return modef;
}

//===========================================================================
// R_CheckIDModelFor
//===========================================================================
modeldef_t *R_CheckIDModelFor(const char *id)
{
	int i;

	if(!id[0]) return NULL;

	for(i = 0; i < nummodels; i++)
		if(!strcmp(models[i].id, id))
			return models + i;
	return NULL;
}

//===========================================================================
// R_CheckModelFor
//	Is there a model for this mobj? The decision is made based on the 
//	state and tics of the mobj. Returns the modeldefs that are in effect 
//	at the moment (interlinks checked appropriately).
//===========================================================================
float R_CheckModelFor(mobj_t *mo, modeldef_t **modef, modeldef_t **nextmodef)
{
	float interp = -1;
	state_t *st = mo->state;
	modeldef_t *mdit;
	boolean worldTime = false;

	// By default there are no models.
	*nextmodef = NULL;
	*modef = GetStateModel(st, mo->selector);
	if(!*modef) return -1; // No model available.

	// World time animation?
	if((*modef)->flags & MFF_WORLD_TIME_ANIM)
	{
		float duration = (*modef)->interrange[0];
		float offset = (*modef)->interrange[1];
		// Validate/modify the values.
		if(duration == 0) duration = 1;
		if(offset == -1) offset = M_CycleIntoRange(THING_TO_ID(mo), duration);
		interp = M_CycleIntoRange(levelTime/duration + offset, 1);
		worldTime = true;
	}
	else
	{
		// Calculate the currently applicable intermark.
		interp = 1.0f - (mo->tics - frameTimePos) / (float) st->tics;
	}

/*#if _DEBUG
	if(mo->dplayer) Con_Printf("itp:%f mot:%i stt:%i\n", interp,
		mo->tics, st->tics);
#endif*/

	// First find the modef for the interpoint. Intermark is 'stronger'
	// than interrange.

	// Scan interlinks.
	while((*modef)->internext && (*modef)->internext->intermark <= interp)
		*modef = (*modef)->internext;

	if(!worldTime)
	{
		// Scale to the modeldef's interpolation range.
		interp = (*modef)->interrange[0] + interp *
			((*modef)->interrange[1] - (*modef)->interrange[0]);
	}

	// What would be the next model? Check interlinks first.
	if((*modef)->internext)
	{
		*nextmodef = (*modef)->internext;
	}
	else if(worldTime)
	{
		*nextmodef = GetStateModel(st, mo->selector);
	}
	else if(st->nextstate > 0) // Check next state.
	{
		int max = 20; // Let's not be here forever...
		// Find the appropriate state based on interrange.		
		state_t *it = states + st->nextstate;
		if((*modef)->interrange[1] < 1)
		{
			// Current modef doesn't interpolate to the end, find the
			// proper destination modef (it isn't just the next one).
			// Scan the states that follow (and interlinks of each).
			while(max-- 
				&& (!it->model 
					|| GetStateModel(it, mo->selector)->interrange[0] > 0)
				&& it->nextstate > 0) 
			{
				// Scan interlinks, then go to the next state.
				if((mdit = GetStateModel(it, mo->selector))
					&& mdit->internext)
				{
					for(;;)
					{
						mdit = mdit->internext;
						if(!mdit) break;
						if(mdit->interrange[0] <= 0) // A new beginning?
						{
							*nextmodef = mdit;
							goto found_next;
						}
					}
				}
				it = states + it->nextstate;
			}
			// FIXME: What about max == -1? What should 'it' be then?
		}
		*nextmodef = GetStateModel(it, mo->selector);
found_next:;
	}

	// Is this group disabled?
	if(useModels >= 2 && (*modef)->group & useModels) 
	{
		*modef = *nextmodef = NULL;
		return -1;
	}
	return interp;
}

//===========================================================================
// R_GetModelFrame
//===========================================================================
model_frame_t *R_GetModelFrame(int model, int frame)
{
	return modellist[model]->frames + frame;
}

//===========================================================================
// R_GetModelBounds
//===========================================================================
void R_GetModelBounds(int model, int frame, float min[3], float max[3])
{
	model_frame_t *mframe = R_GetModelFrame(model, frame);
	model_t *mo = modellist[model];
	model_vertex_t *v;
	int i, k;

	if(!mframe) Con_Error("R_GetModelBounds: bad model/frame.\n");
	for(i = 0, v = mframe->vertices; i < mo->info.numVertices; i++, v++)
		for(k = 0; k < 3; k++)
		{
			if(!i || v->xyz[k] < min[k]) min[k] = v->xyz[k];
			if(!i || v->xyz[k] > max[k]) max[k] = v->xyz[k];
		}
}

//===========================================================================
// R_GetModelHRange
//	Height range, really ("horizontal range" comes to mind...).
//===========================================================================
float R_GetModelHRange(int model, int frame, float *top, float *bottom)
{
	float min[3], max[3];

	R_GetModelBounds(model, frame, min, max);
	*top = max[VY];
	*bottom = min[VY];
	return max[VY] - min[VY]; 
}

//===========================================================================
// R_ScaleModel
//	Scales the given model so that it'll be 'destHeight' units tall.
//	The measurements are based on submodel zero. The scaling is done
//	uniformly! 
//===========================================================================
void R_ScaleModel(modeldef_t *mf, float destHeight, float offset)
{
	submodeldef_t	*smf = &mf->sub[0];
/*	model_frame_t	*mFrame = R_GetModelFrame(smf->model, smf->frame); */
	int				i; /*, num = modellist[smf->model]->info.numVertices; */
	float			top, bottom, height;
	float			scale;

	if(!smf->model) return;	// No model to scale!

	// Find the top and bottom heights.
	height = R_GetModelHRange(smf->model, smf->frame, &top, &bottom);
	if(!height) height = 1;
	
	scale = destHeight / height;

	for(i = 0; i < 3; i++) mf->scale[i] = scale;
	mf->offset[VY] = -bottom*scale + offset;
}

//===========================================================================
// R_ScaleModelToSprite
//===========================================================================
void R_ScaleModelToSprite(modeldef_t *mf, int sprite, int frame)
{
	spritedef_t	*spr = sprites + sprite;
	int lump, off;

	if(!spr->numframes || spr->spriteframes == NULL) return;

	lump = spr->spriteframes[frame].lump[0];
	off = spritelumps[lump].topoffset - spritelumps[lump].height;
	if(off < 0) off = 0;
	R_ScaleModel(mf, spritelumps[lump].height, off);
}

//===========================================================================
// R_GetModelVisualRadius
//===========================================================================
float R_GetModelVisualRadius(modeldef_t *mf)
{
	float min[3], max[3];

	if(!mf->sub[0].model) return 0;
	// Use the first submodel's bounds.
	R_GetModelBounds(mf->sub[0].model, mf->sub[0].frame, min, max);
	// Half of the average of width and depth.
	return (mf->scale[VX] * (max[VX] - min[VX]) 
		+ mf->scale[VZ] * (max[VZ] - min[VZ]))/3.5f;
}

//===========================================================================
// R_NewModelSkin
//	Allocate room for a new skin file name. This allows using more than
//	the maximum number of skins.
//===========================================================================
short R_NewModelSkin(model_t *mdl, const char *fileName)
{
	int added = mdl->info.numSkins, i;

	mdl->skins = realloc(mdl->skins, sizeof(dmd_skin_t) 
		* ++mdl->info.numSkins);
	memset(mdl->skins + added, 0, sizeof(dmd_skin_t));
	strncpy(mdl->skins[added].name, fileName, 64);
	R_RegisterModelSkin(mdl, added);

	// Did we get a dupe?
	for(i = 0; i < mdl->info.numSkins - 1; i++)
	{
		if(mdl->skins[i].id == mdl->skins[added].id)
		{
			// This is the same skin file.
			// We did a lot of unnecessary work...
			mdl->info.numSkins--;
			mdl->skins = realloc(mdl->skins, sizeof(dmd_skin_t) 
				* mdl->info.numSkins);
			return i;
		}
	}

	return added;
}

//===========================================================================
// R_GetIDModelDef
//	Create a new modeldef or find an existing one. This is for ID'd models.
//===========================================================================
modeldef_t *R_GetIDModelDef(const char *id)
{
	modeldef_t *md;

	// ID defined?
	if(!id[0]) return NULL;

	// First try to find an existing modef.
	if((md = R_CheckIDModelFor(id)) != NULL)
		return md;

	// Get a new entry.
	md = models + nummodels++;
	memset(md, 0, sizeof(*md));
	strcpy(md->id, id);
	return md;
}

//===========================================================================
// R_GetModelDef
//	Create a new modeldef or find an existing one. There can be only one
//	model definition associated with a state/intermark pair. 
//===========================================================================
modeldef_t *R_GetModelDef(int state, float intermark, int select)
{
	int			i;
	modeldef_t	*md;

	if(state < 0 || state >= count_states.num) return NULL; // Not a valid state.

	// First try to find an existing modef.
	for(i = 0; i < nummodels; i++)
		if(models[i].state == &states[state] 
			&& models[i].intermark == intermark
			&& models[i].select == select) 
		{
			// Models are loaded in reverse order; this one already has 
			// a model.
			return NULL;
		}
	
	// This is impossible, but checking won't hurt...
	if(nummodels >= maxmodels) return NULL;

	md = models + nummodels++;
	memset(md, 0, sizeof(*md));
	// Set initial data.
	md->state = &states[state];
	md->intermark = intermark;
	md->select = select;
	return md;
}

//===========================================================================
// R_SetupModel
//	Creates a modeldef based on the given DED info.
//	A pretty straightforward operation. No interlinks are set yet.
//	Autoscaling is done and the scale factors set appropriately.
//	After this has been called for all available Model DEDs, each
//	State that has a model will have a pointer to the one with the
//	smallest intermark (start of a chain). 
//===========================================================================
void R_SetupModel(ded_model_t *def)
{
	modeldef_t *modef;
	int model_scope_flags = def->flags | defs.model_flags;
	ded_submodel_t *subdef;
	submodeldef_t *sub;
	int i, k, statenum = Def_GetStateNum(def->state);
	float min[3], max[3];

	// Is this an ID'd model?
	if((modef = R_GetIDModelDef(def->id)) == NULL)
	{
		// No, normal State-model.
		if(statenum < 0)
		{
			Con_Message("R_SetupModel: Undefined state '%s'.\n", def->state);
			return; 
		}
		
		BEGIN_PROF( PROF_GET_MODEL_DEF );
		modef = R_GetModelDef(statenum + def->off, def->intermark, 
			def->selector);
		END_PROF( PROF_GET_MODEL_DEF );

		if(!modef) return; // Can't get a modef, quit!
	}

	BEGIN_PROF( PROF_DATA_INIT );

	// Init modef info (state & intermark already set).
	modef->def = def;
	modef->group = def->group;
	modef->flags = model_scope_flags;
	for(i = 0; i < 3; i++)
	{
		modef->offset[i] = def->offset[i];
		modef->scale[i] = def->scale[i];
	}
	modef->offset[VY] += defs.model_offset;	// Common Y axis offset.
	modef->scale[VY] *= defs.model_scale;	// Common Y axis scaling.
	modef->resize = def->resize;
	modef->skintics = def->skintics;
	for(i = 0; i < 2; i++)
	{
		modef->interrange[i] = def->interrange[i];
	}
	if(modef->skintics < 1) modef->skintics = 1;
	
	END_PROF( PROF_DATA_INIT );

	// Submodels.
	for(i = 0, subdef = def->sub, sub = modef->sub; 
		i < MAX_FRAME_MODELS; 
		i++, subdef++, sub++)
	{
		BEGIN_PROF( PROF_LOAD_MODEL );
		sub->model = R_LoadModel(subdef->filename.path);
		END_PROF( PROF_LOAD_MODEL);

		if(!sub->model) continue;

		BEGIN_PROF( PROF_REGISTER_SKIN );

		sub->frame = R_ModelFrameNumForName(sub->model, subdef->frame);
		sub->framerange = subdef->framerange;
		// Frame range must always be greater than zero.
		if(sub->framerange < 1) sub->framerange = 1;
		// Submodel-specific flags cancel out model-scope flags!
		sub->flags = model_scope_flags ^ subdef->flags;
		if(subdef->skinfilename.path[0])
		{
			// A specific file name has been given for the skin.
			sub->skin = R_NewModelSkin(modellist[sub->model], 
				subdef->skinfilename.path);
		}
		else
		{
			sub->skin = subdef->skin;
		}
		sub->skinrange = subdef->skinrange;
		// Skin range must always be greater than zero.
		if(sub->skinrange < 1) sub->skinrange = 1;
		// Offset within the model.
		for(k = 0; k < 3; k++) sub->offset[k] = subdef->offset[k];
		sub->alpha = (byte) (subdef->alpha * 255);
		sub->shinyskin = R_RegisterSkin(subdef->shinyskin, 
			subdef->filename.path, NULL);

		END_PROF( PROF_REGISTER_SKIN );

		// Should we allow texture compression with this model?
		if(sub->flags & MFF_NO_TEXCOMP)
		{
			// All skins of this model will no longer use compression.
			modellist[sub->model]->allowTexComp = false;
		}
	}

	BEGIN_PROF( PROF_SCALING );

	// Do scaling, if necessary.
	if(modef->resize)
	{
		R_ScaleModel(modef, modef->resize, modef->offset[VY]);
	}
	else if(modef->state && modef->sub[0].flags & MFF_AUTOSCALE)
	{
		int sprNum = Def_GetSpriteNum(def->sprite.id);
		int sprFrame = def->spriteframe;
		if(sprNum < 0) // No sprite ID given?
		{
			sprNum = modef->state->sprite;
			sprFrame = modef->state->frame;
		}		
		R_ScaleModelToSprite(modef, sprNum, sprFrame);
	}

	if(modef->state)
	{
		// Associate this modeldef with its state.
		if(!modef->state->model)
		{
			// No modef; use this.
			modef->state->model = modef;
		}
		else // Must check intermark; smallest wins!
		{
			modeldef_t *other = (modeldef_t*) modef->state->model;
			if((modef->intermark <= other->intermark // Should never be ==
				&& modef->select == other->select)
				|| modef->select < other->select) // Smallest selector?
				modef->state->model = modef;
		}
	}

	// Calculate the particle offset for each submodel.
	for(i = 0, sub = modef->sub; i < MAX_FRAME_MODELS; i++, sub++)
	{
		if(!sub->model) 
		{
			memset(modef->ptcoffset[i], 0, sizeof(modef->ptcoffset[i]));
			continue;
		}
	
		R_GetModelBounds(sub->model, sub->frame, min, max);

		// Apply the various scalings and offsets.
		for(k = 0; k < 3; k++)
		{
			modef->ptcoffset[i][k] = ((max[k] + min[k])/2 + sub->offset[k]) 
				* modef->scale[k] + modef->offset[k];
		}
	}

	// Calculate visual radius for shadows.
	if(def->shadowradius)
	{
		modef->visualradius = def->shadowradius;
	}
	else
	{
		modef->visualradius = R_GetModelVisualRadius(modef);
	}

	END_PROF( PROF_SCALING );
}

//==========================================================================
// R_InitModels
//	States must be initialized before this. 
//==========================================================================
void R_InitModels(void)
{
	int	i, k, minsel;
	float minmark;
	modeldef_t *me, *other, *closest;
	uint usedTime;

	// Dedicated servers do nothing with models.
	if(isDedicated || ArgCheck("-nomd2")) return;

	Con_Message("R_InitModels: Initializing MD2 models.\n");
	VERBOSE2( Con_Message("  Search path: %s\n", modelPath.str) );

	// Build the file hash for searching model files.
	usedTime = Sys_GetRealTime();
	FH_Init(modelPath.str);
	VERBOSE( Con_Message("  File hash built in %.2f seconds.\n",
		(Sys_GetRealTime() - usedTime) / 1000.0f) );
	
	Con_InitProgress("R_Init: Initializing models...", defs.count.models.num);

	usedTime = Sys_GetRealTime();

	free(models);
	// There can't be more modeldefs than there are DED Models.
	// There can be fewer, though.
	maxmodels = defs.count.models.num;
	models = malloc(sizeof(modeldef_t) * maxmodels);
	nummodels = 0;

	// Clear the modef pointers of all States.
	for(i = 0; i < count_states.num; i++) states[i].model = NULL;

	// Read in the model files and their data, 'n stuff.
	// Use the latest definition available for each sprite ID.
	for(i = defs.count.models.num - 1; i >= 0; --i) 
	{
		Con_Progress(1, PBARF_DONTSHOW);
		R_SetupModel(defs.models + i);
	}
	Con_HideProgress();

	// Create interlinks. Note that the order in which the defs were loaded
	// is important. We want to allow "patch" definitions, right?
	
	// For each modeldef we will find the "next" def.
	for(i = nummodels - 1, me = models + i; i >= 0; --i, --me)
	{
		minmark = 2; // max = 1, so this is "out of bounds".
		closest = NULL;
		for(k = nummodels - 1, other = models + k; k >= 0; --k, --other)
		{
			// Same state and a bigger order are the requirements.
			if(other->state == me->state 
				&& other->def > me->def // Defined after me.
				&& other->intermark > me->intermark
				&& other->intermark < minmark)
			{
				minmark = other->intermark;
				closest = other;
			}
		}
		me->internext = closest;
	}

	// Create selectlinks.
	for(i = nummodels - 1, me = models + i; i >= 0; --i, --me)
	{
		minsel = DDMAXINT;
		closest = NULL;
		// Start scanning from the next definition.
		for(k = nummodels - 1, other = models + k; k >= 0; --k, --other)
		{
			// Same state and a bigger order are the requirements.
			if(other->state == me->state 
				&& other->def > me->def // Defined after me.
				&& other->select > me->select
				&& other->select < minsel
				&& other->intermark >= me->intermark)
			{
				minsel = other->select;
				closest = other;
			}
		}
		me->selectnext = closest;

/*		if(closest)
			Con_Message("%s -> %s\n",
				defs.states[me->state - states].id,
				defs.states[closest->state - states].id);*/
	}

	Con_Message("R_InitModels: Done in %.2f seconds.\n",
		(Sys_GetRealTime() - usedTime) / 1000.0f);

	PRINT_PROF( PROF_GET_MODEL_DEF );
	PRINT_PROF( PROF_DATA_INIT );
	PRINT_PROF( PROF_LOAD_MODEL );
	PRINT_PROF( PROF_LM_FIND_MODEL );
	PRINT_PROF( PROF_LM_LOADERS );
	PRINT_PROF( PROF_LM_SKINS );
	PRINT_PROF( PROF_REGISTER_SKIN );
	PRINT_PROF( PROF_SCALING );
}

//===========================================================================
// R_ShutdownModels
//	Frees all memory allocated for models.
//	FIXME: Why only centralized memory deallocation? Bad design...
//===========================================================================
void R_ShutdownModels(void)
{
	int i, k;
	model_t *m;

	free(models);
	models = NULL;
	for(i = 1; i < MAX_MODELS; i++)
	{
		if(!(m = modellist[i])) continue;
		free(m->skins);
		//free(modellist[i]->texCoords);
		for(k = 0; k < m->info.numFrames; k++)
		{
			free(m->frames[k].vertices);
			free(m->frames[k].normals);
		}
		free(m->frames);
		for(k = 0; k < m->info.numLODs; k++)
		{
			//free(modellist[i]->lods[k].triangles);
			free(m->lods[k].glCommands);
		}
		free(m->vertexUsage);
		free(m);
		modellist[i] = NULL;
	}
}

//===========================================================================
// R_LoadSkin
//	Loads a model skin. The caller must free the returned buffer with Z_Free! 
//===========================================================================
byte *R_LoadSkin(model_t *mdl, int skin, int *width, int *height, int *pxsize)
{
	image_t image;

	GL_LoadImage(&image, mdl->skins[skin].name, false);

	*width = image.width;
	*height = image.height;
	*pxsize = image.pixelSize;
	return image.pixels;
}

//===========================================================================
// R_SetModelFrame
//===========================================================================
void R_SetModelFrame(modeldef_t *modef, int frame)
{
	int k;
	model_t *mdl;

	for(k = 0; k < DED_MAX_SUB_MODELS; k++)
	{
		if(!modef->sub[k].model) continue;
		mdl = modellist[modef->sub[k].model];
		// Modify the modeldef itself: set the current frame.
		modef->sub[k].frame = frame % mdl->info.numFrames;
	}
}

//===========================================================================
// R_PrecacheModelSkins
//===========================================================================
void R_PrecacheModelSkins(modeldef_t *modef)
{
	int k, sub;
	model_t *mdl;

	// Precache this.
	for(sub = 0; sub < MAX_FRAME_MODELS; sub++)
	{
		if(!modef->sub[sub].model) continue;
		mdl = modellist[modef->sub[sub].model];
		// Load all skins.
		for(k = 0; k < mdl->info.numSkins; k++) 
			GL_BindTexture( GL_PrepareSkin(mdl, k) );
		GL_BindTexture( GL_PrepareShinySkin(modef, sub) );
	}
}

//===========================================================================
// R_PrecacheSkinsForState
//===========================================================================
void R_PrecacheSkinsForState(int stateIndex)
{
	state_t *st = states + stateIndex;

	if(stateIndex <= 0 || stateIndex >= defs.count.states.num
		|| !st->model) return;
	
	R_PrecacheModelSkins(st->model);
}

//===========================================================================
// R_PrecacheSkinsForMobj
//	The skins are also bound here once so they should be ready for use the 
//	next time they're needed.
//===========================================================================
void R_PrecacheSkinsForMobj(mobj_t *mo)
{
	int i;
	modeldef_t *modef;

	// Check through all the model definitions.
	for(i = 0, modef = models; i < nummodels; i++, modef++)
	{
		if(!modef->state) continue;
		if(mo->type < 0 || mo->type >= defs.count.mobjs.num) continue; // Hmm?
		if(stateowners[modef->state - states] != mobjinfo + mo->type)
			continue;

		R_PrecacheModelSkins(modef);
	}
}
