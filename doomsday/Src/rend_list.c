/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�en <jaakko.keranen@iki.fi>
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
 * rend_list.c: Doomsday Rendering Lists v3.1
 *
 * 3.1 -- Support for multiple shadow textures
 * 3.0 -- Multitexturing
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"

#include "def_main.h"
#include "m_profiler.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
	PROF_RL_ADD_POLY,
	PROF_RL_GET_LIST,
	PROF_RL_RENDER_ALL,
	PROF_RL_RENDER_NORMAL,
	PROF_RL_SETUP_STATE,
	PROF_RL_DRAW_PRIMS,
	PROF_RL_DRAW_ELEMS,
	PROF_RL_RENDER_LIGHT,
	PROF_RL_RENDER_MASKED
END_PROF_TIMERS()

#define MAX_TEX_UNITS		8		// Only two used, really.
#define RL_HASH_SIZE		128

// Number of extra bytes to keep allocated in the end of each rendering list.
#define LIST_DATA_PADDING	16

// FIXME: Rlist allocation could be dynamic.
#define MAX_RLISTS			1024 

#define MTEX_DETAILS_ENABLED (r_detail && useMultiTexDetails && defs.count.details.num > 0)
#define IS_MTEX_DETAILS		(MTEX_DETAILS_ENABLED && numTexUnits > 1)
#define IS_MTEX_LIGHTS		(!IS_MTEX_DETAILS && !useFog && useMultiTexLights \
							&& numTexUnits > 1 && envModAdd)

// Drawing condition flags.
#define DCF_NO_BLEND				0x00000001
#define DCF_BLEND					0x00000002
#define DCF_SET_LIGHT_ENV0			0x00000004
#define DCF_SET_LIGHT_ENV1			0x00000008
#define DCF_SET_LIGHT_ENV			(DCF_SET_LIGHT_ENV0 | DCF_SET_LIGHT_ENV1)
#define DCF_JUST_ONE_LIGHT			0x00000010
#define DCF_MANY_LIGHTS				0x00000020
#define DCF_SKIP					0x80000000

// List Modes.
typedef enum listmode_e
{
	LM_SKYMASK,
	LM_ALL,
	LM_LIGHT_MOD_TEXTURE,
	LM_FIRST_LIGHT,
	LM_TEXTURE_PLUS_LIGHT,
	LM_UNBLENDED_TEXTURE_AND_DETAIL,
	LM_BLENDED,
	LM_BLENDED_FIRST_LIGHT,
	LM_NO_LIGHTS,
	LM_WITHOUT_TEXTURE,
	LM_LIGHTS,
	LM_MOD_TEXTURE,
	LM_MOD_TEXTURE_MANY_LIGHTS,
	LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL,
	LM_BLENDED_MOD_TEXTURE,
	LM_ALL_DETAILS,
	LM_BLENDED_DETAILS,
	LM_SHADOW
}
listmode_t;

// Types of rendering primitives.
typedef enum primtype_e
{
	PT_TRIANGLES,			// Used for most stuff.
	PT_FAN,
	PT_DOUBLE_FAN,
} 
primtype_t;

// Texture coordinate array indices.
enum
{
	TCA_MAIN,				// Main texture.
	TCA_BLEND,				// Blendtarget texture.
	TCA_DETAIL,				// Detail texture coordinates.
	TCA_BLEND_DETAIL,		// Blendtarget's detail texture coordinates.
	TCA_LIGHT,				// Glow texture coordinates.
	NUM_TEXCOORD_ARRAYS
};

// TYPES -------------------------------------------------------------------

/*
 * Each primhdr begins a block of polygon data that ends up as one or
 * more triangles on the screen. Note that there are pointers to the
 * rendering list itself here; they will need to be properly restored
 * whenever the list is resized.
 */
typedef struct primhdr_s
{
	// RL_AddPoly expects that size is the first thing in the header.
	// Must be an offset since the list is sometimes reallocated.
	int size;				// Size of this primitive (zero = n/a).
	
	// Generic data, common to all polys.
	primtype_t type;	
	short flags;			// RPF_*

	// Number of vertices in the primitive.
	uint primSize;

	// Elements in the vertex array for this primitive. 
	// The indices are always contiguous: indices[0] is the base, and 
	// indices[1...n] > indices[0]. 
	// All indices in the range indices[0]...indices[n] are used by this
	// primitive (some are shared).
	ushort numIndices;	
	uint *indices;

	// First index of the other fan in a Double Fan.
	ushort beginOther;

	// The first dynamic light affecting the surface.
	dynlight_t *light;
}
primhdr_t;

// Rendering List 'has' flags.
#define RLF_LIGHTS		0x1		// Primitives are dynamic lights.
#define RLF_BLENDED		0x2		// List contains only texblended prims.

/*
 * The rendering list. When the list is resized, pointers in the primitives
 * need to be restored so that they point to the new list.
 */
typedef struct rendlist_s
{
	struct rendlist_s *next;
	int		flags;
	gltexture_t tex, intertex;
	float	interpos;			// 0 = primary, 1 = secondary texture
	int		size;				// Number of bytes allocated for the data.
	byte	*data;				// Data for a number of polygons (The List).
	byte	*cursor;			// A pointer to data, for reading/writing.
	primhdr_t *last;			// Pointer to the last primitive (or NULL).
}
rendlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

vissprite_t *R_NewVisSprite(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void RL_SetupState(listmode_t mode, rendlist_t *list);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int			skyhemispheres;
extern int			useDynLights, dlBlend, simpleSky;
extern boolean		useFog;
extern float		maxLightDist;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int					renderTextures = true;
int					renderWireframe = false;
int					useMultiTexLights = true;
int					useMultiTexDetails = true;

// Intensity of angle-based wall lighting.
float				rend_light_wall_angle = 1;

// Rendering parameters for detail textures.
float				detailFactor = .5f;
//float				detailMaxDist = 256;
float				detailScale = 4;

typedef struct listhash_s {
	rendlist_t *first, *last;
} listhash_t;

// Maximum number of dynamic lights stored in the global vertex array.
// Primitives with this many lights can be rendered with a single pass.
// The rest are allocated from the rendering list.
int maxArrayLights = 1;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/*
 * The vertex arrays.
 */
static gl_vertex_t *vertices;
static gl_texcoord_t *texCoords[NUM_TEXCOORD_ARRAYS];
static gl_color_t *colors;

static uint numVertices, maxVertices;

/*
 * The rendering lists.
 */
// Surfaces without lights.
static listhash_t plainHash[RL_HASH_SIZE];

// Surfaces with lights.
static listhash_t litHash[RL_HASH_SIZE];

// Additional light primitives.
static listhash_t dynHash[RL_HASH_SIZE];

static listhash_t shadowHash[RL_HASH_SIZE];
static rendlist_t skyMaskList;

// CODE --------------------------------------------------------------------

//===========================================================================
// RL_AddMaskedPoly
//	This doesn't create a rendering primitive but a vissprite! The vissprite
//	represents the masked poly and will be rendered during the rendering 
//	of sprites. This is necessary because all masked polygons must be 
//	rendered back-to-front, or there might be alpha artifacts along edges.
//===========================================================================
void RL_AddMaskedPoly(rendpoly_t *poly)
{
	vissprite_t *vis = R_NewVisSprite();
	byte brightest[3];
	dynlight_t *dyn;
	int i;
	
	vis->type = VSPR_MASKED_WALL;
	vis->distance = (poly->vertices[0].dist + poly->vertices[1].dist) / 2;
	vis->data.wall.texture = poly->tex.id;
	vis->data.wall.masked = texmask;	// Store texmask status in flip.
	vis->data.wall.top = poly->top;
	vis->data.wall.bottom = poly->bottom;
	for(i = 0; i < 2; i++)
	{
		vis->data.wall.vertices[i].pos[VX] = poly->vertices[i].pos[VX];
		vis->data.wall.vertices[i].pos[VY] = poly->vertices[i].pos[VY];
		memcpy(&vis->data.wall.vertices[i].color,
			   poly->vertices[i].color.rgba, 3);
		vis->data.wall.vertices[i].color |= 0xff000000; // Alpha.
	}
	vis->data.wall.texc[0][VX] = poly->texoffx / (float) poly->tex.width;
	vis->data.wall.texc[1][VX] = vis->data.wall.texc[0][VX]
		+ poly->length/poly->tex.width;
	vis->data.wall.texc[0][VY] = poly->texoffy / (float) poly->tex.height;
	vis->data.wall.texc[1][VY] = vis->data.wall.texc[0][VY] 
		+ (poly->top - poly->bottom)/poly->tex.height;

	// We can render ONE dynamic light, maybe.
	if(poly->lights && numTexUnits > 1 && envModAdd)
	{
		// Choose the brightest light.
		memcpy(brightest, poly->lights->color, 3);
		vis->data.wall.light = poly->lights;
		for(dyn = poly->lights->next; dyn; dyn = dyn->next)
		{
			if((brightest[0] + brightest[1] + brightest[2])/3
				< (dyn->color[0] + dyn->color[1] + dyn->color[2])/3)
			{
				memcpy(brightest, dyn->color, 3);
				vis->data.wall.light = dyn;
			}
		}
	}
	else
	{
		vis->data.wall.light = NULL;
	}
}

//===========================================================================
// RL_VertexColors
//	Color distance attenuation, extralight, fixedcolormap.
//	"Torchlight" is white, regardless of the original RGB.
//===========================================================================
void RL_VertexColors(rendpoly_t *poly, int lightlevel, const byte *rgb)
{
	int i;
	float light, real, minimum;
	rendpoly_vertex_t *vtx;
	boolean usewhite;

	if(poly->numvertices == 2) // A quad?
	{
		// Do a lighting adjustment based on orientation.
		lightlevel += (poly->vertices[1].pos[VY] - poly->vertices[0].pos[VY]) 
			/ poly->length * 18 * rend_light_wall_angle;
		if(lightlevel < 0) lightlevel = 0;
		if(lightlevel > 255) lightlevel = 255;
	}

	light = lightlevel / 255.0f;

	for(i = 0, vtx = poly->vertices; i < poly->numvertices; i++, vtx++)
	{
		usewhite = false;

		real = light - (vtx->dist - 32)/maxLightDist * (1-light);
		minimum = light*light + (light - .63f) / 2;
		if(real < minimum) real = minimum; // Clamp it.

		// Add extra light.
		real += extralight/16.0f;

		// Check for torch.
		if(viewplayer->fixedcolormap)
		{
			// Colormap 1 is the brightest. I'm guessing 16 would be the darkest.
			int ll = 16 - viewplayer->fixedcolormap;
			float d = (1024 - vtx->dist) / 512.0f;
			float newmin = d*ll / 15.0f;
			if(real < newmin) 
			{
				real = newmin;
				usewhite = true; // FIXME : Do some linear blending.
			}
		}

		// Clamp the final light.
		if(real < 0) real = 0;
		if(real > 1) real = 1;

		if(usewhite)
		{
			vtx->color.rgba[0] 
				= vtx->color.rgba[1]
				= vtx->color.rgba[2]
				= (DGLubyte) (0xff * real);
		}
		else
		{
			vtx->color.rgba[0] = (DGLubyte) (rgb[0] * real);
			vtx->color.rgba[1] = (DGLubyte) (rgb[1] * real);
			vtx->color.rgba[2] = (DGLubyte) (rgb[2] * real);
		}
	}
}

//===========================================================================
// RL_PrepareFlat
//===========================================================================
void RL_PrepareFlat
	(planeinfo_t *plane, rendpoly_t *poly, subsector_t *subsector)
{
	int	i;

	poly->numvertices = plane->numvertices;

	// Calculate the distance to each vertex.
	for(i = 0; i < plane->numvertices; i++)
	{
		poly->vertices[i].pos[VX] = plane->vertices[i].x;
		poly->vertices[i].pos[VY] = plane->vertices[i].y;
		poly->vertices[i].dist = Rend_PointDist2D(poly->vertices[i].pos);
	}

	// Calculate the color for each vertex.
	RL_VertexColors(poly, Rend_SectorLight(poly->sector),
					R_GetSectorLightColor(poly->sector));
}

//===========================================================================
// RL_SelectTexUnits
//	The first selected unit is active after this call.
//===========================================================================
void RL_SelectTexUnits(int count)
{
	int i;

	// Disable extra units.
	for(i = numTexUnits - 1; i >= count; i--) 
		gl.Disable(DGL_TEXTURE0 + i);

	// Enable the selected units.
	for(i = count - 1; i >= 0; i--)
	{
		if(i >= numTexUnits) continue;

		gl.Enable(DGL_TEXTURE0 + i);
		// Enable the texcoord array for this unit.
		gl.EnableArrays(0, 0, 0x1 << i);
	}
}

//===========================================================================
// RL_SelectTexCoordArray
//===========================================================================
void RL_SelectTexCoordArray(int unit, int index)
{
	void *coords[MAX_TEX_UNITS];

	// Does this unit exist?
	if(unit >= numTexUnits)	return;

	memset(coords, 0, sizeof(coords));
	coords[unit] = texCoords[index];
	gl.Arrays(NULL, NULL, numTexUnits, coords, 0);
}

//===========================================================================
// RL_Bind
//===========================================================================
void RL_Bind(DGLuint texture)
{
	gl.Bind(renderTextures? texture : 0);
}

//===========================================================================
// RL_BindTo
//===========================================================================
void RL_BindTo(int unit, DGLuint texture)
{
	gl.SetInteger(DGL_ACTIVE_TEXTURE, unit);
	gl.Bind(renderTextures? texture : 0);
}

//===========================================================================
// RL_ClearHash
//===========================================================================
void RL_ClearHash(listhash_t *hash)
{
	memset(hash, 0, sizeof(listhash_t) * RL_HASH_SIZE);
}

//===========================================================================
// RL_Init
//	Called only once, from R_Init -> Rend_Init.
//===========================================================================
void RL_Init(void)
{
	RL_ClearHash(plainHash);
	RL_ClearHash(litHash);
	RL_ClearHash(dynHash);
	RL_ClearHash(shadowHash);

	memset(&skyMaskList, 0, sizeof(skyMaskList));
}

//===========================================================================
// RL_ClearVertices
//==========================================================================
void RL_ClearVertices(void)
{
	numVertices = 0;
}

//===========================================================================
// RL_DestroyVertices
//===========================================================================
void RL_DestroyVertices(void)
{
	int i;

	numVertices = maxVertices = 0;
	free(vertices);
	vertices = NULL;
	free(colors);
	colors = NULL;
	for(i = 0; i < NUM_TEXCOORD_ARRAYS; i++)
	{
		free(texCoords[i]);
		texCoords[i] = NULL;
	}
}

//===========================================================================
// RL_AllocateVertices
//	Allocate vertices from the global vertex array.
//===========================================================================
uint RL_AllocateVertices(uint count)
{
	uint i, base = numVertices;
	
	// Do we need to allocate more memory?
	numVertices += count;
	while(numVertices > maxVertices)
	{
		if(maxVertices == 0)
		{
			maxVertices = 16;
		}
		else
		{
			maxVertices *= 2;
		}

		vertices = realloc(vertices, sizeof(gl_vertex_t) * maxVertices);
		colors = realloc(colors, sizeof(gl_color_t) * maxVertices);
		for(i = 0; i < NUM_TEXCOORD_ARRAYS; i++)
		{
			texCoords[i] = realloc(texCoords[i],
				sizeof(gl_texcoord_t) * maxVertices);
		}
	}
	return base;	
}

//===========================================================================
// RL_DestroyList
//===========================================================================
void RL_DestroyList(rendlist_t *rl)
{
	// All the list data will be destroyed.
	if(rl->data) Z_Free(rl->data);
	rl->data = NULL;

#if _DEBUG
	Z_CheckHeap();
#endif

	rl->cursor = NULL;
	rl->tex.detail = NULL;
	rl->intertex.detail = NULL;
	rl->last = NULL;
	rl->size = 0;
	rl->flags = 0;
}

//===========================================================================
// RL_DeleteHash
//===========================================================================
void RL_DeleteHash(listhash_t *hash)
{
	int i;
	rendlist_t *list, *next;

	for(i = 0; i < RL_HASH_SIZE; i++)
	{
		for(list = hash[i].first; list; list = next)
		{
			next = list->next;
			RL_DestroyList(list);
			Z_Free(list);
		}
	}
	RL_ClearHash(hash);
}

//===========================================================================
// RL_DeleteLists
//	All lists will be destroyed.
//===========================================================================
void RL_DeleteLists(void)
{
	// Delete all lists.
	RL_DeleteHash(plainHash);
	RL_DeleteHash(litHash);
	RL_DeleteHash(dynHash);
	RL_DeleteHash(shadowHash);
	
	RL_DestroyList(&skyMaskList);

	RL_DestroyVertices();

	PRINT_PROF( PROF_RL_ADD_POLY );
	PRINT_PROF( PROF_RL_GET_LIST );
	PRINT_PROF( PROF_RL_RENDER_ALL );
	PRINT_PROF( PROF_RL_RENDER_NORMAL );
	PRINT_PROF( PROF_RL_SETUP_STATE );
	PRINT_PROF( PROF_RL_DRAW_PRIMS );
	PRINT_PROF( PROF_RL_DRAW_ELEMS );
	PRINT_PROF( PROF_RL_RENDER_LIGHT );
	PRINT_PROF( PROF_RL_RENDER_MASKED );

#ifdef _DEBUG
	Z_CheckHeap();
#endif
}

//===========================================================================
// RL_RewindList
//	Set the R/W cursor to the beginning.
//===========================================================================
void RL_RewindList(rendlist_t *rl)
{
	rl->cursor = rl->data;
	rl->last = NULL;
	rl->flags = 0;
	rl->tex.detail = NULL;
	rl->intertex.detail = NULL;

	// The interpolation target must be explicitly set (in RL_AddPoly).
	memset(&rl->intertex, 0, sizeof(rl->intertex));
	rl->interpos = 0;
}

//===========================================================================
// RL_RewindHash
//===========================================================================
void RL_RewindHash(listhash_t *hash)
{
	int i;
	rendlist_t *list;

	for(i = 0; i < RL_HASH_SIZE; i++)
	{
		for(list = hash[i].first; list; list = list->next)
			RL_RewindList(list);
	}
}

//===========================================================================
// RL_ClearLists
//	Called before rendering a frame.
//===========================================================================
void RL_ClearLists(void)
{
	// Clear the vertex array.
	RL_ClearVertices();

	RL_RewindHash(plainHash);
	RL_RewindHash(litHash);
	RL_RewindHash(dynHash);
	RL_RewindHash(shadowHash);

	RL_RewindList(&skyMaskList);

	// FIXME: Does this belong here?
	skyhemispheres = 0;
}

//===========================================================================
// RL_CreateList
//===========================================================================
rendlist_t *RL_CreateList(listhash_t *hash)
{
	rendlist_t *list = Z_Calloc(sizeof(rendlist_t), PU_STATIC, 0);

	if(hash->last) hash->last->next = list;
	hash->last = list;
	if(!hash->first) hash->first = list;
	return list;
}

//===========================================================================
// RL_GetListFor
//===========================================================================
rendlist_t *RL_GetListFor(rendpoly_t *poly, boolean useLights)
{
	listhash_t *hash, *table;
	rendlist_t *dest, *convertable = NULL;
	
	// Check for specialized rendering lists first.
	if(poly->flags & RPF_SKY_MASK)
	{
		return &skyMaskList;
	}

	// Choose the correct hash table.
	if(poly->flags & RPF_SHADOW)
	{
		table = shadowHash;
	}
	else
	{
		table = (useLights? litHash : plainHash);
	}

	// Find/create a list in the hash.
	hash = &table[ poly->tex.id % RL_HASH_SIZE ];
	for(dest = hash->first; dest; dest = dest->next)
	{
		if(dest->tex.id == poly->tex.id)
		{
			if(!poly->intertex.id && !dest->intertex.id)
			{
				// This will do great.
				return dest;
			}

			// Is this eligible for conversion to a blended list?
			if(poly->intertex.id && !dest->last && !convertable)
			{
				// If necessary, this empty list will be selected.
				convertable = dest;
			}

			// Possibly an exact match?
			if(poly->intertex.id == dest->intertex.id
				&& poly->interpos == dest->interpos)
			{
				return dest;
			}
		}
	}

	// Did we find a convertable list?
	if(convertable)
	{
		// This list is currently empty.
		memcpy(&convertable->intertex, &poly->intertex, 
			sizeof(poly->intertex));
		convertable->interpos = poly->interpos;
		return convertable;
	}

	// Create a new list.
	dest = RL_CreateList(hash);

	// Init the info.
	dest->tex.id = poly->tex.id;
	dest->tex.width = poly->tex.width;
	dest->tex.height = poly->tex.height;

	if(poly->intertex.id)
	{
		memcpy(&dest->intertex, &poly->intertex, sizeof(poly->intertex));
		dest->interpos = poly->interpos;
	}

	return dest;
}

//===========================================================================
// RL_GetLightListFor
//===========================================================================
rendlist_t *RL_GetLightListFor(DGLuint texture)
{
	listhash_t *hash;
	rendlist_t *dest;

	// Find/create a list in the hash.
	hash = &dynHash[ texture % RL_HASH_SIZE ];
	for(dest = hash->first; dest; dest = dest->next)
	{
		if(dest->tex.id == texture) 
			return dest;
	}

	// There isn't a list for this yet.
	dest = RL_CreateList(hash);
	dest->tex.id = texture;

	return dest;
}

//===========================================================================
// RL_AllocateData
//	Returns a pointer to the start of the allocated data.
//===========================================================================
void *RL_AllocateData(rendlist_t *list, int bytes)
{
	int required;
	int startOffset = list->cursor - list->data;
	primhdr_t *hdr;

	if(bytes <= 0) return NULL;

	// We require the extra bytes because we want that the end of the 
	// list data is always safe for writing-in-advance. This is needed
	// when the 'end of data' marker is written in RL_AddPoly.
	required = startOffset + bytes + LIST_DATA_PADDING;

	// First check that the data buffer of the list is large enough.
	if(required > list->size)
	{
		// Offsets must be preserved.
		byte *oldData    = list->data;
		int cursorOffset = -1;
		int lastOffset   = -1;

		if(list->cursor) cursorOffset = list->cursor - oldData;
		if(list->last) lastOffset = (byte*) list->last - oldData;

		// Allocate more memory for the data buffer.
		if(list->size == 0) list->size = 1024;
		while(list->size < required) list->size *= 2;

		list->data = Z_Realloc(list->data, list->size, PU_STATIC);

		// Restore main pointers.
		list->cursor = (cursorOffset >= 0? list->data + cursorOffset
						: list->data);
		list->last = (lastOffset >= 0? (primhdr_t*) (list->data + lastOffset)
					  : NULL);

		// Restore in-list pointers.
		if(oldData)
		{
			for(hdr = (primhdr_t*) list->data; ; 
				hdr = (primhdr_t*) ((byte*) hdr + hdr->size))
			{
				if(hdr->indices != NULL)
				{
					hdr->indices = (uint*)
						(list->data + ((byte*) hdr->indices - oldData));
				}

				// Check here in the end; primitive composition may be 
				// in progress.
				if(!hdr->size) break;
			}
		}
	}

	// Advance the cursor. 
	list->cursor += bytes;

	return list->data + startOffset;
}

//===========================================================================
// RL_AllocateIndices
//===========================================================================
void RL_AllocateIndices(rendlist_t *list, int numIndices)
{
	void *indices;
	
	list->last->numIndices = numIndices;
	indices = RL_AllocateData
		(list, sizeof(*list->last->indices) * numIndices);

	// list->last may change during RL_AllocateData.
	list->last->indices = indices;
}

//===========================================================================
// RL_QuadTexCoords
//===========================================================================
void RL_QuadTexCoords(gl_texcoord_t *tc, rendpoly_t *poly, gltexture_t *tex)
{
	float width, height;
	
	if(!tex->id) return;

	if(poly->flags & RPF_SHADOW)
	{
		// Shadows use the width and height from the polygon itself.
		width = poly->tex.width;
		height = poly->tex.height;

		if(poly->flags & RPF_HORIZONTAL)
		{
			// Special horizontal coordinates for wall shadows.
			tc[3].st[0] = tc[2].st[0] 
				= poly->texoffy / height;
			tc[3].st[1]	= tc[0].st[1]
				= poly->texoffx / width;
			tc[0].st[0] = tc[1].st[0]
				= tc[3].st[0] + (poly->top - poly->bottom) / height;
			tc[1].st[1] = tc[2].st[1]
				= tc[3].st[1] + poly->length / width;
			return;
		}
	}
	else
	{
		// Normally the texture's width and height are considered
		// constant inside a rendering list.
		width = tex->width;
		height = tex->height;
	}

	tc[0].st[0] = tc[3].st[0] 
		= poly->texoffx / width;
	tc[0].st[1]	= tc[1].st[1]
		= poly->texoffy / height;
	tc[1].st[0] = tc[2].st[0]
		= tc[0].st[0] + poly->length / width;
	tc[2].st[1] = tc[3].st[1]
		= tc[0].st[1] + (poly->top - poly->bottom) / height;
}

//===========================================================================
// RL_QuadDetailTexCoords
//===========================================================================
void RL_QuadDetailTexCoords
	(gl_texcoord_t *tc, rendpoly_t *poly, gltexture_t *tex)
{
	float mul = tex->detail->scale * detailScale;

	tc[0].st[0] = tc[3].st[0] 
		= poly->texoffx / tex->detail->width;
	tc[0].st[1]	= tc[1].st[1]
		= poly->texoffy / tex->detail->height;
	tc[1].st[0] = tc[2].st[0]
		= (tc[0].st[0] + poly->length / tex->detail->width) * mul;
	tc[2].st[1] = tc[3].st[1]
		= (tc[0].st[1] + (poly->top - poly->bottom) 
		  / tex->detail->height) * mul;
	tc[0].st[0] *= mul;
	tc[0].st[1]	*= mul;
	tc[1].st[1] *= mul;
	tc[3].st[0] *= mul;
}

//===========================================================================
// RL_QuadColors
//===========================================================================
void RL_QuadColors(gl_color_t *color, rendpoly_t *poly)
{
	if(poly->flags & RPF_SKY_MASK)
	{
		// Sky mask doesn't need a color.
		return;
	}
	if(poly->flags & RPF_GLOW)
	{
		memset(color, 255, 4 * 4);
		return;
	}
	if(poly->flags & RPF_SHADOW)
	{
		memcpy(color[0].rgba, poly->vertices[0].color.rgba, 4);
		memcpy(color[1].rgba, poly->vertices[1].color.rgba, 4);
		memcpy(color[2].rgba, poly->vertices[1].color.rgba, 4);
		memcpy(color[3].rgba, poly->vertices[0].color.rgba, 4);
		return;
	}

	// Just copy RGB, set A to 255.
	memcpy(color[0].rgba, poly->vertices[0].color.rgba, 3);
	memcpy(color[1].rgba, poly->vertices[1].color.rgba, 3);
	memcpy(color[2].rgba, poly->vertices[1].color.rgba, 3);
	memcpy(color[3].rgba, poly->vertices[0].color.rgba, 3);

	color[0].rgba[3] = color[1].rgba[3] 
		= color[2].rgba[3] = color[3].rgba[3] 
		= 255;
}

//===========================================================================
// RL_QuadVertices
//===========================================================================
void RL_QuadVertices(gl_vertex_t *v, rendpoly_t *poly)
{
	v[0].xyz[0] = v[3].xyz[0] = poly->vertices[0].pos[VX];
	v[0].xyz[1] = v[1].xyz[1] = poly->top;
	v[0].xyz[2] = v[3].xyz[2] = poly->vertices[0].pos[VY];
	v[1].xyz[0] = v[2].xyz[0] = poly->vertices[1].pos[VX];
	v[2].xyz[1] = v[3].xyz[1] = poly->bottom;
	v[1].xyz[2] = v[2].xyz[2] = poly->vertices[1].pos[VY];
}

//===========================================================================
// RL_QuadLightCoords
//===========================================================================
void RL_QuadLightCoords(gl_texcoord_t *tc, dynlight_t *dyn)
{
	tc[0].st[0] = tc[3].st[0] = dyn->s[0];
	tc[0].st[1] = tc[1].st[1] = dyn->t[0];
	tc[1].st[0] = tc[2].st[0] = dyn->s[1];
	tc[2].st[1] = tc[3].st[1] = dyn->t[1];
}

//===========================================================================
// RL_FlatDetailTexCoords
//===========================================================================
void RL_FlatDetailTexCoords
	(gl_texcoord_t *tc, float xy[2], rendpoly_t *poly, gltexture_t *tex)
{
	tc->st[0] = (xy[VX] + poly->texoffx) / tex->detail->width
		* detailScale * tex->detail->scale;
	tc->st[1] = (-xy[VY] - poly->texoffy) / tex->detail->height
		* detailScale * tex->detail->scale;
}

//===========================================================================
// RL_InterpolateTexCoordS
//	Inter = 0 in the bottom. Only 's' is affected.
//===========================================================================
void RL_InterpolateTexCoordS
	(gl_texcoord_t *tc, uint index, uint top, uint bottom, float inter)
{
	// Start working with the bottom.
	memcpy(&tc[index], &tc[bottom], sizeof(gl_texcoord_t));

	tc[index].st[0] += (tc[top].st[0] - tc[bottom].st[0]) * inter;
}

//===========================================================================
// RL_InterpolateTexCoordT
//	Inter = 0 in the bottom. Only 't' is affected.
//===========================================================================
void RL_InterpolateTexCoordT
	(gl_texcoord_t *tc, uint index, uint top, uint bottom, float inter)
{
	// Start working with the bottom.
	memcpy(&tc[index], &tc[bottom], sizeof(gl_texcoord_t));

	tc[index].st[1] += (tc[top].st[1] - tc[bottom].st[1]) * inter;
}

//===========================================================================
// RL_WriteQuad
//===========================================================================
void RL_WriteQuad(rendlist_t *list, rendpoly_t *poly)
{
	uint base;
	primhdr_t *hdr = NULL;

	// A quad is composed of two triangles.
	// We need four vertices.
	list->last->primSize = 4;
	base = RL_AllocateVertices(list->last->primSize);

	// Setup the indices.
	RL_AllocateIndices(list, 4);
	hdr = list->last;
	hdr->indices[0] = base;
	hdr->indices[1] = base + 1;
	hdr->indices[2] = base + 2;
	hdr->indices[3] = base + 3;
	
	// Primary texture coordinates.
	RL_QuadTexCoords(&texCoords[TCA_MAIN][base], poly, &list->tex);

	// Blend texture coordinates.
	if(list->intertex.id)
	{
		RL_QuadTexCoords(&texCoords[TCA_BLEND][base], poly, &list->intertex);
	}

	// Detail texture coordinates.
	if(list->tex.detail)
	{
		RL_QuadDetailTexCoords(&texCoords[TCA_DETAIL][base], poly,
			&list->tex);
	}
	if(list->intertex.detail)
	{
		RL_QuadDetailTexCoords(&texCoords[TCA_BLEND_DETAIL][base], poly,
			&list->intertex);
	}

	// Colors.
	RL_QuadColors(&colors[base], poly);

	// Vertices.
	RL_QuadVertices(&vertices[base], poly);

	// Light texture coordinates.
	if(list->last->light && IS_MTEX_LIGHTS)
	{
		RL_QuadLightCoords(&texCoords[TCA_LIGHT][base], list->last->light);
	}
}

//===========================================================================
// RL_WriteDivQuad
//===========================================================================
void RL_WriteDivQuad(rendlist_t *list, rendpoly_t *poly)
{
	gl_vertex_t *v;
	uint base;
	uint sideBase[2];
	int i, side, other, index, top, bottom, div;
	float z, height = poly->top - poly->bottom, inter;
	primhdr_t *hdr = NULL;

	// Vertex layout (and triangles for one side):
	// [n] = fan base vertex
	//
	//[0]-->---------1
	// |\.......     |
	// 8  \...  .....4
	// |    \ ....   |
	// 7      \   ...5
	// |        \    |
	// 6          \  |
	// |            \|
	// 3---------<--[2]
	//

	list->last->type = PT_DOUBLE_FAN;

	// A divquad is composed of two triangle fans.
	list->last->primSize = 4 + poly->divs[0].num + poly->divs[1].num;
	base = RL_AllocateVertices(list->last->primSize);

	// Allocate the indices.
	RL_AllocateIndices(list, 3 + poly->divs[1].num + 
		3 + poly->divs[0].num);

	hdr = list->last;		
	hdr->indices[0] = base;

	// The first four vertices are the normal quad corner points.
	RL_QuadTexCoords(&texCoords[TCA_MAIN][base], poly, &list->tex);
	if(list->intertex.id)
	{
		RL_QuadTexCoords(&texCoords[TCA_BLEND][base], poly, &list->intertex);
	}
	if(list->tex.detail)
	{
		RL_QuadDetailTexCoords(&texCoords[TCA_DETAIL][base], poly,
			&list->tex);
	}
	if(list->intertex.detail)
	{
		RL_QuadDetailTexCoords(&texCoords[TCA_BLEND_DETAIL][base], poly,
			&list->intertex);
	}
	RL_QuadColors(&colors[base], poly);
	RL_QuadVertices(&vertices[base], poly);

	// Texture coordinates for lights (normal quad corners).
	if(hdr->light && IS_MTEX_LIGHTS)
	{
		RL_QuadLightCoords(&texCoords[TCA_LIGHT][base], hdr->light);
	}

	// Index of the indices array.
	index = 0;

	// First vertices of each side (1=right, 0=left).
	sideBase[1] = base + 4;
	sideBase[0] = sideBase[1] + poly->divs[1].num;

	// Set the rest of the indices and init the division vertices.
	for(side = 0; side < 2; side++)	// Left->right is side zero.
	{
		other = !side;

		// The actual top/bottom corner vertex.
		top    = base + (!side? 1 : 0);
		bottom = base + (!side? 2 : 3);

		// Here begins the other triangle fan.
		if(side) hdr->beginOther = index;

		// The fan origin is the same for all the triangles.
		hdr->indices[index++] = base + (!side? 0 : 2);

		// The first (top/bottom) vertex of the side.
		hdr->indices[index++] = base + (!side? 1 : 3);

		// The division vertices.
		for(i = 0; i < poly->divs[other].num; i++)
		{
			// A division vertex of the other side.
			hdr->indices[index++] = div = sideBase[other] + i;

			// Division height of this vertex.
			z = poly->divs[other].pos[i];

			// We'll init this vertex by interpolating.
			inter = (z - poly->bottom) / height;

			if(!(poly->flags & RPF_SKY_MASK))
			{
				if(poly->flags & RPF_HORIZONTAL)
				{
					// Currently only shadows use this texcoord mode.
					RL_InterpolateTexCoordS(texCoords[TCA_MAIN], div,
											top, bottom, inter);
				}
				else
				{
					// Primary texture coordinates.
					RL_InterpolateTexCoordT(texCoords[TCA_MAIN], div, 
											top, bottom, inter);
				}

				// Detail texture coordinates.
				if(poly->tex.detail)
				{
					RL_InterpolateTexCoordT(texCoords[TCA_DETAIL], div,
						top, bottom, inter);
				}
				if(poly->intertex.detail)
				{
					RL_InterpolateTexCoordT(texCoords[TCA_BLEND_DETAIL], div,
						top, bottom, inter);
				}

				// Light coordinates.
				if(hdr->light && IS_MTEX_LIGHTS)
				{
					RL_InterpolateTexCoordT(texCoords[TCA_LIGHT], div, 
						top, bottom, inter);
				}
				
				// Blend texture coordinates.
				if(list->intertex.id)
				{
					RL_InterpolateTexCoordT(texCoords[TCA_BLEND], div,
						top, bottom, inter);
				}

				// Color.
				memcpy(&colors[div], &colors[top], sizeof(gl_color_t));
			}

			// Vertex.
			v = &vertices[div];
			v->xyz[0] = vertices[top].xyz[0];
			v->xyz[1] = z;
			v->xyz[2] = vertices[top].xyz[2];
		}

		// The last (bottom/top) vertex of the side.
		hdr->indices[index++] = base + (!side? 2 : 0);
	}
}

//===========================================================================
// RL_WriteFlat
//===========================================================================
void RL_WriteFlat(rendlist_t *list, rendpoly_t *poly)
{
	rendpoly_vertex_t *vtx;
	gl_color_t *col;
	gl_texcoord_t *tc;
	gl_vertex_t *v;
	uint base;
	int i;

	// A flat is composed of N triangles, where N = poly->numvertices - 2.
	list->last->primSize = poly->numvertices;
	base = RL_AllocateVertices(list->last->primSize);

	// Allocate the indices.
	RL_AllocateIndices(list, poly->numvertices);

	// Setup indices in a triangle fan.
	for(i = 0; i < poly->numvertices; i++)
	{
		list->last->indices[i] = base + i;
	}

	for(i = 0, vtx = poly->vertices; i < poly->numvertices; i++, vtx++)
	{
		// Coordinates.
		v = &vertices[base + i];
		v->xyz[0] = vtx->pos[VX];
		v->xyz[1] = poly->top;
		v->xyz[2] = vtx->pos[VY];

		if(poly->flags & RPF_SKY_MASK) 
		{
			// Skymask polys don't need any further data.
			continue;
		}

		// Primary texture coordinates.
		if(list->tex.id)
		{
			tc = &texCoords[TCA_MAIN][base + i];
			tc->st[0] = (vtx->pos[VX] + poly->texoffx) /
				(poly->flags & RPF_SHADOW? poly->tex.width : list->tex.width);
			tc->st[1] = (-vtx->pos[VY] - poly->texoffy) /
				(poly->flags & RPF_SHADOW? poly->tex.height :
				 list->tex.height);
		}

		// Detail texture coordinates.
		if(list->tex.detail)
		{
			RL_FlatDetailTexCoords(&texCoords[TCA_DETAIL][base + i], 
				vtx->pos, poly, &list->tex);
		}
		if(list->intertex.detail)
		{
			RL_FlatDetailTexCoords(&texCoords[TCA_BLEND_DETAIL][base + i],
				vtx->pos, poly, &list->intertex);
		}

		// Light coordinates.
		if(list->last->light && IS_MTEX_LIGHTS)
		{
			tc = &texCoords[TCA_LIGHT][base + i];
			tc->st[0] = (vtx->pos[VX] + list->last->light->s[0]) 
				* list->last->light->s[1];
			tc->st[1] = (-vtx->pos[VY] + list->last->light->t[0]) 
				* list->last->light->t[1];
		}

		// Blend texture coordinates.
		if(list->intertex.id)
		{
			tc = &texCoords[TCA_BLEND][base + i];
			tc->st[0] = (vtx->pos[VX] + poly->texoffx) / list->intertex.width;
			tc->st[1] = (-vtx->pos[VY] - poly->texoffy) 
				/ list->intertex.height;
		}

		// Color.
		col = &colors[base + i];
		if(poly->flags & RPF_GLOW)
		{
			memset(col->rgba, 255, 4);
		}
		else if(poly->flags & RPF_SHADOW)
		{
			memcpy(col->rgba, vtx->color.rgba, 4);
		}
		else
		{
			memcpy(col->rgba, vtx->color.rgba, 3);
			col->rgba[3] = 255;
		}
	}
}

//===========================================================================
// RL_EndWrite
//===========================================================================
void RL_EndWrite(rendlist_t *list)
{
	// The primitive has been written, update the size in the header.
	list->last->size = list->cursor - (byte*)list->last;

	// Write the end marker (which will be overwritten by the next
	// primitive). The idea is that this zero is interpreted as the
	// size of the following primhdr.
	*(int*) list->cursor = 0;
}

//===========================================================================
// RL_WriteDynLight
//===========================================================================
void RL_WriteDynLight
	(rendlist_t *list, dynlight_t *dyn, primhdr_t *prim, rendpoly_t *poly)
{
	uint i, base;
	gl_texcoord_t *tc;
	gl_color_t *col;
	void *ptr;

	ptr = RL_AllocateData(list, sizeof(primhdr_t));
	list->last = ptr;

	list->last->size = 0;
	list->last->type = prim->type;
	list->last->flags = 0;
	list->last->numIndices = prim->numIndices;
	ptr = RL_AllocateData(list, sizeof(uint) * list->last->numIndices);
	list->last->indices = ptr; 
	list->last->beginOther = prim->beginOther;

	// Make copies of the original vertices.
	list->last->primSize = prim->primSize;
	base = RL_AllocateVertices(prim->primSize);
	memcpy(&vertices[base], &vertices[prim->indices[0]], prim->primSize 
		* sizeof(gl_vertex_t));

	// Copy the vertex order from the original.
	for(i = 0; i < prim->numIndices; i++)
	{
		list->last->indices[i] = base + prim->indices[i] - prim->indices[0];
	}

	for(i = 0; i < prim->primSize; i++)
	{
		// Each vertex uses the light's color.
		col = &colors[base + i];
		memcpy(col->rgba, dyn->color, 3);
		col->rgba[3] = 255;
	}

	// Texture coordinates need a bit of calculation.
	tc = &texCoords[TCA_MAIN][base];
	if(poly->type == RP_FLAT)
	{
		for(i = 0; i < prim->primSize; i++)
		{
			tc[i].st[0] = (vertices[base + i].xyz[0] + dyn->s[0]) * dyn->s[1];
			tc[i].st[1] = (-vertices[base + i].xyz[2] + dyn->t[0]) * dyn->t[1];
		}
	}
	else 
	{
		RL_QuadLightCoords(tc, dyn);

		if(poly->type == RP_DIVQUAD)
		{
			int side, other, top, bottom, sideBase[2];
			float height = poly->top - poly->bottom;

			// First vertices of each side (1=right, 0=left).
			sideBase[1] = base + 4;
			sideBase[0] = sideBase[1] + poly->divs[1].num;

			// Set the rest of the indices and init the division vertices.
			for(side = 0; side < 2; side++)	// Left->right is side zero.
			{
				other = !side;

				// The actual top/bottom corner vertex.
				top    = base + (!side? 1 : 0);
				bottom = base + (!side? 2 : 3);

				// Number of vertices per side: 2 + numdivs
				for(i = 1; i <= poly->divs[other].num; i++)
				{
					RL_InterpolateTexCoordT(texCoords[TCA_MAIN], 
						sideBase[other] + i - 1, 
						top, bottom, (poly->divs[other].pos[i - 1] 
						- poly->bottom) / height);
				}
			}
		}
	}

	// The dynlight has been written.
	RL_EndWrite(list);
}

//===========================================================================
// RL_AddPoly
//	Adds the given poly onto the correct list.
//===========================================================================
void RL_AddPoly(rendpoly_t *poly)
{
	rendlist_t	*li;
	primhdr_t	*hdr;
	dynlight_t	*dyn;
	boolean		useLights = false;
	
	if(poly->flags & RPF_MASKED)
	{
		// Masked polys (walls) get a special treatment (=> vissprite).
		// This is needed because all masked polys must be sorted (sprites
		// are masked polys). Otherwise there will be artifacts.
		RL_AddMaskedPoly(poly);
		return;
	}

	BEGIN_PROF( PROF_RL_ADD_POLY );
	BEGIN_PROF( PROF_RL_GET_LIST );

	// Are lights allowed?
	if(!(poly->flags & (RPF_SKY_MASK | RPF_SHADOW)))
	{
		// In multiplicative mode, glowing surfaces are fullbright.
		// Rendering lights on them would be pointless.
		if(!IS_MUL || !( poly->flags & RPF_GLOW	
			|| (poly->sector && poly->sector->lightlevel >= 250) ))
		{
			// Surfaces lit by dynamic lights may need to be rendered 
			// differently than non-lit surfaces.
			if(poly->lights) 
			{
				useLights = true;
			}
		}
	}

	// Find/create a rendering list for the polygon's texture.
	li = RL_GetListFor(poly, useLights);
	
	// The entire list will use the same detail textures.
	li->tex.detail = poly->tex.detail;
	li->intertex.detail = poly->intertex.detail;

	END_PROF( PROF_RL_GET_LIST );

	// This becomes the new last primitive.
	li->last = hdr = RL_AllocateData(li, sizeof(primhdr_t));

	hdr->size    = 0;
	hdr->type    = PT_FAN; 
	hdr->indices = NULL;
	hdr->flags   = poly->flags;
	hdr->light   = (useLights? poly->lights : NULL);

	switch(poly->type)
	{
	case RP_QUAD:
		RL_WriteQuad(li, poly);
		break;

	case RP_DIVQUAD:
		RL_WriteDivQuad(li, poly);
		break;

	case RP_FLAT:
		RL_WriteFlat(li, poly);
		break;

	default:
		break;
	}
	
	RL_EndWrite(li);

	// Generate a dynlight primitive for each of the lights affecting 
	// the surface. Multitexturing may be used for the first light, so 
	// it's skipped.
	if(useLights)
		for(dyn = (IS_MTEX_LIGHTS? li->last->light->next : li->last->light);
			dyn; dyn = dyn->next)
		{
			RL_WriteDynLight( RL_GetLightListFor(dyn->texture), dyn, 
				li->last, poly );
		}
	
	END_PROF( PROF_RL_ADD_POLY );
}

//===========================================================================
// RL_FloatRGB
//===========================================================================
void RL_FloatRGB(byte *rgb, float *dest)
{
	int i;

	for(i = 0; i < 3; i++)
	{
		dest[i] = rgb[i] / 255.0f;
	}
	dest[3] = 1;
}

//===========================================================================
// RL_DrawPrimitives
//	Draws the privitives that match the conditions. If no condition bits
//	are given, all primitives are considered eligible.
//===========================================================================
void RL_DrawPrimitives(int conditions, rendlist_t *list)
{
	primhdr_t *hdr;
	float color[4];
	boolean bypass = false;

	// Should we just skip all this?
	if(conditions & DCF_SKIP) return;

	// Is blending allowed?
	if(conditions & DCF_NO_BLEND && list->intertex.id) return;

	// Should all blended primitives be included?
	if(conditions & DCF_BLEND && list->intertex.id)
	{
		// The other conditions will be bypassed.
		bypass = true;
	}

	BEGIN_PROF( PROF_RL_DRAW_PRIMS );

	// Compile our list of indices.
	for(hdr = (primhdr_t*) list->data; hdr->size; 
		hdr = (primhdr_t*) ((byte*) hdr + hdr->size))
	{
		// Check for skip conditions.
		if(!bypass)
		{
			if(conditions & DCF_JUST_ONE_LIGHT && hdr->light->next)
				continue;

			if(conditions & DCF_MANY_LIGHTS && !hdr->light->next)
				continue;
		}
		
		if(conditions & DCF_SET_LIGHT_ENV)
		{
			// Use the correct texture and color for the light.
			gl.SetInteger(DGL_ACTIVE_TEXTURE, 
				conditions & DCF_SET_LIGHT_ENV0? 0 : 1);
			RL_Bind(hdr->light->texture);
			RL_FloatRGB(hdr->light->color, color);
			gl.SetFloatv(DGL_ENV_COLOR, color);
			// Make sure the light is not repeated.
			gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
			gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
		}

		// Render a primitive (or two) as a triangle fan.
		if(hdr->type == PT_FAN)
		{
			gl.DrawElements(DGL_TRIANGLE_FAN, hdr->numIndices, 
				hdr->indices);
		}
		else if(hdr->type == PT_DOUBLE_FAN)
		{
			gl.DrawElements(DGL_TRIANGLE_FAN, hdr->beginOther,
				hdr->indices);

			gl.DrawElements(DGL_TRIANGLE_FAN, 
				hdr->numIndices - hdr->beginOther, 
				hdr->indices + hdr->beginOther);
		}
	}

	END_PROF( PROF_RL_DRAW_PRIMS );
}

//===========================================================================
// RL_BlendState
//===========================================================================
void RL_BlendState(rendlist_t *list, int modMode)
{
#ifdef _DEBUG
	if(numTexUnits < 2)
		Con_Error("RL_BlendState: Not enough texture units.\n");
#endif

	RL_SelectTexUnits(2);

	RL_BindTo(0, list->tex.id);
	RL_BindTo(1, list->intertex.id);

	gl.SetInteger(DGL_MODULATE_TEXTURE, modMode);
	gl.SetInteger(DGL_ENV_ALPHA, list->interpos * 256);
}

//===========================================================================
// RL_DetailFogState
//===========================================================================
void RL_DetailFogState(void)
{
	// The fog color alpha is probably meaningless?
	byte midGray[4] = { 0x80, 0x80, 0x80, fogColor[3] };
	gl.Enable(DGL_FOG);
	gl.Fogv(DGL_FOG_COLOR, midGray);
}

//===========================================================================
// RL_SetupListState
//	Set per-list GL state.
//	Returns the conditions to select primitives.
//===========================================================================
int RL_SetupListState(listmode_t mode, rendlist_t *list)
{
	switch(mode)
	{
	case LM_SKYMASK:
		// Render all primitives on the list without discrimination.
		return 0;

	case LM_ALL:		// All surfaces.
		// Should we do blending?
		if(list->intertex.id)
		{
			// Blend between two textures, modulate with primary color.
			RL_BlendState(list, 2);
		}
		else
		{
			// Normal modulation.
			RL_SelectTexUnits(1);
			RL_Bind(list->tex.id);
			gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
		}
		return 0;

	case LM_LIGHT_MOD_TEXTURE:
		// Modulate sector light, dynamic light and regular texture.
		RL_BindTo(1, list->tex.id);
		return DCF_SET_LIGHT_ENV0 | DCF_JUST_ONE_LIGHT | DCF_NO_BLEND;

	case LM_TEXTURE_PLUS_LIGHT:
		RL_BindTo(0, list->tex.id);
		return DCF_SET_LIGHT_ENV1 | DCF_NO_BLEND;

	case LM_FIRST_LIGHT:
		// Draw all primitives with more than one light
		// and all primitives which will have a blended texture.
		return DCF_SET_LIGHT_ENV0 | DCF_MANY_LIGHTS | DCF_BLEND;

	case LM_BLENDED:
		// Only render the blended surfaces.
		if(!list->intertex.id) return DCF_SKIP;
		RL_BlendState(list, 2);
		return 0;

	case LM_BLENDED_FIRST_LIGHT:
		// Only blended surfaces.
		if(!list->intertex.id) return DCF_SKIP;
		return DCF_SET_LIGHT_ENV0;

	case LM_WITHOUT_TEXTURE:
		// Only render the primitives affected by dynlights.
		return 0;

	case LM_LIGHTS:
		// The light lists only contain dynlight primitives.
		RL_Bind(list->tex.id);
		// Make sure the texture is not repeated.
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
		return 0;

	case LM_BLENDED_MOD_TEXTURE:
		// Blending required.
		if(!list->intertex.id) break;
	case LM_MOD_TEXTURE:
	case LM_MOD_TEXTURE_MANY_LIGHTS:
		// Texture for surfaces with (many) dynamic lights.
		// Should we do blending?
		if(list->intertex.id)
		{
			// Mode 3 actually just disables the second texture stage,
			// which would modulate with primary color.
			RL_BlendState(list, 3);
			// Render all primitives.
			return 0;
		}
		// No modulation at all.
		RL_SelectTexUnits(1);
		RL_Bind(list->tex.id);
		gl.SetInteger(DGL_MODULATE_TEXTURE, 0);
		return (mode == LM_MOD_TEXTURE_MANY_LIGHTS? DCF_MANY_LIGHTS : 0);

	case LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
		// Blending is not done now.
		if(list->intertex.id) break;
		if(list->tex.detail)
		{
			RL_SelectTexUnits(2);
			gl.SetInteger(DGL_MODULATE_TEXTURE, 9); // Tex+Detail, no color.
			RL_BindTo(0, list->tex.id);
			RL_BindTo(1, list->tex.detail->tex);
		}
		else
		{
			RL_SelectTexUnits(1);
			gl.SetInteger(DGL_MODULATE_TEXTURE, 0);
			RL_Bind(list->tex.id);
		}
		return 0;

	case LM_ALL_DETAILS:
		if(!list->tex.detail) break;
		RL_Bind(list->tex.detail->tex);
		// Render all surfaces on the list.
		return 0;

	case LM_UNBLENDED_TEXTURE_AND_DETAIL:
		// Only unblended. Details are optional.
		if(list->intertex.id) break;
		if(list->tex.detail)
		{
			RL_SelectTexUnits(2);
			gl.SetInteger(DGL_MODULATE_TEXTURE, 8);
			RL_BindTo(0, list->tex.id);
			RL_BindTo(1, list->tex.detail->tex);
		}
		else
		{
			// Normal modulation.
			RL_SelectTexUnits(1);
			gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
			RL_Bind(list->tex.id);
		}
		return 0;

	case LM_BLENDED_DETAILS:
		// We'll only render blended primitives.
		if(!list->intertex.id) break;
		if(!list->tex.detail || !list->intertex.detail) break;
	
		RL_BindTo(0, list->tex.detail->tex);
		RL_BindTo(1, list->intertex.detail->tex);
		gl.SetInteger(DGL_ENV_ALPHA, list->interpos * 256);		
		return 0;

	case LM_SHADOW:
		// Render all primitives.
		RL_Bind(list->tex.id);
		if(!list->tex.id)
		{
			// Apply a modelview shift.
			gl.MatrixMode(DGL_MODELVIEW);
			gl.PushMatrix();

			// Scale towards the viewpoint to avoid Z-fighting.
			gl.Translatef(vx, vy, vz);
			gl.Scalef(.99f, .99f, .99f);
			gl.Translatef(-vx, -vy, -vz);
		}
		return 0;

	default:
		break;
	}

	// Unknown mode, let's not draw anything.
	return DCF_SKIP;
}

//===========================================================================
// RL_FinishListState
//===========================================================================
void RL_FinishListState(listmode_t mode, rendlist_t *list)
{
	switch(mode)
	{
	default:
		break;

	case LM_SHADOW:
		if(!list->tex.id)
		{
			// Restore original modelview matrix.
			gl.MatrixMode(DGL_MODELVIEW);
			gl.PopMatrix();
		}
		break;
	}	  
}

//===========================================================================
// RL_SetupPassState
//	Setup GL state for an entire rendering pass.
//===========================================================================
void RL_SetupPassState(listmode_t mode)
{
	switch(mode)
	{
	case LM_SKYMASK:
		RL_SelectTexUnits(0);
		gl.Disable(DGL_ALPHA_TEST);
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		// We don't want to write to the color buffer.
		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE);
		// No need for fog.
		if(useFog) gl.Disable(DGL_FOG);
		break;

	case LM_BLENDED:
		RL_SelectTexUnits(2);
	case LM_ALL:
		// The first texture unit is used for the main texture.
		RL_SelectTexCoordArray(0, TCA_MAIN);
		RL_SelectTexCoordArray(1, TCA_BLEND);
		gl.Disable(DGL_ALPHA_TEST);
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		// Fog is allowed during this pass.
		if(useFog) gl.Enable(DGL_FOG);
		// All of the surfaces are opaque.
		gl.Disable(DGL_BLENDING);
		break;

	case LM_LIGHT_MOD_TEXTURE:
	case LM_TEXTURE_PLUS_LIGHT:
		// Modulate sector light, dynamic light and regular texture.
		RL_SelectTexUnits(2);
		if(mode == LM_LIGHT_MOD_TEXTURE)
		{
			RL_SelectTexCoordArray(0, TCA_LIGHT);
			RL_SelectTexCoordArray(1, TCA_MAIN);
			gl.SetInteger(DGL_MODULATE_TEXTURE, 4); // Light * texture.
		}
		else
		{
			RL_SelectTexCoordArray(0, TCA_MAIN);
			RL_SelectTexCoordArray(1, TCA_LIGHT);
			gl.SetInteger(DGL_MODULATE_TEXTURE, 5); // Texture + light.
		}
		gl.Disable(DGL_ALPHA_TEST);
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		// Fog is allowed during this pass.
		if(useFog) gl.Enable(DGL_FOG);
		// All of the surfaces are opaque.
		gl.Disable(DGL_BLENDING);
		break;

	case LM_FIRST_LIGHT:
		// One light, no texture.
		RL_SelectTexUnits(1);
		RL_SelectTexCoordArray(0, TCA_LIGHT);
		gl.SetInteger(DGL_MODULATE_TEXTURE, 6);
		gl.Disable(DGL_ALPHA_TEST);
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		// Fog is allowed during this pass.
		if(useFog) gl.Disable(DGL_FOG);
		// All of the surfaces are opaque.
		gl.Disable(DGL_BLENDING);
		break;

	case LM_BLENDED_FIRST_LIGHT:
		// One additive light, no texture.
		RL_SelectTexUnits(1);
		RL_SelectTexCoordArray(0, TCA_LIGHT);
		gl.SetInteger(DGL_MODULATE_TEXTURE, 7); // Add light, no color.
		gl.Enable(DGL_ALPHA_TEST);
		gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 1);
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		// Fog is allowed during this pass.
		if(useFog) gl.Disable(DGL_FOG);
		// All of the surfaces are opaque.
		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_ONE, DGL_ONE);
		break;

	case LM_WITHOUT_TEXTURE:
		RL_SelectTexUnits(0);
		gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
		gl.Disable(DGL_ALPHA_TEST);
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		// Fog must be disabled during this pass.
		gl.Disable(DGL_FOG);
		// All of the surfaces are opaque.
		gl.Disable(DGL_BLENDING);
		break;

	case LM_LIGHTS:
		RL_SelectTexUnits(1);
		RL_SelectTexCoordArray(0, TCA_MAIN);
		gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
		gl.Enable(DGL_ALPHA_TEST);
		gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 1);
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		// Fog would mess with the color (this is an additive pass).
		gl.Disable(DGL_FOG);
		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
		break;

	case LM_MOD_TEXTURE:
	case LM_MOD_TEXTURE_MANY_LIGHTS:
	case LM_BLENDED_MOD_TEXTURE:
		// The first texture unit is used for the main texture.
		RL_SelectTexCoordArray(0, TCA_MAIN);
		RL_SelectTexCoordArray(1, TCA_BLEND);
		gl.Disable(DGL_ALPHA_TEST);
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		// All of the surfaces are opaque.
		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_ZERO);
		// Fog would mess with the color (this is a multiplicative pass).
		gl.Disable(DGL_FOG);
		break;

	case LM_UNBLENDED_TEXTURE_AND_DETAIL:
		RL_SelectTexCoordArray(0, TCA_MAIN);
		RL_SelectTexCoordArray(1, TCA_DETAIL);
		gl.Disable(DGL_ALPHA_TEST);
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		// All of the surfaces are opaque.
		gl.Disable(DGL_BLENDING);
		// Fog is allowed.
		if(useFog) gl.Enable(DGL_FOG);
		break;

	case LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL:
		RL_SelectTexCoordArray(0, TCA_MAIN);
		RL_SelectTexCoordArray(1, TCA_DETAIL);
		gl.Disable(DGL_ALPHA_TEST);
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		// All of the surfaces are opaque.
		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_ZERO);
		// This is a multiplicative pass.
		gl.Disable(DGL_FOG);
		break;
	
	case LM_ALL_DETAILS:
		RL_SelectTexUnits(1);
		RL_SelectTexCoordArray(0, TCA_DETAIL);
		gl.SetInteger(DGL_MODULATE_TEXTURE, 0);
		gl.Disable(DGL_ALPHA_TEST);
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		// All of the surfaces are opaque.
		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_SRC_COLOR);
		// Use fog to fade the details, if fog is enabled.
		if(useFog) RL_DetailFogState();
		break;

	case LM_BLENDED_DETAILS:
		RL_SelectTexUnits(2);
		RL_SelectTexCoordArray(0, TCA_DETAIL);
		RL_SelectTexCoordArray(1, TCA_BLEND_DETAIL);
		gl.SetInteger(DGL_MODULATE_TEXTURE, 3);
		gl.Disable(DGL_ALPHA_TEST);
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		// All of the surfaces are opaque.
		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_SRC_COLOR);
		// Use fog to fade the details, if fog is enabled.
		if(useFog) RL_DetailFogState();
		break;

	case LM_SHADOW:
		// A bit like 'negative lights'.
		RL_SelectTexUnits(1);
		RL_SelectTexCoordArray(0, TCA_MAIN);
		gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
		gl.Enable(DGL_ALPHA_TEST);
		gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 1);
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		// Set normal fog, if it's enabled.
		if(useFog)
		{
			gl.Enable(DGL_FOG);
			gl.Fogv(DGL_FOG_COLOR, fogColor);
		}
		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		break;

	default:
		break;
	}
}

//===========================================================================
// RL_RenderLists
//	Renders the given lists. They must not be empty.
//===========================================================================
void RL_RenderLists(listmode_t mode, rendlist_t **lists, int num)
{
	int i;

	// If the first list is empty, we do nothing. Normally we expect
	// all lists to contain something.
	if(!num || lists[0]->last == NULL) return;

	// Setup GL state that's common to all the lists in this mode.
	RL_SetupPassState(mode);

	// Draw each given list.
	for(i = 0; i < num; i++)
	{
		// Setup GL state for this list, and
		// draw the necessary subset of primitives on the list.
		RL_DrawPrimitives( RL_SetupListState(mode, lists[i]), lists[i] );

		// Some modes require cleanup.
		RL_FinishListState(mode, lists[i]);
	}
}

//===========================================================================
// RL_CollectLists
//	Extracts a selection of lists from the hash.
//===========================================================================
uint RL_CollectLists(listhash_t *table, rendlist_t **lists) 
{
	rendlist_t *it;
	uint i, count = 0;

	// Collect a list of rendering lists.
	for(i = 0; i < RL_HASH_SIZE; i++)
	{
		for(it = table[i].first; it; it = it->next)
		{
			// Only non-empty lists are collected.
			if(it->last != NULL)
			{
				if(count == MAX_RLISTS)
				{
#ifdef _DEBUG
					Con_Error("RL_CollectLists: Ran out of MAX_RLISTS.\n");
#else
					return count;
#endif
				}
				lists[ count++ ] = it;
			}
		}
	}
	return count;
}

//===========================================================================
// RL_LockVertices
//===========================================================================
void RL_LockVertices(void)
{
	// We're only locking the vertex and color arrays, so disable the
	// texcoord arrays for now. Every pass will enable/disable the texcoords
	// that are needed.
	gl.DisableArrays(0, 0, DGL_ALL_BITS);

	// Actually, don't lock anything. (Massive slowdown?)
	gl.Arrays(vertices, colors, 0, NULL, 0 /*numVertices*/);
}

//===========================================================================
// RL_UnlockVertices
//===========================================================================
void RL_UnlockVertices(void)
{
	// Nothing was locked.
	//gl.UnlockArrays();
}

//===========================================================================
// RL_RenderAllLists
//	We have several different paths to accommodate both multitextured 
//	details	and dynamic lights. Details take precedence (they always cover 
//	entire primitives, and usually *all* of the surfaces in a scene).
//===========================================================================
void RL_RenderAllLists(void)
{
	// Pointers to all the rendering lists.
	rendlist_t *lists[MAX_RLISTS];
	uint count;

	BEGIN_PROF( PROF_RL_RENDER_ALL );

	// The sky might be visible. Render the needed hemispheres.
	Rend_RenderSky(skyhemispheres);				

	RL_LockVertices();

	// Mask the sky in the Z-buffer.
	lists[0] = &skyMaskList;
	RL_RenderLists(LM_SKYMASK, lists, 1);

	// Render the real surfaces of the visible world.
	BEGIN_PROF( PROF_RL_RENDER_NORMAL );
	
/*
 * Unlit Primitives
 */
	// Collect all normal lists.
	count = RL_CollectLists(plainHash, lists);
	if(IS_MTEX_DETAILS)
	{
		// Draw details for unblended surfaces in this pass.
		RL_RenderLists(LM_UNBLENDED_TEXTURE_AND_DETAIL, lists, count);

		// Blended surfaces.
		RL_RenderLists(LM_BLENDED, lists, count);
	}
	else
	{
		// Blending is done during this pass.
		RL_RenderLists(LM_ALL, lists, count);
	}

/*
 * Lit Primitives
 */
	// Then the lit primitives.
	count = RL_CollectLists(litHash, lists);

	// If multitexturing is available, we'll use it to our advantage
	// when rendering lights.
	if(IS_MTEX_LIGHTS) 
	{
		if(IS_MUL)
		{
			// All (unblended) surfaces with exactly one light can be 
			// rendered in a single pass.
			RL_RenderLists(LM_LIGHT_MOD_TEXTURE, lists, count);

			// Render surfaces with many lights without a texture, just 
			// with the first light.
			RL_RenderLists(LM_FIRST_LIGHT, lists, count);
		}
		else // Additive ('foggy') lights.
		{
			RL_RenderLists(LM_TEXTURE_PLUS_LIGHT, lists, count);

			// Render surfaces with blending.
			RL_RenderLists(LM_BLENDED, lists, count);

			// Render the first light for surfaces with blending.
			// (Not optimal but shouldn't matter; texture is changed for 
			// each primitive.)
			RL_RenderLists(LM_BLENDED_FIRST_LIGHT, lists, count);
		}
	}
	else // Multitexturing is not available for lights.
	{
		if(IS_MUL)
		{
			// Render all lit surfaces without a texture.
			RL_RenderLists(LM_WITHOUT_TEXTURE, lists, count);
		}
		else
		{
			if(IS_MTEX_DETAILS) // Draw detail textures using multitexturing.
			{
				// Unblended surfaces with a detail.
				RL_RenderLists(LM_UNBLENDED_TEXTURE_AND_DETAIL, lists, count);

				// Blended surfaces without details.
				RL_RenderLists(LM_BLENDED, lists, count);
				
				// Details for blended surfaces.
				RL_RenderLists(LM_BLENDED_DETAILS, lists, count);
			}
			else
			{
				RL_RenderLists(LM_ALL, lists, count);
			}
		}
	}

/*
 * Dynamic Lights
 */
	// Draw all dynamic lights (always additive).
	count = RL_CollectLists(dynHash, lists);
	RL_RenderLists(LM_LIGHTS, lists, count);

/*
 * Texture Modulation Pass
 */
	if(IS_MUL)
	{
		// Finish the lit surfaces that didn't yet get a texture.
		count = RL_CollectLists(litHash, lists);
		if(IS_MTEX_DETAILS)
		{
			RL_RenderLists(LM_UNBLENDED_MOD_TEXTURE_AND_DETAIL, lists, count);
			RL_RenderLists(LM_BLENDED_MOD_TEXTURE, lists, count);
			RL_RenderLists(LM_BLENDED_DETAILS, lists, count);
		}
		else
		{
			if(IS_MTEX_LIGHTS)
			{
				RL_RenderLists(LM_MOD_TEXTURE_MANY_LIGHTS, lists, count);
			}
			else
			{
				RL_RenderLists(LM_MOD_TEXTURE, lists, count);			
			}
		}
	}	

/*
 * Detail Modulation Pass
 */
	// If multitexturing is not available for details, we need to apply 
	// them as an extra pass over all the detailed surfaces.
	if(r_detail)
	{
		// Render detail textures for all surfaces that need them.
		count = RL_CollectLists(plainHash, lists);
		if(IS_MTEX_DETAILS)
		{
			// Blended detail textures.
			RL_RenderLists(LM_BLENDED_DETAILS, lists, count);
		}
		else
		{
			RL_RenderLists(LM_ALL_DETAILS, lists, count);

			count = RL_CollectLists(litHash, lists);
			RL_RenderLists(LM_ALL_DETAILS, lists, count);
		}
	}

/*
 * Shadow Pass: Objects and FakeRadio
 */
	{
		int oldr = renderTextures;
		renderTextures = true;
		count = RL_CollectLists(shadowHash, lists);

		RL_RenderLists(LM_SHADOW, lists, count);
		
		renderTextures = oldr;
	}

	END_PROF( PROF_RL_RENDER_NORMAL );

	RL_UnlockVertices();

	// Return to the normal GL state.
	RL_SelectTexUnits(1);
	gl.DisableArrays(true, true, DGL_ALL_BITS);
	gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
	gl.Enable(DGL_DEPTH_WRITE);
	gl.Enable(DGL_DEPTH_TEST);
	gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
	gl.Enable(DGL_ALPHA_TEST);
	gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 0);
	gl.Enable(DGL_BLENDING);
	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
	if(useFog) 
	{
		gl.Enable(DGL_FOG);
		gl.Fogv(DGL_FOG_COLOR, fogColor);
	}
	else
	{
		gl.Disable(DGL_FOG);
	}

	BEGIN_PROF( PROF_RL_RENDER_MASKED );

	// Draw masked walls, sprites and models.
	Rend_DrawMasked();

	// Draw particles.
	PG_Render();

	END_PROF( PROF_RL_RENDER_MASKED );

	END_PROF( PROF_RL_RENDER_ALL );
}
