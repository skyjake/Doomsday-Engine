
//**************************************************************************
//**
//** REND_LIST.C
//**
//** Doomsday Rendering Lists v3.0
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"

#define DD_PROFILE
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

#define MAX_INDICES			65536

#define MAX_TEX_UNITS		4

#define RL_HASH_SIZE		128

// Number of extra bytes to keep allocated in the end of each rendering list.
#define LIST_DATA_PADDING	16

// FIXME: Rlist allocation could be dynamic.
#define MAX_RLISTS			1024 

#define IS_MUL_MODE			(!dlBlend && !whitefog)

/*
#define BYTESIZE		1
#define SHORTSIZE		2
#define LONGSIZE		4
#define FLOATSIZE		4
*/
/*
#define NORMALBIAS		2 //1
#define SHADOWBIAS		2 //1
#define DYNLIGHTBIAS	0
#define DLITBIAS		0
#define DETAILBIAS		0
*/

/*#define IFCOL	if(with_col)
#define IFDET	if(with_det)
#define IFTEX	if(with_tex)
#define IFTC	if(with_tex || with_det)*/

// Drawing condition flags.
#define DCF_NO_LIGHTS				0x00000001
#define DCF_FEW_LIGHTS				0x00000002
#define DCF_FIRST_UNIT_LIGHTS		0x00000004
#define DCF_EXTRA_UNIT_LIGHTS		0x00000008
#define	DCF_NO_SINGLE_PASS_TEXTURE	0x00000010
#define DCF_NO_BLEND				0x00000020
#define DCF_BLEND					0x00000040

// List Modes.
typedef enum listmode_e
{
	// Major modes:
	LM_NORMAL_MUL,			// normal surfaces (multiplicative lights)
	LM_NORMAL_ADD,			// normal surfaces (additive lights)

	// Minor modes:
	LM_MUL_SINGLE_PASS,
	LM_NO_LIGHTS,
	LM_FEW_LIGHTS,
	LM_MANY_LIGHTS,
	LM_EXTRA_LIGHTS,
	LM_NEEDS_TEXTURE,
	
	// Submodes:
	LMS_LAST_UNIT_TEXTURE_ONLY,
	LMS_LIGHTS_TEXTURE_MUL,
	LMS_Z_DONE,
	LMS_LIGHTS_ADD_FIRST,
	LMS_LIGHTS_ADD_SUBSEQUENT,
	LMS_TEXTURE_MUL,
	LMS_SINGLE_TEXTURE,

/*
	LID_SKYMASK,				// Draw only into Z-buffer.
	//LID_NORMAL,					// Normal walls and planes (dlBlend=1, fog).
	//LID_NORMAL_DLIT,			// Normal, DLIT with no textures.
	LID_DLIT_NOTEX,				// Just DLIT with no textures (automatical).
	LID_DLIT_TEXTURED,			// DLIT with multiplicative blending.
	LID_LIGHTS,					// Dynamic lights (always additive).
	LID_SINGLE_PASS_LIGHTS,		// Dynamic lights (multitex, 1 source, 1 pass).
	//LID_DYNAMIC_LIGHTS,
	LID_DETAILS
	//LID_SHADOWS*/
}
listmode_t;

// Lists for skymask.
enum
{
	RLSKY_FLATS,
	RLSKY_WALLS,
	NUM_RLSKY
};

// Lists for dynamic lights.
/*enum
{
	RLDYN_FLATS,
	RLDYN_WALLS,
	RLDYN_GLOW,
	NUM_RLDYN
};*/

// Types of rendering primitives.
typedef enum primtype_e
{
	PT_TRIANGLES			// Used for most stuff.
} 
primtype_t;

// Texture coordinate array indices.
enum
{
	TCA_MAIN,				// Main texture.
	TCA_BLEND,				// Blendtarget texture.
	TCA_DETAIL,				// Detail texture coordinates.
	TCA_LIGHT1,				// The first dynamic lights. Only #TU-1 of these
	TCA_LIGHT2,				//	will be actually used.
	TCA_LIGHT3,
	NUM_TEXCOORD_ARRAYS
};

// TYPES -------------------------------------------------------------------

typedef struct glvertex_s
{
	float xyz[4];		// The fourth is padding.
}
glvertex_t;

typedef struct gltexcoord_s
{
	float st[2];
}
gltexcoord_t;

typedef struct glcolor_s
{
	byte rgba[4];
}
glcolor_t;

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
	short flags;				// RPF_*.

	// Elements in the vertex array for this primitive. 
	// The indices are always contiguous: indices[0] is the base, and 
	// indices[1...n] > indices[0]. 
	// All indices in the range indices[0]...indices[n] are used by this
	// primitive (some are shared).
	// Since everything is triangles, count is a multiple of 3.
	uint numIndices;	
	uint *indices;

	// Number of vertices in the primitive.
	uint primSize;

	// Dynamic lights on the surface.
	ushort numLights;
	dynlight_t *lights;
	
	// Array of texture coordinates for lights. There is a texcoord for
	// each element of the primitive. These can be accessed with:
	// primSize * light + (indices[i] - indices[0])
	gltexcoord_t *lightCoords;	

	/*union primhdr_data_u {
		//lumobj_t	*light;			// For RPF_LIGHT polygons.
		int			shadowradius;	// For RPF_SHADOW polygons.
	};*/
}
primhdr_t;

// The primitive data follows immediately after the header.

// Primvertices are used by the datablocks to store vertex data.
// 2D vertex.
/*typedef struct primvertex2_s
{
	float	pos[2];				// X and Y coordinates.
	byte	color[4];			// Color of the vertex (RGBA).
	float	dist;				// Distance to the vertex.
}
primvertex2_t;

typedef struct texcoord_s
{
	float	s, t;
}
texcoord_t;

typedef struct color3_s
{
	float	rgb[3];
}
color3_t;*/

// Data for a quad (wall segment).
/*typedef struct primquad_s
{
	float	length;				// Length of the wall segment.
	float	top, bottom;		// Top and bottom heights.
	primvertex2_t vertices[2];	// Start and end vertex.
} 
primquad_t;

// Data for a divquad (wall segment with divided sides).
typedef struct primdivquad_s
{
	primquad_t quad;			// The data for a normal quad.
	int		numdivs[2];			// Number of divisions for start and end.
	float	divs[1];			// Really [numdivs[0] + numdivs[1]].
	// The division heights follow:	first numdivs[0] floats and then
	// numdivs[1] floats.
}
primdivquad_t;*/

// Data for a flat (planes).
/*typedef struct primflat_s
{
	float	z;					// Z height.
	int		numvertices;		// Number of vertices for the poly.		
	primvertex2_t vertices[1];	// Really [numvertices].
}
primflat_t;*/

// Rendering List 'has' flags.
#define RLHAS_DLIT		0x1		// Primitives with dynamic lights.
#define RLHAS_DETAIL	0x2		// ...with RPF_DETAIL.

/*
 * The rendering list. When the list is resized, pointers in the primitives
 * need to be restored so that they point to the new list.
 */
typedef struct rendlist_s
{
	struct rendlist_s *next;
	DGLuint	tex;				// The name of the texture for this list.
	int		texw, texh;			// Width and height of the texture.
	DGLuint	intertex;			// Secondary tex to be used in interpolation.
	int		intertexw, intertexh;
	float	interpos;			// 0 = primary, 1 = secondary texture
	detailinfo_t *detail;		// Detail texture name and dimensions.
	int		size;				// Number of bytes allocated for the data.
	byte	*data;				// Data for a number of polygons (The List).
	byte	*cursor;			// A pointer to data, for reading/writing.
	primhdr_t *last;			// Pointer to the last primitive (or NULL).
	int		has;				
}
rendlist_t;

/*typedef struct divquaddata_s 
{
	primdivquad_t *quad;
	float zpos[2];
	float height;
	float *heights[2];
}
divquaddata_t;*/

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

vissprite_t *R_NewVisSprite(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void RL_SetupState(listmode_t mode, rendlist_t *list);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int			skyhemispheres;
extern int			useDynLights, dlBlend, simpleSky;
extern boolean		whitefog;
extern float		maxLightDist;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean				renderTextures = true;

// Intensity of angle-based wall lighting.
float				rend_light_wall_angle = 1;

// Rendering parameters for detail textures.
float				detailFactor = .5f;
float				detailMaxDist = 256;
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
static glvertex_t *vertices;
static gltexcoord_t *texCoords[NUM_TEXCOORD_ARRAYS];
static glcolor_t *colors;

static uint numVertices, maxVertices;

static uint numIndices;
static uint indices[MAX_INDICES];

/*
 * The rendering lists.
 */
//static rendlist_t rlists[MAX_RLISTS]; // The list of rendering lists.
static listhash_t list_hash[RL_HASH_SIZE];

static rendlist_t mask_rlists[NUM_RLSKY];
/*static rendlist_t dl_rlists[NUM_RLDYN];
static rendlist_t shadow_rlist;*/

static rendlist_t *ptr_mask_rlists[NUM_RLSKY];
/*static rendlist_t *ptr_dl_rlists[NUM_RLDYN];
static rendlist_t *ptr_shadow_rlist[1] = { &shadow_rlist };*/

// The rendering state for RL_Draw(Div)Quad and RL_DrawFlat.
static int with_tex, with_col, with_det;
//static float rl_texw, rl_texh;
static detailinfo_t *rl_detail;


// CODE --------------------------------------------------------------------

// Some utilities first... -------------------------------------------------

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
	int i;
	
	vis->issprite = false;
	vis->distance = (poly->vertices[0].dist + poly->vertices[1].dist) / 2;
	vis->wall.texture = poly->tex;
	vis->wall.masked = texmask;	// Store texmask status in flip.
	vis->wall.top = poly->top;
	vis->wall.bottom = poly->bottom;
	for(i = 0; i < 2; i++)
	{
		vis->wall.vertices[i].pos[VX] = poly->vertices[i].pos[VX];
		vis->wall.vertices[i].pos[VY] = poly->vertices[i].pos[VY];
		memcpy(&vis->wall.vertices[i].color, poly->vertices[i].color.rgb, 3);
		vis->wall.vertices[i].color |= 0xff000000; // Alpha.
	}
	vis->wall.texc[0][VX] = poly->texoffx / (float) poly->texw;
	vis->wall.texc[1][VX] = vis->wall.texc[0][VX] + poly->length/poly->texw;
	vis->wall.texc[0][VY] = poly->texoffy / (float) poly->texh;
	vis->wall.texc[1][VY] = vis->wall.texc[0][VY] 
		+ (poly->top - poly->bottom)/poly->texh;
}

//===========================================================================
// RL_VertexColors
//	Color distance attenuation, extralight, fixedcolormap.
//	"Torchlight" is white, regardless of the original RGB.
//===========================================================================
void RL_VertexColors(rendpoly_t *poly, int lightlevel, byte *rgb)
{
	int		i, c;
	float	light, real, minimum;
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

	for(i=0, vtx=poly->vertices; i<poly->numvertices; i++, vtx++)
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

		for(c=0; c<3; c++)
			vtx->color.rgb[c] = (DGLubyte) ((usewhite? 0xff : rgb[c]) * real);
	}
}

//===========================================================================
// RL_PrepareFlat
//===========================================================================
void RL_PrepareFlat(rendpoly_t *poly, int numvrts, fvertex_t *vrts, 
					int dir, subsector_t *subsector)
{
	float distances[MAX_POLY_SIDES];
	fvertex_t *vtx;
	int	i, lightlevel = poly->vertices[0].color.rgb[CR];
	rendpoly_vertex_t *rpv;

	if(!numvrts || !vrts)
	{
		// Take the subsector's vertices.
		numvrts = subsector->numverts;
		vrts = subsector->verts;
	}

	// We're preparing a plane here.
	poly->type = RP_FLAT;	

	// Calculate the distance to each vertex.
	for(i = 0; i < numvrts; i++) distances[i] = Rend_PointDist2D(&vrts[i].x);

	// Copy the vertices to the poly.
	if(subsector && subsector->flags & DDSUBF_MIDPOINT)
	{
		// Triangle fan base is the midpoint of the subsector.
		poly->numvertices = 2 + numvrts;
		poly->vertices[0].pos[VX] = subsector->midpoint.x;
		poly->vertices[0].pos[VY] = subsector->midpoint.y;
		poly->vertices[0].dist = Rend_PointDist2D(&subsector->midpoint.x);
		
		vtx = vrts + (!dir? 0 : numvrts-1);
		rpv = poly->vertices + 1;
	}
	else
	{
		poly->numvertices = numvrts;
		// The first vertex is always the same: vertex zero.
		rpv = poly->vertices;
		rpv->pos[VX] = vrts[0].x;
		rpv->pos[VY] = vrts[0].y;
		rpv->dist = distances[0];

		vtx = vrts + (!dir? 1 : numvrts-1);
		rpv++; 
		numvrts--;
	}
	// Add the rest of the vertices.
	for(; numvrts > 0; numvrts--, (!dir? vtx++ : vtx--), rpv++)
	{
		rpv->pos[VX] = vtx->x;
		rpv->pos[VY] = vtx->y;
		rpv->dist = distances[vtx - vrts];
	}
	if(poly->numvertices > numvrts)
	{
		// Readd the first vertex so the triangle fan wraps around.
		memcpy(rpv, poly->vertices + 1, sizeof(*rpv));
	}
	
	// Calculate the color for each vertex.
	RL_VertexColors(poly, lightlevel, subsector->sector->rgb);
}

// The Rendering Lists -----------------------------------------------------

//===========================================================================
// RL_Init
//	Called only once, from R_Init -> Rend_Init.
//===========================================================================
void RL_Init(void)
{
	int i;

	for(i = 0; i < NUM_RLSKY; i++)
	{
		ptr_mask_rlists[i] = &mask_rlists[i];
	}
/*	for(i = 0; i < NUM_RLDYN; i++)
	{
		ptr_dl_rlists[i] = &dl_rlists[i];
	}*/

	//numrlists = 0;
	//memset(rlists, 0, sizeof(rlists));
	memset(list_hash, 0, sizeof(list_hash));
	memset(mask_rlists, 0, sizeof(mask_rlists));
	//memset(dl_rlists, 0, sizeof(dl_rlists));
	//memset(&shadow_rlist, 0, sizeof(shadow_rlist));
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

		vertices = realloc(vertices, sizeof(glvertex_t) * maxVertices);
		colors = realloc(colors, sizeof(glcolor_t) * maxVertices);
		for(i = 0; i < NUM_TEXCOORD_ARRAYS; i++)
		{
			texCoords[i] = realloc(texCoords[i],
				sizeof(gltexcoord_t) * maxVertices);
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
	rl->detail = NULL;
	rl->last = NULL;
	rl->size = 0;
	rl->has = false;
}

//===========================================================================
// RL_DeleteLists
//	All lists will be destroyed.
//===========================================================================
void RL_DeleteLists(void)
{
	int	i;
	rendlist_t *list, *next;

	// Delete all lists.
	//for(i = 0; i < numrlists; i++) RL_DestroyList(&rlists[i]);
	for(i = 0; i < RL_HASH_SIZE; i++)
	{
		for(list = list_hash[i].first; list; list = next)
		{
			next = list->next;
			RL_DestroyList(list);
			Z_Free(list);
		}
	}
	memset(list_hash, 0, sizeof(list_hash));
	for(i = 0; i < NUM_RLSKY; i++) RL_DestroyList(&mask_rlists[i]);
//	for(i = 0; i < NUM_RLDYN; i++) RL_DestroyList(&dl_rlists[i]);
//	RL_DestroyList(&shadow_rlist);
	//numrlists = 0;

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
	rl->has = 0;

	// The interpolation target must be explicitly set (in RL_AddPoly).
	rl->intertex = 0;
	rl->intertexw = 0;
	rl->intertexh = 0;
}

//===========================================================================
// RL_ClearLists
//	Called before rendering a frame.
//===========================================================================
void RL_ClearLists(void)
{
	int	i;
	rendlist_t *list;

	// Clear the vertex array.
	RL_ClearVertices();

	//for(i = 0; i < numrlists; i++) RL_RewindList(&rlists[i]);
	for(i = 0; i < RL_HASH_SIZE; i++)
	{
		for(list = list_hash[i].first; list; list = list->next)
			RL_RewindList(list);
	}
	for(i = 0; i < NUM_RLSKY; i++) RL_RewindList(&mask_rlists[i]);
//	for(i = 0; i < NUM_RLDYN; i++) RL_RewindList(&dl_rlists[i]);
//	RL_RewindList(&shadow_rlist);

	// Types for the 'static' lists.
//	mask_rlists[RLSKY_WALLS].type = rl_quads;
//	mask_rlists[RLSKY_FLATS].type = rl_flats;
/*	dl_rlists[RLDYN_WALLS].type = dl_rlists[RLDYN_GLOW].type = rl_quads;
	dl_rlists[RLDYN_FLATS].type = rl_flats;
	shadow_rlist.type = rl_flats;*/

	// Dynamic light textures.
/*	dl_rlists[RLDYN_WALLS].tex = dltexname;
	dl_rlists[RLDYN_FLATS].tex = dltexname;
	dl_rlists[RLDYN_GLOW].tex = glowtexname;*/

	// FIXME: Does this belong here?
	skyhemispheres = 0;
}

//===========================================================================
// RL_GetListFor
//===========================================================================
rendlist_t *RL_GetListFor(rendpoly_t *poly)
{
//	int type;
	listhash_t *hash;
	rendlist_t *dest, *convertable = NULL;
	
	// Check for specialized rendering lists first.
/*	if(poly->flags & RPF_SHADOW)
	{
		return &shadow_rlist;
	}*/
	if(poly->flags & RPF_SKY_MASK)
	{
		return &mask_rlists[poly->type==RP_QUAD? RLSKY_WALLS : RLSKY_FLATS];
	}
/*	if(poly->flags & RPF_WALL_GLOW)
	{
		return &dl_rlists[RLDYN_GLOW];
	}
	if(poly->flags & RPF_LIGHT) // Dynamic lights?
	{
		return &dl_rlists[poly->type==rp_quad? RLDYN_WALLS : RLDYN_FLATS];
	}*/

	// Find a suitable normal list. 
	//type = (poly->type == rp_flat? rl_flats : rl_quads);

	hash = &list_hash[ poly->tex % RL_HASH_SIZE ];
	for(dest = hash->first; dest; dest = dest->next)
	{
		if(dest->tex == poly->tex)// && dest->type == type)
		{
			if(!poly->blendtex && !dest->intertex)
			{
				// This will do great.
				return dest;
			}

			// Is this eligible for conversion to a blended list?
			if(poly->blendtex && !dest->last && !convertable)
			{
				// If necessary, this empty list will be selected.
				convertable = dest;
			}

			// Possibly an exact match?
			if(poly->blendtex == dest->intertex
				&& poly->blendpos == dest->interpos)
			{
				return dest;
			}

			/*if(dest->intertex != poly->blendtex)
				continue;
				&& (dest->intertex != poly->blendtex
					|| dest->interpos != poly->blendpos))
			{
				// This list does not match the blending parameters.
				continue;
			}*/
			
			// This is it.
			//return dest;
		}
	}

#ifdef _DEBUG
	if(poly->tex == poly->blendtex)
	{
		poly->tex = poly->tex;
	}
#endif

	// Did we find a convertable list?
	if(convertable)
	{
		// This list is currently empty.
		convertable->intertex  = poly->blendtex;
		convertable->intertexw = poly->blendtexw;
		convertable->intertexh = poly->blendtexh;
		convertable->interpos  = poly->blendpos;
		return convertable;
	}

	// Create a new list.
	dest = Z_Calloc(sizeof(rendlist_t), PU_STATIC, 0);
	if(hash->last) hash->last->next = dest;
	hash->last = dest;
	if(!hash->first) hash->first = dest;

	// Init the info.
	dest->tex = poly->tex;
	dest->texw = poly->texw;
	dest->texh = poly->texh;
	dest->detail = poly->detail;

	if(poly->blendtex)
	{
		dest->intertex  = poly->blendtex;
		dest->intertexw = poly->blendtexw;
		dest->intertexh = poly->blendtexh;
		dest->interpos  = poly->blendpos;
	}

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

		if(list->cursor)
		{
			cursorOffset = list->cursor - oldData;
		}
		if(list->last)
		{
			lastOffset = (byte*) list->last - oldData;
		}

		// Allocate more memory for the data buffer.
		if(list->size == 0) list->size = 1024;
		while(list->size < required)
		{
			list->size *= 2;
		}

		list->data = Z_Realloc(list->data, list->size, PU_STATIC);

		// Restore main pointers.
		list->cursor = 
			(cursorOffset >= 0? list->data + cursorOffset : list->data);
		list->last = 
			(lastOffset >= 0? (primhdr_t*) (list->data + lastOffset) : NULL);

		// Restore in-list pointers.
		if(oldData)
		{
			for(hdr = (primhdr_t*) list->data; ; 
				hdr = (primhdr_t*) ((byte*) hdr + hdr->size))
			{
				hdr->indices = (uint*) (list->data 
					+ ((byte*) hdr->indices - oldData));
				if(hdr->lightCoords)
				{
					hdr->lightCoords = (gltexcoord_t*) (list->data 
						+ ((byte*) hdr->lightCoords - oldData));
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
	list->last->numIndices = numIndices;
	list->last->indices = RL_AllocateData(list, 
		sizeof(*list->last->indices) * numIndices);
}

//===========================================================================
// RL_AllocateLightCoords
//===========================================================================
void RL_AllocateLightCoords(rendlist_t *list)
{
	list->last->lightCoords = RL_AllocateData(list,
		sizeof(*list->last->lightCoords) * list->last->primSize
		* (list->last->numLights - maxArrayLights));
}

//===========================================================================
// RL_TexCoordArray
//===========================================================================
gltexcoord_t *RL_TexCoordArray(rendlist_t *list, int index)
{
	if(index < maxArrayLights)
	{
		return &texCoords[TCA_LIGHT1 + index][list->last->indices[0]];
	}
	else
	{
		return &list->last->lightCoords[ (index - maxArrayLights) 
			* list->last->primSize ];
	}
}

//===========================================================================
// RL_QuadTexCoords
//===========================================================================
void RL_QuadTexCoords(gltexcoord_t *tc, rendpoly_t *poly, rendlist_t *list)
{
	tc[0].st[0] = tc[3].st[0] 
		= poly->texoffx / list->texw;
	tc[0].st[1]	= tc[1].st[1]
		= poly->texoffy / list->texh;
	tc[1].st[0] = tc[2].st[0]
		= tc[0].st[0] + poly->length / list->texw;
	tc[2].st[1] = tc[3].st[1]
		= tc[0].st[1] + (poly->top - poly->bottom) / list->texh;
}

//===========================================================================
// RL_QuadInterTexCoords
//===========================================================================
void RL_QuadInterTexCoords
	(gltexcoord_t *tc, rendpoly_t *poly, rendlist_t *list)
{
	tc[0].st[0] = tc[3].st[0] 
		= poly->texoffx / list->intertexw;
	tc[0].st[1]	= tc[1].st[1]
		= poly->texoffy / list->intertexh;
	tc[1].st[0] = tc[2].st[0]
		= tc[0].st[0] + poly->length / list->intertexw;
	tc[2].st[1] = tc[3].st[1]
		= tc[0].st[1] + (poly->top - poly->bottom) / list->intertexh;
}

//===========================================================================
// RL_QuadColors
//===========================================================================
void RL_QuadColors(glcolor_t *color, rendpoly_t *poly)
{
	if(poly->flags & RPF_GLOW)
	{
		memset(color, 255, 4 * 4);
		return;
	}

	memcpy(color[0].rgba, poly->vertices[0].color.rgb, 3);
	memcpy(color[1].rgba, poly->vertices[1].color.rgb, 3);
	memcpy(color[2].rgba, poly->vertices[1].color.rgb, 3);
	memcpy(color[3].rgba, poly->vertices[0].color.rgb, 3);

	// No alpha.
	color[0].rgba[3] = color[1].rgba[3] 
		= color[2].rgba[3] = color[3].rgba[3] 
		= 255;
}

//===========================================================================
// RL_QuadVertices
//===========================================================================
void RL_QuadVertices(glvertex_t *v, rendpoly_t *poly)
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
void RL_QuadLightCoords(gltexcoord_t *tc, dynlight_t *dyn)
{
	tc[0].st[0] = tc[3].st[0] = dyn->s[0];
	tc[0].st[1] = tc[1].st[1] = dyn->t[0];
	tc[1].st[0] = tc[2].st[0] = dyn->s[1];
	tc[2].st[1] = tc[3].st[1] = dyn->t[1];
}

//===========================================================================
// RL_InterpolateTexCoordT
//	Inter = 0 in the bottom. Only 't' is affected.
//===========================================================================
void RL_InterpolateTexCoordT
	(gltexcoord_t *tc, uint index, uint top, uint bottom, float inter)
{
	// Start working with the bottom.
	memcpy(&tc[index], &tc[bottom], sizeof(gltexcoord_t));

	tc[index].st[1] += (tc[top].st[1] - tc[bottom].st[1]) * inter;
}

//===========================================================================
// RL_WriteQuad
//===========================================================================
void RL_WriteQuad(rendlist_t *list, rendpoly_t *poly)
{
	uint base;
	dynlight_t *dyn;
	primhdr_t *hdr = NULL;

	// A quad is composed of two triangles.
	// We need four vertices.
	list->last->primSize = 4;
	base = RL_AllocateVertices(list->last->primSize);

	// Setup the indices (in strip order).
	RL_AllocateIndices(list, 6);
	hdr = list->last;
	hdr->indices[0] = base;
	hdr->indices[1] = base + 1;
	hdr->indices[2] = base + 3;
	hdr->indices[3] = base + 3;
	hdr->indices[4] = base + 1;
	hdr->indices[5] = base + 2;
	
	// Primary texture coordinates.
	RL_QuadTexCoords(&texCoords[TCA_MAIN][base], poly, list);

	// Blend texture coordinates.
	if(list->intertex)
	{
		RL_QuadInterTexCoords(&texCoords[TCA_BLEND][base], poly, list);
	}

	// Colors.
	RL_QuadColors(&colors[base], poly);

	// Vertices.
	RL_QuadVertices(&vertices[base], poly);

	// Texture coordinates for lights.
	RL_AllocateLightCoords(list);

	for(dyn = list->last->lights; dyn; dyn = dyn->next)
	{
		RL_QuadLightCoords( RL_TexCoordArray(list, dyn->index), dyn );
	}
}

//===========================================================================
// RL_WriteDivQuad
//===========================================================================
void RL_WriteDivQuad(rendlist_t *list, rendpoly_t *poly)
{
	glvertex_t *v;
	uint base;
	dynlight_t *dyn;
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

	// A divquad is composed of two triangle fans.
	list->last->primSize = 4 + poly->divs[0].num + poly->divs[1].num;
	base = RL_AllocateVertices(list->last->primSize);

	// Allocate the indices.
	RL_AllocateIndices(list, 3 * (list->last->primSize - 2));
	list->last->indices[0] = base;

	// The first four vertices are the normal quad corner points.
	RL_QuadTexCoords(&texCoords[TCA_MAIN][base], poly, list);		
	if(list->intertex)
		RL_QuadInterTexCoords(&texCoords[TCA_BLEND][base], poly, list);
	RL_QuadColors(&colors[base], poly);
	RL_QuadVertices(&vertices[base], poly);

	// Texture coordinates for lights (normal quad corners).
	RL_AllocateLightCoords(list);
	for(dyn = list->last->lights; dyn; dyn = dyn->next)
	{
		RL_QuadLightCoords( RL_TexCoordArray(list, dyn->index), dyn );
	}

	// Index of the indices array.
	index = 0;

	// First vertices of each side (1=right, 0=left).
	sideBase[1] = base + 4;
	sideBase[0] = sideBase[1] + poly->divs[1].num;

	hdr = list->last;

	// Set the rest of the indices and init the division vertices.
	for(side = 0; side < 2; side++)	// Left->right is side zero.
	{
		other = !side;

		// The actual top/bottom corner vertex.
		top    = base + (!side? 1 : 0);
		bottom = base + (!side? 2 : 3);

		// Number of vertices per side: 2 + numdivs
		for(i = 0; i <= poly->divs[other].num; i++)
		{
			// The fan origin is the same for all the triangles.
			hdr->indices[index++] = base + (!side? 0 : 2);
			
			if(i == 0)
			{
				// The first (top/bottom) vertex of the side.
				hdr->indices[index++] = base + (!side? 1 : 3);
			}
			else
			{
				// A division vertex of the other side.
				hdr->indices[index++] = div = sideBase[other] + i - 1;

				// Division height of this vertex.
				z = poly->divs[other].pos[i - 1];

				// We'll init this vertex by interpolating.
				inter = (z - poly->bottom) / height;

				// Primary texture coordinates.
				RL_InterpolateTexCoordT(texCoords[TCA_MAIN], div, 
					top, bottom, inter);
				
				// Blend texture coordinates.
				if(list->intertex)
				{
					RL_InterpolateTexCoordT(texCoords[TCA_BLEND], div,
						top, bottom, inter);
				}

				// Color.
				memcpy(&colors[div], &colors[top], sizeof(glcolor_t));

				// Vertex.
				v = &vertices[div];
				v->xyz[0] = vertices[top].xyz[0];
				v->xyz[1] = z;
				v->xyz[2] = vertices[top].xyz[2];

				// Light coordinates for each light.
				for(dyn = hdr->lights; dyn; dyn = dyn->next)
				{
					RL_InterpolateTexCoordT( 
						RL_TexCoordArray(list, dyn->index),
						div - base, 
						top - base, 
						bottom - base,
						inter );
				}
			}

			// The next vertex of the side (last of the triangle).
			if(i == poly->divs[other].num)
			{
				// The last (bottom/top) vertex of the side.
				hdr->indices[index++] = base + (!side? 2 : 0);
			}
			else
			{
				// A division vertex of the other side. Will be inited
				// on the next iteration.
				hdr->indices[index++] = sideBase[other] + i;
			}
		}
	}
}

//===========================================================================
// RL_WriteFlat
//===========================================================================
void RL_WriteFlat(rendlist_t *list, rendpoly_t *poly)
{
	rendpoly_vertex_t *vtx;
	glcolor_t *col;
	gltexcoord_t *tc;
	glvertex_t *v;
	uint base;
	dynlight_t *dyn;
	int i, k;

	// A flat is composed of N triangles, where N = poly->numvertices - 2.
	list->last->primSize = poly->numvertices;
	base = RL_AllocateVertices(list->last->primSize);

	// Allocate the indices.
	RL_AllocateIndices(list, 3 * (list->last->primSize - 2));

	// Setup indices in a triangle fan.
	for(i = 0, k = 0; i < poly->numvertices - 2; i++)
	{
		// All triangles share the first vertex.
		list->last->indices[k++] = base;
		list->last->indices[k++] = base + i + 1;
		list->last->indices[k++] = base + i + 2;
	}

	// Allocate texture coordinates for lights.
	RL_AllocateLightCoords(list);

	for(i = 0, vtx = poly->vertices; i < poly->numvertices; i++, vtx++)
	{
		// Primary texture coordinates.
		tc = &texCoords[TCA_MAIN][base + i];
		tc->st[0] = (vtx->pos[VX] + poly->texoffx) / list->texw;
		tc->st[1] = (-vtx->pos[VY] - poly->texoffy) / list->texh;

		// Blend texture coordinates.
		if(list->intertex)
		{
			tc = &texCoords[TCA_BLEND][base + i];
			tc->st[0] = (vtx->pos[VX] + poly->texoffx) / list->intertexw;
			tc->st[1] = (-vtx->pos[VY] - poly->texoffy) / list->intertexh;
		}

		// Color.
		col = &colors[base + i];
		if(poly->flags & RPF_GLOW)
		{
			memset(col->rgba, 255, 4);
		}
		else
		{
			memcpy(col->rgba, vtx->color.rgb, 3);
			col->rgba[3] = 255;
		}
		
		// Coordinates.
		v = &vertices[base + i];
		v->xyz[0] = vtx->pos[VX];
		v->xyz[1] = poly->top;
		v->xyz[2] = vtx->pos[VY];

		// Lights.
		for(dyn = list->last->lights; dyn; dyn = dyn->next)
		{
			tc = RL_TexCoordArray(list, dyn->index);
			tc[i].st[0] = (vtx->pos[VX] + dyn->s[0]) * dyn->s[1];
			tc[i].st[1] = (-vtx->pos[VY] + dyn->t[0]) * dyn->t[1];
		}
	}
}

//===========================================================================
// RL_AddPoly
//	Adds the given poly onto the correct list.
//===========================================================================
void RL_AddPoly(rendpoly_t *poly)
{
	rendlist_t		*li;
	int				i;
	float			mindist;
	primhdr_t		*hdr;
	dynlight_t		*dyn;
	
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

	// Find/create a rendering list for the polygon's texture.
	li = RL_GetListFor(poly);

	END_PROF( PROF_RL_GET_LIST );

	// Calculate the distance to each vertex.
	if(!(poly->flags & (RPF_SKY_MASK | RPF_SHADOW)))
	{
		// This is a "normal" poly. 
		for(i = 0; i < poly->numvertices; i++)
		{
			// See if we need to calculate distances.
			if(poly->type != RP_FLAT)
				poly->vertices[i].dist = Rend_PointDist2D(poly->vertices[i].pos);
			// What is the minimum distance?
			if(!i || poly->vertices[i].dist < mindist)
				mindist = poly->vertices[i].dist;
		}
		if(mindist < detailMaxDist)	// Detail limit.
		{
			poly->flags |= RPF_DETAIL;	// Eligible for a detail texture.
			if(poly->detail) li->has |= RLHAS_DETAIL;
		}

		// Should we take note of texture interpolation? 
		/*if(poly->blendtex && !li->intertex)
		{
			li->intertex  = poly->blendtex;
			li->intertexw = poly->blendtexw;
			li->intertexh = poly->blendtexh;
			li->interpos  = poly->blendpos;
		}*/
	}

	// This becomes the new last primitive.
	li->last = hdr = RL_AllocateData(li, sizeof(primhdr_t));

	//(primhdr_t*) li->cursor;
	//li->cursor += sizeof(*hdr);

	hdr->size = 0;
	hdr->type = PT_TRIANGLES;
	hdr->indices = NULL;
	hdr->flags = poly->flags;
	hdr->lights = poly->lights;
	hdr->numLights = 0;
	hdr->lightCoords = NULL;

	// In multiplicative mode, glowing surfaces are fullbright.
	// Rendering lights on them would be pointless.
	if(poly->flags & RPF_GLOW && IS_MUL_MODE)
	{
		hdr->lights = NULL;
	}

	// Light indices.
	for(i = 0, dyn = hdr->lights; dyn; dyn = dyn->next)
	{
		hdr->numLights++;
		dyn->index = i++;
	}

	if(hdr->lights) li->has |= RLHAS_DLIT;
	
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
	}
	

/*	hdr->ptr = NULL;
	hdr->flags = poly->flags;
	hdr->type = poly->type;
	hdr->texoffx = poly->texoffx;
	hdr->texoffy = poly->texoffy;
	// The light data is needed for calculating texture coordinates 
	// for RPF_LIGHT polygons.
	hdr->lights = poly->lights;

//	hdr->alpha = (poly->flags & RPF_ALPHA? poly->alpha : 1);

	// Type specific data.
	switch(poly->type)
	{
	case rp_quad:
	case rp_divquad:
		pq = (primquad_t*) li->cursor;
		pq->top = poly->top;
		pq->bottom = poly->bottom;
		pq->length = poly->length;
		for(i = 0, vtx = poly->vertices, pv2 = pq->vertices; i < 2; 
			i++, vtx++, pv2++)
		{
			pv2->pos[VX] = vtx->pos[VX];
			pv2->pos[VY] = vtx->pos[VY];
			pv2->dist = vtx->dist;
			if(poly->flags & RPF_GLOW)
				memset(pv2->color, 255, 4);
			else
			{
				pv2->color[CR] = vtx->color.rgb[CR];
				pv2->color[CG] = vtx->color.rgb[CG];
				pv2->color[CB] = vtx->color.rgb[CB];
				pv2->color[CA] = 255;
			}
		}
		// Divquads need the division info.
		if(poly->type == rp_divquad)
		{
			pdq = (primdivquad_t*) pq;
			for(i = 0, k = 0; i < 2; i++)
			{
				pdq->numdivs[i] = poly->divs[i].num;
				for(d = 0; d < pdq->numdivs[i]; d++)
					pdq->divs[k++] = poly->divs[i].pos[d];
			}
			// Move the cursor forward the necessary amount.
			li->cursor += sizeof(*pdq) + sizeof(float)*(k - 1);
		}
		else
		{
			// Move the cursor forward the necessary amount.
			li->cursor += sizeof(*pq);
		}
		break;

	case rp_flat:
		pf = (primflat_t*) li->cursor;
		pf->numvertices = poly->numvertices;
		pf->z = poly->top;
		for(i = 0, vtx = poly->vertices, pv2 = pf->vertices; 
			i < pf->numvertices; i++, vtx++, pv2++)
		{
			pv2->pos[VX] = vtx->pos[VX];
			pv2->pos[VY] = vtx->pos[VY];
			pv2->dist = vtx->dist;
			if(poly->flags & RPF_GLOW)
				memset(pv2->color, 255, 4);
			else
			{
				pv2->color[CR] = vtx->color.rgb[CR];
				pv2->color[CG] = vtx->color.rgb[CG];
				pv2->color[CB] = vtx->color.rgb[CB];
				pv2->color[CA] = 255;
			}
		}
		// Move the cursor forward.
		li->cursor += sizeof(*pf) + sizeof(*pv2)*(pf->numvertices - 1);
		break;
	}
*/	
	// The primitive has been written, update the size in the header.
	li->last->size = li->cursor - (byte*)li->last;

	// Write the end marker (which will be overwritten by the next
	// primitive). The idea is that this zero is interpreted as the
	// size of the following primhdr.
	*(int*) li->cursor = 0;

	END_PROF( PROF_RL_ADD_POLY );
}

/*
//===========================================================================
// RL_QuadTexCoords
//===========================================================================
void RL_QuadTexCoords(primhdr_t *prim, primquad_t *quad, texcoord_t *tex)
{
	tex[0].s = prim->texoffx/rl_texw;
	tex[0].t = prim->texoffy/rl_texh;
	tex[1].s = tex[0].s + quad->length/rl_texw;
	tex[1].t = tex[0].t + (quad->top - quad->bottom)/rl_texh;
}
*/

#if 0
//===========================================================================
// RL_QuadDetailTexCoords
//===========================================================================
void RL_QuadDetailTexCoords
	(primhdr_t *prim, primquad_t *quad, texcoord_t *tex)
{
	float mul = rl_detail->scale * detailScale;
	int i;

	tex[0].s = prim->texoffx/rl_detail->width;
	tex[0].t = prim->texoffy/rl_detail->height;
	tex[1].s = tex[0].s + quad->length/rl_detail->width;
	tex[1].t = tex[0].t + (quad->top - quad->bottom)/rl_detail->height;
	for(i = 0; i < 2; i++) 
	{
		tex[i].s *= mul;
		tex[i].t *= mul;
	}
}

//===========================================================================
// RL_DetailDistFactor
//===========================================================================
float RL_DetailDistFactor(primvertex2_t *vertices, int idx)
{
	float f = 1 - vertices[idx].dist 
		/ (rl_detail->maxdist? rl_detail->maxdist : detailMaxDist);
	// Clamp to [0,1].
	if(f > 1) f = 1;
	if(f < 0) f = 0;
	return f;
}

//===========================================================================
// RL_DetailColor
//===========================================================================
void RL_DetailColor(color3_t *col, primvertex2_t *vertices, int num) 
{
	float mul;
	int i, k;

	// Also calculate the color for the detail texture.
	// It's <vertex-color> * <distance-factor> * <detail-factor>.
	for(i = 0; i < num; i++)
	{
		mul = RL_DetailDistFactor(vertices, i) 
			* detailFactor 
			* rl_detail->strength;
		for(k = 0; k < 3; k++)
			col[i].rgb[k] = vertices[i].color[k]/255.0f * mul;
	}
}	

//===========================================================================
// RL_DrawQuad
//===========================================================================
void RL_DrawQuad(primhdr_t *prim)
{
	primquad_t *quad = (primquad_t*) ((byte*)prim + sizeof(*prim));
	primvertex2_t *vtx = quad->vertices;
	color3_t detail_color[2];
	texcoord_t texcoord[2];

	// Calculate normal texture coordinates.
	IFTEX	RL_QuadTexCoords(prim, quad, texcoord);

	// Calculate detail texture coordinates and detail color.
	IFDET {
		RL_QuadDetailTexCoords(prim, quad, texcoord);
		RL_DetailColor(detail_color, vtx, 2);
	}

	// Dynamic light polygons are the same color all over.
	//if(prim->flags & RPF_LIGHT) gl.Color3ubv(vtx[0].color);

	// Start side.
	IFCOL	gl.Color3ubv(vtx[0].color);
	IFDET	gl.Color3fv(detail_color[0].rgb);
	IFTC	gl.TexCoord2f(texcoord[0].s, texcoord[1].t);
			gl.Vertex3f(vtx[0].pos[VX], quad->bottom, vtx[0].pos[VY]);
		
	IFTC	gl.TexCoord2f(texcoord[0].s, texcoord[0].t);
			gl.Vertex3f(vtx[0].pos[VX], quad->top, vtx[0].pos[VY]);
		
	// End side.
	IFCOL	gl.Color3ubv(vtx[1].color);
	IFDET	gl.Color3fv(detail_color[1].rgb);
	IFTC	gl.TexCoord2f(texcoord[1].s, texcoord[0].t);
			gl.Vertex3f(vtx[1].pos[VX], quad->top, vtx[1].pos[VY]);
		
	IFTC	gl.TexCoord2f(texcoord[1].s, texcoord[1].t);
			gl.Vertex3f(vtx[1].pos[VX], quad->bottom, vtx[1].pos[VY]);
}

//===========================================================================
// RL_GetDivQuad
//===========================================================================
void RL_GetDivQuad(primhdr_t *prim, divquaddata_t *div)
{
	div->quad = (primdivquad_t*) ((byte*)prim + sizeof(*prim));
	div->zpos[0] = div->quad->quad.top;
	div->zpos[1] = div->quad->quad.bottom;
	div->height = div->zpos[0] - div->zpos[1];
	div->heights[0] = div->quad->divs;
	div->heights[1] = div->quad->divs + div->quad->numdivs[0];
}

//===========================================================================
// RL_DrawDivQuad
//	DivQuads are rendered as two triangle fans.
//===========================================================================
void RL_DrawDivQuad(primhdr_t *prim)
{
	divquaddata_t div;
/*	primdivquad_t *divquad = (primdivquad_t*) ((byte*)prim + sizeof(*prim));
	primvertex2_t *vtx = divquad->quad.vertices;
	float zpos[2] = { divquad->quad.top, divquad->quad.bottom }, z;
	float *divpos[2];*/
	primvertex2_t *vtx;
	color3_t detail_color[2];
	texcoord_t tc[2];
	int i, side, other;
	float z;

	/*divpos[0] = divquad->divs;
	divpos[1] = divquad->divs + divquad->numdivs[0];*/

	RL_GetDivQuad(prim, &div);
	vtx = div.quad->quad.vertices;
	
	// Calculate texture coordinates.
	IFTEX
	{
		RL_QuadTexCoords(prim, &div.quad->quad, tc);
	}
	IFDET 
	{ 
		RL_QuadDetailTexCoords(prim, &div.quad->quad, tc);
		RL_DetailColor(detail_color, vtx, 2);
	}

	// Dynamic light polygons are the same color all over.
	/*if(prim->flags & RPF_LIGHT) gl.Color3ubv(vtx[0].color);*/

	// A more general algorithm is used for divquads.
	for(side = 0; side < 2; side++)	// Left->right is side zero.
	{
		other = !side;

		// We'll render two fans.
		gl.Begin(DGL_TRIANGLE_FAN);

		// The origin vertex.
		IFCOL	gl.Color3ubv(vtx[side].color);
		IFTC	gl.TexCoord2f(tc[side].s, tc[side].t);
		IFDET	gl.Color3fv(detail_color[side].rgb);
				gl.Vertex3f(vtx[side].pos[VX], div.zpos[side], vtx[side].pos[VY]);

		for(i = 0; i <= div.quad->numdivs[other] + 1; i++)
		{
			// The vertex on the opposite side.
			IFCOL	gl.Color3ubv(vtx[other].color);
			IFDET	gl.Color3fv(detail_color[other].rgb);
			if(!i) // The top/bottom vertex.
			{
				IFTC gl.TexCoord2f(tc[other].s, tc[side].t);
				gl.Vertex3f(vtx[other].pos[VX], div.zpos[side], 
					vtx[other].pos[VY]);
			}
			else if(i == div.quad->numdivs[other] + 1) // The bottom/top vertex.
			{
				IFTC gl.TexCoord2f(tc[other].s, tc[other].t);
				gl.Vertex3f(vtx[other].pos[VX], div.zpos[other], 
					vtx[other].pos[VY]);
			}
			else // A division vertex.
			{
				z = div.heights[other][i - 1];
				IFTC { 
					// Calculate the texture coordinate.
					gl.TexCoord2f(tc[other].s, (z - div.zpos[1]) / div.height
						* (tc[0].t - tc[1].t) + tc[1].t);
				}					
				gl.Vertex3f(vtx[other].pos[VX], z, vtx[other].pos[VY]);
			}
		}
		
		gl.End();
	}
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
// RL_DrawSinglePassQuad
//===========================================================================
void RL_DrawSinglePassQuad(primhdr_t *prim)
{
	primquad_t *quad = (primquad_t*) ((byte*)prim + sizeof(*prim));
	primvertex2_t *vtx = quad->vertices;
	dynlight_t *dyn = prim->lights;
	//DGLuint currentTexture = gl.GetInteger(DGL_TEXTURE_BINDING);
	texcoord_t texcoord[2];
	float lightcolor[4];

	// Calculate normal texture coordinates.
	RL_QuadTexCoords(prim, quad, texcoord);

	//if(dyn->texture != currentTexture)
	//{
	// Must change textures.
	gl.End();
	gl.Bind(dyn->texture);
	RL_FloatRGB(dyn->color, lightcolor);
	gl.SetFloatv(DGL_ENV_COLOR, lightcolor);

	gl.Begin(DGL_QUADS);
//}

	// Start side.
	gl.Color3ubv(vtx[0].color);
	gl.TexCoord2f(dyn->s[0], dyn->t[1]);
	gl.MultiTexCoord2f(DGL_TEXTURE1, texcoord[0].s, texcoord[1].t);
	gl.Vertex3f(vtx[0].pos[VX], quad->bottom, vtx[0].pos[VY]);
		
	gl.TexCoord2f(dyn->s[0], dyn->t[0]);
	gl.MultiTexCoord2f(DGL_TEXTURE1, texcoord[0].s, texcoord[0].t);
	gl.Vertex3f(vtx[0].pos[VX], quad->top, vtx[0].pos[VY]);
	
	// End side.
	gl.Color3ubv(vtx[1].color);
	gl.TexCoord2f(dyn->s[1], dyn->t[0]);
	gl.MultiTexCoord2f(DGL_TEXTURE1, texcoord[1].s, texcoord[0].t);
	gl.Vertex3f(vtx[1].pos[VX], quad->top, vtx[1].pos[VY]);
		
	gl.TexCoord2f(dyn->s[1], dyn->t[1]);
	gl.MultiTexCoord2f(DGL_TEXTURE1, texcoord[1].s, texcoord[1].t);
	gl.Vertex3f(vtx[1].pos[VX], quad->bottom, vtx[1].pos[VY]);
}

//===========================================================================
// RL_DrawQuadLights
//	Draw lights for a normal quad (non-div'd).
//===========================================================================
void RL_DrawQuadLights(primhdr_t *prim)
{
	primquad_t *quad = (primquad_t*) ((byte*)prim + sizeof(*prim));
	primvertex2_t *vtx = quad->vertices;
	dynlight_t *dyn;
	DGLuint currentTexture = gl.GetInteger(DGL_TEXTURE_BINDING);
	
	for(dyn = prim->lights; dyn; dyn = dyn->next)
	{
		if(dyn->texture != currentTexture)
		{
			// Must change textures.
			gl.End();
			gl.Bind(currentTexture = dyn->texture);
			gl.Begin(DGL_QUADS);
		}

		// Dynamic light polygons are the same color all over.
		gl.Color3ubv(dyn->color);

		// Start side.
		gl.TexCoord2f(dyn->s[0], dyn->t[1]);
		gl.Vertex3f(vtx[0].pos[VX], quad->bottom, vtx[0].pos[VY]);
			
		gl.TexCoord2f(dyn->s[0], dyn->t[0]);
		gl.Vertex3f(vtx[0].pos[VX], quad->top, vtx[0].pos[VY]);
		
		// End side.
		gl.TexCoord2f(dyn->s[1], dyn->t[0]);
		gl.Vertex3f(vtx[1].pos[VX], quad->top, vtx[1].pos[VY]);
			
		gl.TexCoord2f(dyn->s[1], dyn->t[1]);
		gl.Vertex3f(vtx[1].pos[VX], quad->bottom, vtx[1].pos[VY]);
	}
}

//===========================================================================
// RL_DrawDivQuadLights
//===========================================================================
void RL_DrawDivQuadLights(primhdr_t *prim)
{
	divquaddata_t div;
	primvertex2_t *vtx;
	dynlight_t *dyn;
	int i, side, other;

	RL_GetDivQuad(prim, &div);
	vtx = div.quad->quad.vertices;

	for(dyn = prim->lights; dyn; dyn = dyn->next)
	{
		gl.Bind(dyn->texture);
		gl.Color3ubv(dyn->color);
		
		// A more general algorithm is used for divquads.
		for(side = 0; side < 2; side++)	// Left->right is side zero.
		{
			other = !side;
			
			// We'll render two fans.
			gl.Begin(DGL_TRIANGLE_FAN);
			
			// The origin vertex.
			gl.TexCoord2f(dyn->s[side], dyn->t[side]);
			gl.Vertex3f(vtx[side].pos[VX], div.zpos[side], vtx[side].pos[VY]);
			
			for(i = 0; i <= div.quad->numdivs[other] + 1; i++)
			{
				// The vertex on the opposite side.
				if(!i) 				
				{
					// The top/bottom vertex.
					gl.TexCoord2f(dyn->s[other], dyn->t[side]);
					gl.Vertex3f(vtx[other].pos[VX], div.zpos[side], 
						vtx[other].pos[VY]);
				}
				else if(i == div.quad->numdivs[other] + 1) 
				{
					// The bottom/top vertex.
					gl.TexCoord2f(dyn->s[other], dyn->t[other]);
					gl.Vertex3f(vtx[other].pos[VX], div.zpos[other], 
						vtx[other].pos[VY]);
				}
				else // A division vertex.
				{
					float z = div.heights[other][i - 1];

					// Interpolate the texture coordinate.
					gl.TexCoord2f(dyn->s[other], (z - div.zpos[1]) 
						/ div.height * (dyn->t[0] - dyn->t[1]) + dyn->t[1]);

					gl.Vertex3f(vtx[other].pos[VX], z, vtx[other].pos[VY]);
				}
			}
			
			gl.End();
		}
	}
}

//===========================================================================
// RL_DrawSinglePassFlat
//===========================================================================
void RL_DrawSinglePassFlat(primhdr_t *prim)
{
	primflat_t *flat = (primflat_t*) ((byte*)prim + sizeof(*prim));
	primvertex2_t *vtx;
	dynlight_t *dyn = prim->lights;
	float lightcolor[4];
	int i;

#ifdef _DEBUG
	if(dyn->next)
	{
		Con_Error("RL_DrawSinglePassFlat: More than one light.\n");
	}
#endif

	gl.Bind(dyn->texture);
	RL_FloatRGB(dyn->color, lightcolor);
	gl.SetFloatv(DGL_ENV_COLOR, lightcolor);

	// In a fan all the triangles share the first vertex.
	gl.Begin(DGL_TRIANGLE_FAN);
	for(i = 0, vtx = flat->vertices; i < flat->numvertices; i++, vtx++)
	{
		gl.Color3ubv(vtx->color);

		gl.MultiTexCoord2f(DGL_TEXTURE0,
			(vtx->pos[VX] + dyn->s[0]) * dyn->s[1], 
			(-vtx->pos[VY] + dyn->t[0]) * dyn->t[1]);

		gl.MultiTexCoord2f(DGL_TEXTURE1, 
			(vtx->pos[VX] + prim->texoffx)/rl_texw,
			(-vtx->pos[VY] - prim->texoffy)/rl_texh);

		gl.Vertex3f(vtx->pos[VX], flat->z, vtx->pos[VY]);
	}
	gl.End();
}

//===========================================================================
// RL_DrawFlatLights
//===========================================================================
void RL_DrawFlatLights(primhdr_t *prim)
{
	primflat_t *flat = (primflat_t*) ((byte*)prim + sizeof(*prim));
	primvertex2_t *vtx;
	dynlight_t *dyn;
	int i;

	for(dyn = prim->lights; dyn; dyn = dyn->next)
	{
		gl.Bind(dyn->texture);
		gl.Color3ubv(dyn->color);

		// In a fan all the triangles share the first vertex.
		gl.Begin(DGL_TRIANGLE_FAN);
		for(i = 0, vtx = flat->vertices; i < flat->numvertices; i++, vtx++)
		{
			gl.TexCoord2f((vtx->pos[VX] + dyn->s[0]) * dyn->s[1], 
				(-vtx->pos[VY] + dyn->t[0]) * dyn->t[1]);
			gl.Vertex3f(vtx->pos[VX], flat->z, vtx->pos[VY]);
		}
		gl.End();
	}
}

//===========================================================================
// RL_DrawFlat
//===========================================================================
void RL_DrawFlat(primhdr_t *prim)
{
	primflat_t *flat = (primflat_t*) ((byte*)prim + sizeof(*prim));
	primvertex2_t *vtx = flat->vertices;
	color3_t detail_color[RL_MAX_POLY_SIDES];
//	lumobj_t *lum = prim->light;
	//float dlsize, dloffx, dloffy;
	int i;

	// Dynamic light polygons are the same color all over.
/*	if(prim->flags & RPF_LIGHT) 
	{
		// For light polys the texture offset is global.
		dloffx = FIX2FLT(lum->thing->x) + lum->radius;
		dloffy = FIX2FLT(lum->thing->y) + lum->radius;
		dlsize = lum->radius*2;

		gl.Color3ubv(vtx[0].color);
	}

	// Shadow polygons use only one color, too.
	if(prim->flags & RPF_SHADOW)
	{
		gl.Color3ubv(vtx[0].color);
	}*/

	// Detail texture coordinates and color.
	IFDET RL_DetailColor(detail_color, vtx, flat->numvertices);

	// In a fan all the triangles share the first vertex.
	gl.Begin(DGL_TRIANGLE_FAN);
	for(i = 0; i < flat->numvertices; i++, vtx++)
	{
/*		if(prim->flags & RPF_LIGHT)
		{
			gl.TexCoord2f((dloffx - vtx->pos[VX])/dlsize,
				(dloffy - vtx->pos[VY])/dlsize); 
		}
		else if(prim->flags & RPF_SHADOW)
		{
			gl.TexCoord2f((prim->texoffx - vtx->pos[VX])/prim->shadowradius,
				(prim->texoffy - vtx->pos[VY])/prim->shadowradius);
		}
		else*/
		{
			IFCOL gl.Color3ubv(vtx->color);
			IFTEX gl.TexCoord2f((vtx->pos[VX] + prim->texoffx)/rl_texw,
				(-vtx->pos[VY] - prim->texoffy)/rl_texh);
			IFDET { 
				gl.Color3fv(detail_color[i].rgb);
				gl.TexCoord2f((vtx->pos[VX] + prim->texoffx)/rl_detail->width
					* detailScale * rl_detail->scale, 
					(-vtx->pos[VY] - prim->texoffy)/rl_detail->height
					* detailScale * rl_detail->scale);
			}
		}
		gl.Vertex3f(vtx->pos[VX], flat->z, vtx->pos[VY]);
	}
	gl.End();
}
#endif

#if 0
	byte		*cursor;
	boolean		blank_dlight = false;
	primhdr_t	*prim, *firstdq = 0, *prevdq = 0;
	boolean		isquads;
	boolean		skipdlit = (lid==LID_NORMAL_DLIT);
	boolean		onlydlit = (lid==LID_DLIT_NOTEX 
							|| lid==LID_DLIT_TEXTURED 
							|| lid==LID_LIGHTS);
	boolean		onlydetail = (lid==LID_DETAILS);
	boolean		skipsinglesrc = ((lid==LID_NORMAL_DLIT || lid==LID_LIGHTS 
							|| lid==LID_DLIT_NOTEX) && multiTexLights);
	boolean		onlysinglesrc = false;

	// Is there any point in processing this list?
	if(onlydlit && !(li->has & RLHAS_DLIT)
		|| onlydetail && !(li->has & RLHAS_DETAIL)) return;

	// What data to include in the drawing?
	// These control the operation of the primitive drawing routines.
	switch(lid)
	{
	case LID_SKYMASK:
		with_tex = with_col = with_det = false;
		break;

	case LID_NORMAL:
	case LID_NORMAL_DLIT:
		with_tex = with_col = true;
		with_det = false;
		gl.Bind(li->tex);
		rl_texw = li->texw;
		rl_texh = li->texh;
		break;

/*	case LID_SHADOWS:
		with_tex = with_col = true;
		with_det = false;
		gl.Bind(GL_PrepareLightTexture());
		break;*/

	case LID_DLIT_NOTEX:
		with_col = true;
		with_tex = with_det = false;
		break;

	case LID_DLIT_TEXTURED:
		with_tex = true;
		with_det = with_col = false;
		gl.Bind(li->tex);
		rl_texw = li->texw;
		rl_texh = li->texh;
		break;

/*	case LID_DYNAMIC_LIGHTS:
		gl.Bind(li->tex);
		with_tex = true;
		with_col = with_det = false;
		break;*/

	case LID_LIGHTS:
		// Texture will be changed when necessary.
		with_tex = true;
		with_col = with_det = false;
		break;

	case LID_SINGLE_PASS_LIGHTS:
		// This is only done when multitexturing is supported.
		// Let's set up the environments.
		with_col = with_tex = true;
		with_det = false;
		rl_texw = li->texw;
		rl_texh = li->texh;

		gl.SetInteger(DGL_MODULATE_TEXTURE, li->tex);

		// We are only interesting in polys with a single light source.
		// All polys that we render will be marked RPF_DONE.
		onlysinglesrc = true;
		break;

	case LID_DETAILS:
		// li->detail can't be NULL at this stage.
		gl.Bind(li->detail->tex);
		rl_detail = li->detail;
		with_det = true;		
		with_col = with_tex = false;
		break;
	}

	// Does the list contain flats or quads?
	isquads = (li->type == rl_quads);
		
	// Step #1: Render normal quads or flats. If DLIT primitives are
	// encountered, they are skipped and marked for later rendering.
	if(isquads) gl.Begin(DGL_QUADS);
	for(cursor = li->data;;)
	{
		// Get primitive at cursor, move cursor to the next one.
		prim = (primhdr_t*) cursor;
		if(!prim->size) break;	// This is the last.
		cursor += prim->size;	// Advance cursor to the next primitive.

		// Should we render only the DLIT primitives?
		if(prim->flags & RPF_DONE
			|| onlydlit && !prim->lights
			|| onlydetail && !(prim->flags & RPF_DETAIL))
			continue;

		// Should we skip everything with a single light?
		if(prim->lights && !prim->lights->next)
		{
			if(skipsinglesrc) continue;
		}
		else if(onlysinglesrc) continue;

		// What do we have here?
		if(prim->type == rp_divquad) 
		{
			// There are divquads to render. We'll draw them in Step #2.
			// Let's set up a simple linked list through all divquads.
			prim->ptr = NULL; 
			if(!firstdq) firstdq = prim;
			if(prevdq) prevdq->ptr = prim;
			prevdq = prim;
		}
		if(skipdlit && prim->lights)
		{
			// In dlBlend mode zero the real texture is multiplied on the
			// lit surface during a later stage.
			blank_dlight = true;
			continue;
		}
		if(prim->type == rp_divquad) continue;

		switch(lid)
		{
		case LID_LIGHTS:
			if(isquads)
				RL_DrawQuadLights(prim);
			else
				RL_DrawFlatLights(prim);
			break;

		case LID_SINGLE_PASS_LIGHTS:	
			if(isquads)
				RL_DrawSinglePassQuad(prim);
			else
				RL_DrawSinglePassFlat(prim);
			break;

		default:
			// Draw the primitive.
			if(isquads) 
				RL_DrawQuad(prim);
			else
				RL_DrawFlat(prim); // Flats are triangle fans.
			break;
		}
	}
	if(isquads) gl.End();

	// Step #2: Need to draw some divided walls? They're drawn separately 
	// because they're composed of triangles, not quads.
	if(firstdq)
	{
		for(prim = firstdq; prim; prim = prim->ptr) 
		{
			// Skip primitives that have already been drawn.
			if(prim->flags & RPF_DONE) continue;

			// Should we skip everything with a single light?
			if(prim->lights && !prim->lights->next)
			{
				if(skipsinglesrc) continue;
			}
			else if(onlysinglesrc) continue;

			// Don't draw DLIT divquads.
			if(prim->lights && skipdlit) continue;

			switch(lid)
			{
			case LID_LIGHTS:
				RL_DrawDivQuadLights(prim);
				break;

			case LID_SINGLE_PASS_LIGHTS:
				break;
			
			default:
				RL_DrawDivQuad(prim);
				break;
			}
		}
	}

	// Step #3: Need to draw some dlit polys? They're drawn blank, 
	// with no textures. Dynlights are drawn on them and finally the 
	// textures (multiply-blend).
	if(blank_dlight) // This only happens with LID_NORMAL_DLIT.
	{
		// Start a 2nd pass, of sorts.
		gl.Disable(DGL_TEXTURING);
		RL_DoList(LID_DLIT_NOTEX, li);
		gl.Enable(DGL_TEXTURING);
	}

	switch(lid)
	{
	case LID_SINGLE_PASS_LIGHTS:
		gl.SetInteger(DGL_MODULATE_TEXTURE, 0);
		break;
	}
#endif

//===========================================================================
// RL_SetTexCoordArray
//===========================================================================
void RL_SetTexCoordArray(int unit, primhdr_t *prim, int index)
{
	void *coords[MAX_TEX_UNITS];

	memset(coords, 0, sizeof(coords));
	if(index < maxArrayLights)
	{
		// Coordinates are in the main array.
		coords[unit] = texCoords[TCA_LIGHT1 + index];
	}
	else
	{
		// Coordinates are stored in the rendering list.
		// Technically the pointer we are giving is invalid, but we know
		// that we're only going to be using data in the valid range
		// (the primitive's indices).
		coords[unit] = prim->lightCoords 
			+ (index - maxArrayLights) * prim->primSize
			- prim->indices[0];
	}
	gl.Arrays(NULL, NULL, numTexUnits, coords, 0);
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
// RL_Clear
//===========================================================================
void RL_Clear(void)
{
	numIndices = 0;
}

//===========================================================================
// RL_Primitive
//===========================================================================
void RL_Primitive(primhdr_t *prim)
{
	memcpy(indices + numIndices, prim->indices, 
		sizeof(uint) * prim->numIndices);

	numIndices += prim->numIndices;

#ifdef _DEBUG
	if(numIndices > MAX_INDICES)
	{
		Con_Error("RL_Primitive: Too many indices.\n");
	}
#endif
}

//===========================================================================
// RL_Draw
//===========================================================================
void RL_Draw(void)
{
	BEGIN_PROF( PROF_RL_DRAW_ELEMS );

	if(numIndices) gl.DrawElements(DGL_TRIANGLES, numIndices, indices);

	END_PROF( PROF_RL_DRAW_ELEMS );
}

//===========================================================================
// RL_DrawPrimitives
//	Draws the privitives that match the conditions. If no condition bits
//	are given, all primitives are considered eligible.
//===========================================================================
void RL_DrawPrimitives(int conditions, rendlist_t *list)
{
	primhdr_t *hdr;
	dynlight_t *dyn;
	float color[4];
	int i;

	// Is blending allowed?
	if(conditions & DCF_NO_BLEND && list->intertex) return;

	BEGIN_PROF( PROF_RL_DRAW_PRIMS );

	RL_Clear();

	// Compile our list of indices.
	for(hdr = (primhdr_t*) list->data; hdr->size; 
		hdr = (primhdr_t*) ((byte*) hdr + hdr->size))
	{
		// FIXME: Precalculate the condition flags for all primitives.
		if(!conditions
			|| conditions & DCF_NO_LIGHTS && !hdr->numLights
			|| conditions & DCF_FEW_LIGHTS 
				&& hdr->numLights > 0
				&& hdr->numLights <= maxArrayLights
			|| conditions & DCF_FIRST_UNIT_LIGHTS
				&& hdr->numLights > maxArrayLights
			|| conditions & DCF_EXTRA_UNIT_LIGHTS
				&& hdr->numLights > numTexUnits
			|| conditions & DCF_NO_SINGLE_PASS_TEXTURE
				&& hdr->numLights > maxArrayLights
			|| conditions & DCF_BLEND
				&& list->intertex)
		{
			RL_Primitive(hdr);

			if(conditions & ( DCF_FEW_LIGHTS 
				| DCF_FIRST_UNIT_LIGHTS
				| DCF_EXTRA_UNIT_LIGHTS ))
			{
				if(conditions & 
					( DCF_FIRST_UNIT_LIGHTS | DCF_EXTRA_UNIT_LIGHTS ))
				{
					dyn = hdr->lights;
					if(conditions & DCF_EXTRA_UNIT_LIGHTS)
					{
						// Find the first 'extra' light.
						for(; dyn && dyn->index < (unsigned)numTexUnits; 
							dyn = dyn->next);
					}

					// 'i' keeps track of the texture unit.
					for(i = 0; dyn; i++, dyn = dyn->next)
					{
						if(i == numTexUnits)
						{
							// Out of TUs, time to draw.
							RL_Draw();
							if(conditions & DCF_FIRST_UNIT_LIGHTS)
							{
								// No more lights for this pass.
								RL_Clear();
								break;
							}
							i = 0;
						}
						gl.Enable(DGL_TEXTURE0 + i);
						gl.Bind(dyn->texture);
						RL_FloatRGB(dyn->color, color);
						gl.SetFloatv(DGL_ENV_COLOR, color);
						RL_SetTexCoordArray(i, hdr, dyn->index);
					}
					// Disable the rest of the units.
					for(; i < numTexUnits; i++)
						gl.Disable(DGL_TEXTURE0 + i);
					// Draw the last set.
					RL_Draw();
				}
				else
				{
					// Only draw the lights that fit on this pass.
					// Setup the state for this primitive.
					for(i = 0, dyn = hdr->lights; i < maxArrayLights; 
						i++, dyn = (dyn? dyn->next : NULL))
					{
						if(i < hdr->numLights)
						{
							gl.Enable(DGL_TEXTURE0 + i);
							gl.Bind(dyn->texture);
							RL_FloatRGB(dyn->color, color);
							gl.SetFloatv(DGL_ENV_COLOR, color);
						}
						else
						{
							// No light for this unit.
							gl.Disable(DGL_TEXTURE0 + i);
						}
					}
					RL_Draw();
				}
				RL_Clear();
			}
		}
	}

	RL_Draw();	

	END_PROF( PROF_RL_DRAW_PRIMS );
}

//===========================================================================
// RL_DoList
//	This gets called repeatedly; only change GL state when you have to.
//===========================================================================
void RL_DoList(listmode_t mode, rendlist_t *list)
{
	// What kind of primitives should we draw?
	switch(mode)
	{
	case LM_NO_LIGHTS:
		RL_DrawPrimitives(DCF_NO_LIGHTS | DCF_NO_BLEND, list);	
		break;

	case LM_FEW_LIGHTS:
		RL_DrawPrimitives(DCF_FEW_LIGHTS | DCF_NO_BLEND, list);
		break;

	case LM_MANY_LIGHTS:
		//RL_DrawPrimitives(DCF_FIRST_UNIT_LIGHTS | DCF_BLEND, list);
		break;

	case LM_EXTRA_LIGHTS:
		//RL_DrawPrimitives(DCF_EXTRA_UNIT_LIGHTS, list);
		break;

	case LM_NEEDS_TEXTURE:
		//RL_DrawPrimitives(DCF_NO_SINGLE_PASS_TEXTURE | DCF_BLEND, list);
		break;

	default:
		// Unknown mode, do nothing.
		return;
	}
}

//===========================================================================
// RL_SetupState
//	The GL state includes setting up the correct vertex arrays.
//===========================================================================
void RL_SetupState(listmode_t mode, rendlist_t *list)
{
	int i;
	void *coords[MAX_TEX_UNITS];
	float color[4];

	BEGIN_PROF( PROF_RL_SETUP_STATE );

	switch(mode)
	{
	case LM_MUL_SINGLE_PASS:
		// Surfaces that can be lit multiplicatively in a single pass.
		// This state is used for the first pass.
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);

		gl.Disable(DGL_ALPHA_TEST);

		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA); 

		// Use the global vertex arrays.
		for(i = 0; i < numTexUnits - 1; i++)
		{
			coords[i] = texCoords[TCA_LIGHT1 + i];
		}
		coords[numTexUnits - 1] = texCoords[TCA_MAIN];
		// We're only locking the vertex and color arrays.
		gl.DisableArrays(false, false, 0xf);
		gl.Arrays(vertices, colors, 0, NULL, numVertices);
		// The texture coordinates array may change, so we won't lock it.
		gl.Arrays(NULL, NULL, numTexUnits, coords, 0);
		break;

	case LMS_LAST_UNIT_TEXTURE_ONLY:
		// Disable all texture units except the last one.
		for(i = 0; i < numTexUnits - 1; i++)
		{
			gl.Disable(DGL_TEXTURE0 + i);
		}
		gl.Enable(DGL_TEXTURE0 + numTexUnits - 1);
		break;

	case LMS_LIGHTS_TEXTURE_MUL:
		// Setup the add-multiply texture environment.
		gl.Enable(DGL_MODULATE_TEXTURE);
		break;

	case LMS_Z_DONE:
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		break;

	/*case LMS_LIGHTS_ADD:
		// All texture stages will be used for rendering dynamic lights.
		// Result is alpha tested and additively blended.
		gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 0);
		break;*/

	case LMS_LIGHTS_ADD_FIRST:
		// First pass of the LIGHTS_ADD.
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		gl.Disable(DGL_ALPHA_TEST);
		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);

		gl.Enable(DGL_ADD_LIGHTS);
		
		// Setup the environment for the first stage. We need to add to the
		// primary color.
		gl.SetInteger(DGL_ADD_LIGHTS, 1);
		break;

	case LMS_LIGHTS_ADD_SUBSEQUENT:
		// Subsequent passes of the LIGHTS_ADD.
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		gl.Enable(DGL_ALPHA_TEST);

		gl.Func(DGL_BLENDING, DGL_ONE, DGL_ONE);

		// Setup the environment for the first stage. We must ignore the 
		// primary color; additive blending is used to apply the final result
		// to the framebuffer.
		gl.SetInteger(DGL_ADD_LIGHTS, 2);
		break;

	case LMS_TEXTURE_MUL:
		if(list == NULL)
		{
			// Setup GL state for texture modulation.
			gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
			gl.SetInteger(DGL_MODULATE_TEXTURE, 2);

			// Use the appropriate texture coord arrays.
			coords[0] = texCoords[TCA_MAIN];
			coords[1] = texCoords[TCA_BLEND];
			gl.Arrays(NULL, NULL, 2, coords, 0);
		}
		else
		{
			if(list->intertex)
			{
				// Disable all TUs except the first two.
				for(i = 2; i < numTexUnits; i++)
					gl.Disable(DGL_TEXTURE0 + i);
				gl.Enable(DGL_TEXTURE1);
				gl.Bind(list->intertex);
				color[0] = color[1] = color[2] = 0;
				color[3] = list->interpos;
				gl.SetFloatv(DGL_ENV_COLOR, color);

				// Enable the appropriate texture coord arrays.
				gl.EnableArrays(0, 0, 0x1 | 0x2);
			}
			else
			{
				// Disable all TUs except the first one.
				for(i = 1; i < numTexUnits; i++)
					gl.Disable(DGL_TEXTURE0 + i);

				// Enable the appropriate texture coord arrays.
				gl.EnableArrays(0, 0, 0x1);
			}
			gl.Enable(DGL_TEXTURE0);
			gl.Bind(list->tex);
		}
		break;

	case LMS_SINGLE_TEXTURE:
		// Disable all texture units except the first one.
		for(i = 1; i < numTexUnits; i++)
		{
			gl.Disable(DGL_TEXTURE0 + i);
		}
		gl.Disable(DGL_MODULATE_TEXTURE);
		gl.Enable(DGL_TEXTURE0);		
		break;
	}

	END_PROF( PROF_RL_SETUP_STATE );
}

//===========================================================================
// RL_CleanupState
//===========================================================================
void RL_CleanupState(listmode_t mode)
{
	switch(mode)
	{
	case LM_MUL_SINGLE_PASS:
		gl.UnlockArrays();
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		gl.Enable(DGL_ALPHA_TEST);
		gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 0);
		gl.Enable(DGL_BLENDING);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		break;
	}
}

//===========================================================================
// RL_DoLists
//	Major GL state changes are done here.
//===========================================================================
void RL_DoLists(listmode_t mode, rendlist_t **lists, int num)
{
	int i;

	for(i = 0; i < num; i++)
	{
		// Is there something in this list?
		if(lists[i]->last) 
		{
			// Per-list GL state?
			switch(mode)
			{
			case LM_NO_LIGHTS:
			case LM_FEW_LIGHTS:
				gl.SetInteger(DGL_ACTIVE_TEXTURE, numTexUnits - 1);
				gl.Bind(lists[i]->tex);
				break;

			case LM_NEEDS_TEXTURE:
				RL_SetupState(LMS_TEXTURE_MUL, lists[i]);
				break;
			}

			RL_DoList(mode, lists[i]);
		}
	}
}

//===========================================================================
// RL_RenderLists
//	Renders the given lists. RL_DoList does the actual work, we just set
//	up and restore the DGL state here.
//===========================================================================
void RL_RenderLists(listmode_t mode, rendlist_t **lists, int num)
{
//	int i;

	// If there are just a few empty lists, no point in setting and 
	// restoring the state.
	/*if(num <= 3) // Covers dynlights and skymask.
	{
		for(i = 0; i < num; i++) if(lists[i]->last) break;
		if(i == num) return; // Nothing to do!
	}*/
	
	if(mode == LM_NORMAL_MUL)
	{
		// First all the primitives that can be completed in a single pass.
		RL_SetupState(LM_MUL_SINGLE_PASS, 0);

		// Surfaces without any lights.
		RL_SetupState(LMS_LAST_UNIT_TEXTURE_ONLY, 0);
		RL_DoLists(LM_NO_LIGHTS, lists, num);

		// Then, surfaces with Few Lights. 
		RL_SetupState(LMS_LIGHTS_TEXTURE_MUL, 0);
		RL_DoLists(LM_FEW_LIGHTS, lists, num);

		// Lastly, surfaces with Many Lights. We'll draw as many lights as
		// there are texture units.
		RL_SetupState(LMS_LIGHTS_ADD_FIRST, 0);
		RL_DoLists(LM_MANY_LIGHTS, lists, num);

		// Now we have covered all the surfaces in the scene. 
		// Draw additional lights where necessary.
		RL_SetupState(LMS_LIGHTS_ADD_SUBSEQUENT, 0);
		RL_DoLists(LM_EXTRA_LIGHTS, lists, num);

		// We can disable Z-writing because all the surfaces have now been
		// written into the Z-buffer.
		RL_SetupState(LMS_Z_DONE, 0);

		// Draw low-poly lights and shadows.

		// Apply texture to surfaces with Many Lights. Texblending is done 
		// during this step, except in the case of surfaces with no lights
		// (blending was already done during the first pass).
		RL_SetupState(LMS_TEXTURE_MUL, 0);
		RL_DoLists(LM_NEEDS_TEXTURE, lists, num);

		RL_SetupState(LMS_SINGLE_TEXTURE, 0);

		//RL_DoLists(LM_MANY_LIGHTS_WITH_Z, lists, num);

		RL_CleanupState(LM_MUL_SINGLE_PASS);
	}

#if 0
	// Setup the state.
	switch(lid)
	{
	case LID_SKYMASK:
		gl.Disable(DGL_TEXTURING);
		// This will effectively disable color buffer writes.
		gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE);
		break;

	case LID_NORMAL:
	case LID_NORMAL_DLIT:
#if 0
		gl.ZBias(NORMALBIAS);
#endif
		// Disable alpha blending; some video cards think alpha zero is
		// is still translucent. And I guess things should render faster
		// with no blending...
		gl.Disable(DGL_BLENDING);
		break;

/*	case LID_SHADOWS:
#if 0
		gl.ZBias(SHADOWBIAS);
#endif
		if(whitefog) gl.Disable(DGL_FOG);
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE_MINUS_SRC_COLOR);
		break;*/

	case LID_DLIT_TEXTURED:
		return;
#if 0
		gl.ZBias(DLITBIAS);
#endif
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);	
		// Multiply src and dest colors.
		gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
		gl.Color3f(1, 1, 1);
		break;

/*	case LID_DYNAMIC_LIGHTS:
#if 0
		gl.ZBias(DYNLIGHTBIAS);
#endif
		// Disable fog.
		if(whitefog) gl.Disable(DGL_FOG);
		// This'll allow multiple light quads to be rendered on top of 
		// each other.
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		// The source is added to the destination.
		gl.Func(DGL_BLENDING, DGL_ONE, DGL_ONE); 
		break;*/

	case LID_DETAILS:
		if(!gl.Enable(DGL_DETAIL_TEXTURE_MODE))
			return; // Can't render detail textures...
#if 0
		gl.ZBias(DETAILBIAS);
#endif
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		break;
	}

	// Render each of the provided lists.
	RL_DoLists(lid, lists, num);

	// How about some dynamic lights?
	if(lid == LID_NORMAL_DLIT)
	{
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);

		gl.Enable(DGL_ALPHA_TEST);
		gl.Func(DGL_ALPHA_TEST, DGL_GREATER, 0);

		// The (source * alpha) is added to the destination.
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ZERO /*DGL_ONE*/); 
		gl.Enable(DGL_BLENDING);

		// The dynamic lights pass.
		RL_DoLists(LID_LIGHTS, lists, num);

		// The single-pass, one light, combined light+texture pass.
		if(multiTexLights)
		{
			gl.Enable(DGL_DEPTH_WRITE);
			gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
			gl.Enable(DGL_TEXTURE1);
						
			RL_DoLists(LID_SINGLE_PASS_LIGHTS, lists, num);

			gl.Disable(DGL_TEXTURE1);
			gl.Enable(DGL_TEXTURE0);
		}

		// The texture modulation pass.
	}

	// Restore state.
	switch(lid)
	{
	case LID_SKYMASK:
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		gl.Enable(DGL_TEXTURING);
		break;

	case LID_NORMAL:
	case LID_NORMAL_DLIT:
#if 0
		gl.ZBias(0);
#endif
		gl.Enable(DGL_BLENDING);
		gl.Disable(DGL_ALPHA_TEST);

		gl.Enable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA); 		

		break;

/*	case LID_SHADOWS:
#if 0 
		gl.ZBias(0);
#endif
		gl.Enable(DGL_DEPTH_WRITE);
		if(whitefog) gl.Enable(DGL_FOG);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		break;*/

	case LID_DLIT_TEXTURED:
#if 0
		gl.ZBias(0);
#endif
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);	
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		break;

/*	case LID_DYNAMIC_LIGHTS:
#if 0
		gl.ZBias(0);
#endif
		if(whitefog) gl.Enable(DGL_FOG);
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA); 
		break;*/

	case LID_DETAILS:
#if 0
		gl.ZBias(0);
#endif
		gl.Disable(DGL_DETAIL_TEXTURE_MODE);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		break;
	}

#endif
}

//===========================================================================
// RL_RenderAllLists
//===========================================================================
void RL_RenderAllLists(void)
{
	// Multiplicative lights?
	//boolean muldyn = !dlBlend && !whitefog; 

	// Pointers to all the rendering lists.
	rendlist_t *it, *rlists[MAX_RLISTS];
	int i, numrlists = 0;
	
	BEGIN_PROF( PROF_RL_RENDER_ALL );

	// Collect a list of rendering lists.
	for(i = 0; i < RL_HASH_SIZE; i++)
	{
		for(it = list_hash[i].first; it; it = it->next)
		{
			if(it->last != NULL)
			{
				rlists[ numrlists++ ] = it;
			}
		}
	}

/*	if(!renderTextures) gl.Disable(DGL_TEXTURING);*/

	// The sky might be visible. Render the needed hemispheres.
	Rend_RenderSky(skyhemispheres);				

#if 0
	// Mask the sky in the Z-buffer.
	RL_RenderLists(LID_SKYMASK, ptr_mask_rlists, NUM_RLSKY);
#endif

/*	if(!renderTextures) gl.Disable(DGL_TEXTURING);*/

	BEGIN_PROF( PROF_RL_RENDER_NORMAL );

	// Render the real surfaces of the visible world.
	RL_RenderLists(IS_MUL_MODE? LM_NORMAL_MUL : LM_NORMAL_ADD,
		rlists, numrlists);

	END_PROF( PROF_RL_RENDER_NORMAL );

	// Render object shadows.
//	RL_RenderLists(LID_SHADOWS, ptr_shadow_rlist, 1);

/*	BEGIN_PROF( PROF_RL_RENDER_LIGHT );

	// Render dynamic lights.
	if(dlBlend != 3)
		RL_RenderLists(LID_DYNAMIC_LIGHTS, ptr_dl_rlists, NUM_RLDYN);

	// Apply the dlit pass?
	if(muldyn) RL_RenderLists(LID_DLIT_TEXTURED, rlists, numrlists);

	END_PROF( PROF_RL_RENDER_LIGHT );*/

#if 0
	// Render the detail texture pass?
	if(r_detail) RL_RenderLists(LID_DETAILS, rlists, numrlists);
#endif

	BEGIN_PROF( PROF_RL_RENDER_MASKED );

	// Draw masked walls, sprites and models.
	Rend_DrawMasked();

	// Draw particles.
	PG_Render();

	END_PROF( PROF_RL_RENDER_MASKED );

	END_PROF( PROF_RL_RENDER_ALL );

/*	if(!renderTextures) gl.Enable(DGL_TEXTURING);*/
}

