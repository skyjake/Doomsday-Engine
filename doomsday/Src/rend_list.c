
//**************************************************************************
//**
//** REND_LIST.C
//**
//** Doomsday Rendering Lists v2.0
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
	PROF_RL_RENDER_LIGHT,
	PROF_RL_RENDER_MASKED
END_PROF_TIMERS()

#define RL_HASH_SIZE		128

// FIXME: Rlist allocation could be dynamic.
#define MAX_RLISTS			1024 
#define MAX_POLY_SIZE		((int)(4 + sizeof(primhdr_t) \
							+ sizeof(primflat_t) \
							+ sizeof(primvertex2_t)*(RL_MAX_POLY_SIDES-1)))
#define REALLOC_ADDITION	(MAX_POLY_SIZE * 10) // bytes

#define BYTESIZE		1
#define SHORTSIZE		2
#define LONGSIZE		4
#define FLOATSIZE		4

#define NORMALBIAS		2 //1
#define SHADOWBIAS		2 //1
#define DYNLIGHTBIAS	0
#define DLITBIAS		0
#define DETAILBIAS		0

#define IFCOL	if(with_col)
#define IFDET	if(with_det)
#define IFTEX	if(with_tex)
#define IFTC	if(with_tex || with_det)

// List identifiers.
enum 
{
	LID_SKYMASK,				// Draw only into Z-buffer.
	LID_NORMAL,					// Normal walls and planes (dlBlend=1, fog).
	LID_NORMAL_DLIT,			// Normal, DLIT with no textures.
	LID_DLIT_NOTEX,				// Just DLIT with no textures (automatical).
	LID_DLIT_TEXTURED,			// DLIT with multiplicative blending.
	LID_DYNAMIC_LIGHTS,
	LID_DETAILS,
	LID_SHADOWS
};

// Lists for skymask.
enum
{
	RLSKY_FLATS,
	RLSKY_WALLS,
	NUM_RLSKY
};

// Lists for dynamic lights.
enum
{
	RLDYN_FLATS,
	RLDYN_WALLS,
	RLDYN_GLOW,
	NUM_RLDYN
};

// TYPES -------------------------------------------------------------------

// Primitives are the actual datablocks written to rendering lists.
// Each primhdr begins a block of polygon data that ends up as one or
// more triangles on the screen. This data is internal to the engine
// and can be modified at will. The rendering DLL only receives 
// requests to render sets of primitives from a set of vertices.
typedef struct primhdr_s
{
	// RL_AddPoly expects that size is the first thing in the header.
	// Must be an offset since the list is sometimes reallocated.
	int		size;				// Size of this primitive (zero = n/a).
	void	*ptr;
	
	// Generic data common to all polys.
	short	flags;				// RPF_*.
	char	type;	
	float	texoffx, texoffy;	// Texture coordinates (not normalized).
	union primhdr_data_u {
		lumobj_t	*light;			// For RPF_LIGHT polygons.
		int			shadowradius;	// For RPF_SHADOW polygons.
	};
}
primhdr_t;

// The primitive data follows immediately after the header.

// Primvertices are used by the datablocks to store vertex data.
// 2D vertex.
typedef struct primvertex2_s
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
color3_t;

// Data for a quad (wall segment).
typedef struct primquad_s
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
primdivquad_t;

// Data for a flat (planes).
typedef struct primflat_s
{
	float	z;					// Z height.
	int		numvertices;		// Number of vertices for the poly.		
	primvertex2_t vertices[1];	// Really [numvertices].
}
primflat_t;

// Rendering List 'has' flags.
#define RLHAS_DLIT		0x1		// Primitives with RPF_DLIT.
#define RLHAS_DETAIL	0x2		// ...with RPF_DETAIL.

// The rendering list.
typedef struct rendlist_s
{
	struct rendlist_s *next;
	rendlisttype_t type;		// Quads or flats?
	DGLuint	tex;				// The name of the texture for this list.
	int		texw, texh;			// Width and height of the texture.
	detailinfo_t *detail;		// Detail texture name and dimensions.
	int		size;				// Number of bytes allocated for the data.
	byte	*data;				// Data for a number of polygons (The List).
	byte	*cursor;			// A pointer to data, for reading/writing.
	primhdr_t *last;			// Pointer to the last primitive (or NULL).
	int		has;				
}
rendlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

vissprite_t *R_NewVisSprite(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int			skyhemispheres;
extern int			useDynLights, dlBlend, simpleSky;
extern boolean		whitefog;
extern float		maxLightDist;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//int					numrlists = 0;			// Number of rendering lists.
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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

//static rendlist_t rlists[MAX_RLISTS]; // The list of rendering lists.
static listhash_t list_hash[RL_HASH_SIZE];

static rendlist_t mask_rlists[NUM_RLSKY];
static rendlist_t dl_rlists[NUM_RLDYN];
static rendlist_t shadow_rlist;

static rendlist_t *ptr_mask_rlists[NUM_RLSKY];
static rendlist_t *ptr_dl_rlists[NUM_RLDYN];
static rendlist_t *ptr_shadow_rlist[1] = { &shadow_rlist };

// The rendering state for RL_Draw(Div)Quad and RL_DrawFlat.
static int with_tex, with_col, with_det;
static float rl_texw, rl_texh;
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
// RL_DynLightPoly
//	This is only called for floors/ceilings w/dynlights in use.
//===========================================================================
/*void RL_DynLightPoly(rendpoly_t *poly, lumobj_t *lum)
{
	poly->texw = lum->radius*2;
	poly->texh = lum->radius*2;

	// The texture offset is what actually determines where the
	// dynamic light map will be rendered. For light polys the
	// texture offset is global.
	poly->texoffx = FIX2FLT(lum->thing->x) + lum->radius;
	poly->texoffy = FIX2FLT(lum->thing->y) + lum->radius;
}*/

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
	poly->type = rp_flat;	

	if(!(poly->flags & RPF_LIGHT)) // Normal polys.
	{
		// Calculate the distance to each vertex.
		for(i=0; i<numvrts; i++) distances[i] = Rend_PointDist2D(&vrts[i].x);
	}
	/*else
	{
		// This is a light polygon.
		RL_DynLightPoly(poly, (lumobj_t*) poly->tex);
	}*/

	// Copy the vertices to the poly.
	if(!(poly->flags & RPF_LIGHT) && subsector 
		&& subsector->flags & DDSUBF_MIDPOINT)
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
	if(!(poly->flags & RPF_LIGHT)) 
	{
		RL_VertexColors(poly, lightlevel, subsector->sector->rgb);
	}
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
	for(i = 0; i < NUM_RLDYN; i++)
	{
		ptr_dl_rlists[i] = &dl_rlists[i];
	}

	//numrlists = 0;
	//memset(rlists, 0, sizeof(rlists));
	memset(list_hash, 0, sizeof(list_hash));
	memset(mask_rlists, 0, sizeof(mask_rlists));
	memset(dl_rlists, 0, sizeof(dl_rlists));
	memset(&shadow_rlist, 0, sizeof(shadow_rlist));
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
	for(i = 0; i < NUM_RLDYN; i++) RL_DestroyList(&dl_rlists[i]);
	RL_DestroyList(&shadow_rlist);
	//numrlists = 0;

	PRINT_PROF( PROF_RL_ADD_POLY );
	PRINT_PROF( PROF_RL_GET_LIST );
	PRINT_PROF( PROF_RL_RENDER_ALL );
	PRINT_PROF( PROF_RL_RENDER_NORMAL );
	PRINT_PROF( PROF_RL_RENDER_LIGHT );
	PRINT_PROF( PROF_RL_RENDER_MASKED );
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
}

//===========================================================================
// RL_ClearLists
//	Called before rendering a frame.
//===========================================================================
void RL_ClearLists(void)
{
	int	i;
	rendlist_t *list;

	//for(i = 0; i < numrlists; i++) RL_RewindList(&rlists[i]);
	for(i = 0; i < RL_HASH_SIZE; i++)
	{
		for(list = list_hash[i].first; list; list = list->next)
			RL_RewindList(list);
	}
	for(i = 0; i < NUM_RLSKY; i++) RL_RewindList(&mask_rlists[i]);
	for(i = 0; i < NUM_RLDYN; i++) RL_RewindList(&dl_rlists[i]);
	RL_RewindList(&shadow_rlist);

	// Types for the 'static' lists.
	mask_rlists[RLSKY_WALLS].type = rl_quads;
	mask_rlists[RLSKY_FLATS].type = rl_flats;
	dl_rlists[RLDYN_WALLS].type = dl_rlists[RLDYN_GLOW].type = rl_quads;
	dl_rlists[RLDYN_FLATS].type = rl_flats;
	shadow_rlist.type = rl_flats;

	// Dynamic light textures.
	dl_rlists[RLDYN_WALLS].tex = dltexname;
	dl_rlists[RLDYN_FLATS].tex = dltexname;
	dl_rlists[RLDYN_GLOW].tex = glowtexname;

	// FIXME: Does this belong here?
	skyhemispheres = 0;
}

//===========================================================================
// RL_GetListFor
//===========================================================================
rendlist_t *RL_GetListFor(rendpoly_t *poly)
{
	int type;
	listhash_t *hash;
	rendlist_t *dest;
	
	// Check for specialized rendering lists first.
	if(poly->flags & RPF_SHADOW)
	{
		return &shadow_rlist;
	}
	if(poly->flags & RPF_SKY_MASK)
	{
		return &mask_rlists[poly->type==rp_quad? RLSKY_WALLS : RLSKY_FLATS];
	}
	if(poly->flags & RPF_WALL_GLOW)
	{
		return &dl_rlists[RLDYN_GLOW];
	}
	if(poly->flags & RPF_LIGHT) // Dynamic lights?
	{
		return &dl_rlists[poly->type==rp_quad? RLDYN_WALLS : RLDYN_FLATS];
	}

	// Find a suitable normal list. 
	type = (poly->type == rp_flat? rl_flats : rl_quads);

	hash = &list_hash[ (unsigned) (2*poly->tex + type) % RL_HASH_SIZE ];
	for(dest = hash->first; dest; dest = dest->next)
	{
		if(dest->tex == poly->tex && dest->type == type)
		{
			// This is it.
			return dest;
		}
	}

/*#ifdef _DEBUG
	Con_Printf("New list for %i (%i)\n", poly->tex, type);
#endif*/

	// Create a new list.
	dest = Z_Calloc(sizeof(rendlist_t), PU_STATIC, 0);
	if(hash->last) hash->last->next = dest;
	hash->last = dest;
	if(!hash->first) hash->first = dest;

	// Init the info.
	dest->type = type;
	dest->tex = poly->tex;
	dest->texw = poly->texw;
	dest->texh = poly->texh;
	dest->detail = poly->detail;

	return dest;
}

//===========================================================================
// RL_AddPoly
//	Adds the given poly onto the correct list.
//===========================================================================
void RL_AddPoly(rendpoly_t *poly)
{
	rendlist_t		*li;
	int				i, k, d;
	float			mindist;
	rendpoly_vertex_t *vtx;
	primhdr_t		*hdr;
	primvertex2_t	*pv2;
	primquad_t		*pq;
	primdivquad_t	*pdq;
	primflat_t		*pf;
	
	if(poly->flags & RPF_MASKED)
	{
		// Masked polys (walls) get a special treatment (=> vissprite).
		RL_AddMaskedPoly(poly);
		return;
	}

	BEGIN_PROF( PROF_RL_ADD_POLY );
	BEGIN_PROF( PROF_RL_GET_LIST );

	// Find/create a rendering list for the polygon.
	li = RL_GetListFor(poly);

	END_PROF( PROF_RL_GET_LIST );

	// Calculate the distance to each vertex.
	if(!(poly->flags & (RPF_WALL_GLOW | RPF_SKY_MASK | RPF_LIGHT | RPF_SHADOW)))
	{
		// This is a "normal" poly. 
		for(i = 0; i < poly->numvertices; i++)
		{
			// See if we need to calculate distances.
			if(poly->type != rp_flat)
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
	}

	// First check that the data buffer of the list is large enough.
	if(li->cursor - li->data > li->size - MAX_POLY_SIZE)
	{
		// Allocate more memory for the data buffer.
		int cursor_offset = li->cursor - li->data;
		int last_offset = (byte*) li->last - li->data;
		li->size += REALLOC_ADDITION;
		li->data = Z_Realloc(li->data, li->size, PU_STATIC);
		li->cursor = li->data + cursor_offset;
		li->last = (primhdr_t*) (li->data + last_offset);
	}

	// This becomes the new last primitive.
	li->last = hdr = (primhdr_t*) li->cursor;
	li->cursor += sizeof(*hdr);
	hdr->ptr = NULL;
	hdr->flags = poly->flags;
	hdr->type = poly->type;
	hdr->texoffx = poly->texoffx;
	hdr->texoffy = poly->texoffy;
	// The light data is needed for calculating texture coordinates 
	// for RPF_LIGHT polygons.
	hdr->light = poly->light;	

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
	
	// The primitive has been written, update the size.
	hdr->size = li->cursor - (byte*)hdr;

	// Write the end marker (which will be overwritten by the next
	// primitive). The idea is that this zero is interpreted as the
	// size of the following primhdr.
	*(int*) li->cursor = 0;

	if(poly->flags & RPF_DLIT) li->has |= RLHAS_DLIT;

	END_PROF( PROF_RL_ADD_POLY );
}

//===========================================================================
// RL_QuadTexCoords
//===========================================================================
void RL_QuadTexCoords(primhdr_t *prim, primquad_t *quad, texcoord_t *tex)
{
	if(prim->flags & RPF_LIGHT)
	{
		// Wallglow needs different texture coordinates.
		if(prim->flags & RPF_WALL_GLOW)
		{
			// The glow texture is uniform along the S axis.
			tex[0].s = 0;
			tex[1].s = 1;
			// texoffx contains the glow height.
			if(prim->texoffx > 0)
			{
				tex[0].t = prim->texoffy/prim->texoffx;
				tex[1].t = tex[0].t + (quad->top - quad->bottom)/prim->texoffx;
			}
			else
			{
				tex[1].t = -prim->texoffy/prim->texoffx;
				tex[0].t = tex[1].t - (quad->top - quad->bottom)/prim->texoffx;
			}
		}
		else // A regular dynamic light.
		{
			float dlsize = prim->light->radius * 2;
			tex[0].s = -prim->texoffx/dlsize;
			tex[0].t = prim->texoffy/(dlsize/DYN_ASPECT);
			tex[1].s = tex[0].s + quad->length/dlsize;
			tex[1].t = tex[0].t + (quad->top - quad->bottom)/(dlsize/DYN_ASPECT);
		}
	}
	else
	{
		tex[0].s = prim->texoffx/rl_texw;
		tex[0].t = prim->texoffy/rl_texh;
		tex[1].s = tex[0].s + quad->length/rl_texw;
		tex[1].t = tex[0].t + (quad->top - quad->bottom)/rl_texh;
	}
}

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
	if(prim->flags & RPF_LIGHT) gl.Color3ubv(vtx[0].color);

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
// RL_DrawDivQuad
//	DivQuads are rendered as two triangle fans.
//===========================================================================
void RL_DrawDivQuad(primhdr_t *prim)
{
	primdivquad_t *divquad = (primdivquad_t*) ((byte*)prim + sizeof(*prim));
	primvertex2_t *vtx = divquad->quad.vertices;
	float zpos[2] = { divquad->quad.top, divquad->quad.bottom }, z;
	float *divpos[2];
	color3_t detail_color[2];
	texcoord_t texcoord[2];
	int i, side, other;

	divpos[0] = divquad->divs;
	divpos[1] = divquad->divs + divquad->numdivs[0];

	// Calculate texture coordinates.
	IFTEX	RL_QuadTexCoords(prim, &divquad->quad, texcoord);
	IFDET { 
		RL_QuadDetailTexCoords(prim, &divquad->quad, texcoord);
		RL_DetailColor(detail_color, vtx, 2);
	}

	// Dynamic light polygons are the same color all over.
	if(prim->flags & RPF_LIGHT) gl.Color3ubv(vtx[0].color);

	// A more general algorithm is used for divquads.
	for(side = 0; side < 2; side++)	// Left->right is side zero.
	{
		other = !side;

		// We'll render two fans.
		gl.Begin(DGL_TRIANGLE_FAN);

		// The origin vertex.
		IFCOL	gl.Color3ubv(vtx[side].color);
		IFTC	gl.TexCoord2f(texcoord[side].s, texcoord[side].t);
		IFDET	gl.Color3fv(detail_color[side].rgb);
				gl.Vertex3f(vtx[side].pos[VX], zpos[side], vtx[side].pos[VY]);

		for(i = 0; i <= divquad->numdivs[other] + 1; i++)
		{
			// The vertex on the opposite side.
			IFCOL	gl.Color3ubv(vtx[other].color);
			IFDET	gl.Color3fv(detail_color[other].rgb);
			if(!i) // The top/bottom vertex.
			{
				IFTC gl.TexCoord2f(texcoord[other].s, texcoord[side].t);
				gl.Vertex3f(vtx[other].pos[VX], zpos[side], vtx[other].pos[VY]);
			}
			else if(i == divquad->numdivs[other] + 1) // The bottom/top vertex.
			{
				IFTC gl.TexCoord2f(texcoord[other].s, texcoord[other].t);
				gl.Vertex3f(vtx[other].pos[VX], zpos[other], vtx[other].pos[VY]);
			}
			else // A division vertex.
			{
				z = divpos[other][i - 1];
				IFTC { 
					// Calculate the texture coordinate.
					gl.TexCoord2f(texcoord[other].s, (z - divquad->quad.bottom) / 
						(divquad->quad.top - divquad->quad.bottom)
						* (texcoord[0].t - texcoord[1].t) + texcoord[1].t);
				}					
				gl.Vertex3f(vtx[other].pos[VX], z, vtx[other].pos[VY]);
			}
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
	lumobj_t *lum = prim->light;
	float dlsize, dloffx, dloffy;
	int i;

	// Dynamic light polygons are the same color all over.
	if(prim->flags & RPF_LIGHT) 
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
	}

	// Detail texture coordinates and color.
	IFDET RL_DetailColor(detail_color, vtx, flat->numvertices);

	// In a fan all the triangles share the first vertex.
	gl.Begin(DGL_TRIANGLE_FAN);
	for(i = 0; i < flat->numvertices; i++, vtx++)
	{
		if(prim->flags & RPF_LIGHT)
		{
			gl.TexCoord2f((dloffx - vtx->pos[VX])/dlsize,
				(dloffy - vtx->pos[VY])/dlsize); 
		}
		else if(prim->flags & RPF_SHADOW)
		{
			gl.TexCoord2f((prim->texoffx - vtx->pos[VX])/prim->shadowradius,
				(prim->texoffy - vtx->pos[VY])/prim->shadowradius);
		}
		else
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

//===========================================================================
// RL_DoList
//	This is the worker routine, a helper for RL_RenderList (who does all
//	the thinking and sets up and restores the DGL state).
//===========================================================================
void RL_DoList(int lid, rendlist_t *li)
{
	byte		*cursor;
	boolean		blank_dlight = false;
	primhdr_t	*prim, *firstdq = 0, *prevdq = 0;
	boolean		isquads;
	boolean		skipdlit = (lid==LID_NORMAL_DLIT);
	boolean		onlydlit = (lid==LID_DLIT_NOTEX || lid==LID_DLIT_TEXTURED);
	boolean		onlydetail = (lid==LID_DETAILS);

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

	case LID_SHADOWS:
		with_tex = with_col = true;
		with_det = false;
		gl.Bind(GL_PrepareLightTexture());
		break;

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

	case LID_DYNAMIC_LIGHTS:
		gl.Bind(li->tex);
		with_tex = true;
		with_col = with_det = false;
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
		if(onlydlit && !(prim->flags & RPF_DLIT)
			|| onlydetail && !(prim->flags & RPF_DETAIL))
			continue;

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
		if(skipdlit && prim->flags & RPF_DLIT)
		{
			// In dlBlend mode zero the real texture is multiplied on the
			// lit surface during a later stage.
			blank_dlight = true;
			continue;
		}
		if(prim->type == rp_divquad) continue;

		// Draw the primitive.
		if(isquads) 
			RL_DrawQuad(prim);
		else
			RL_DrawFlat(prim); // Flats are triangle fans.
	}
	if(isquads) gl.End();

	// Step #2: Need to draw some divided walls? They're drawn separately 
	// because they're composed of triangles, not quads.
	if(firstdq)
	{
		//gl.Begin(DGL_TRIANGLES);
		for(prim = firstdq; prim; prim = prim->ptr) 
		{
			// Don't draw DLIT divquads.
			if(prim->flags & RPF_DLIT && skipdlit) continue;
			RL_DrawDivQuad(prim);
		}
		//gl.End();
	}

	// Need to draw some dlit polys? They're drawn blank, with no textures.
	// Dynlights are drawn on them and finally the textures (multiply-blend).
	if(blank_dlight) // This only happens with LID_NORMAL_DLIT.
	{
		// Start a 2nd pass, of sorts.
		gl.Disable(DGL_TEXTURING);
		RL_DoList(LID_DLIT_NOTEX, li);
		gl.Enable(DGL_TEXTURING);
	}
}

//===========================================================================
// RL_RenderLists
//	Renders the given lists. RL_DoList does the actual work, we just set
//	up and restore the DGL state here.
//===========================================================================
void RL_RenderLists(int lid, rendlist_t **lists, int num)
{
	int i;

	// If there are just a few empty lists, no point in setting and 
	// restoring the state.
	if(num <= 3) // Covers dynlights and skymask.
	{
		for(i = 0; i < num; i++) if(lists[i]->last) break;
		if(i == num) return; // Nothing to do!
	}
	
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
		gl.ZBias(NORMALBIAS);
		// Disable alpha blending; some video cards think alpha zero is
		// is still translucent. And I guess things should render faster
		// with no blending...
		gl.Disable(DGL_BLENDING);
		break;

	case LID_SHADOWS:
		gl.ZBias(SHADOWBIAS);
		if(whitefog) gl.Disable(DGL_FOG);
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE_MINUS_SRC_COLOR);
		break;

	case LID_DLIT_TEXTURED:
		gl.ZBias(DLITBIAS);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);	
		// Multiply src and dest colors.
		gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
		gl.Color3f(1, 1, 1);
		break;

	case LID_DYNAMIC_LIGHTS:
		gl.ZBias(DYNLIGHTBIAS);
		// Disable fog.
		if(whitefog) gl.Disable(DGL_FOG);
		// This'll allow multiple light quads to be rendered on top of 
		// each other.
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		// The source is added to the destination.
		gl.Func(DGL_BLENDING, DGL_ONE, DGL_ONE); 
		break;

	case LID_DETAILS:
		if(!gl.Enable(DGL_DETAIL_TEXTURE_MODE))
			return; // Can't render detail textures...
		gl.ZBias(DETAILBIAS);
		gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);
		break;
	}

	// Render each of the provided lists.
	for(i = 0; i < num; i++)
		if(lists[i]->last) RL_DoList(lid, lists[i]);

	// Restore state.
	switch(lid)
	{
	case LID_SKYMASK:
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		gl.Enable(DGL_TEXTURING);
		break;

	case LID_NORMAL:
	case LID_NORMAL_DLIT:
		gl.ZBias(0);
		gl.Enable(DGL_BLENDING);
		break;

	case LID_SHADOWS:
		gl.ZBias(0);
		gl.Enable(DGL_DEPTH_WRITE);
		if(whitefog) gl.Enable(DGL_FOG);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		break;

	case LID_DLIT_TEXTURED:
		gl.ZBias(0);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);	
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
		break;

	case LID_DYNAMIC_LIGHTS:
		gl.ZBias(0);
		if(whitefog) gl.Enable(DGL_FOG);
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA); 
		break;

	case LID_DETAILS:
		gl.ZBias(0);
		gl.Disable(DGL_DETAIL_TEXTURE_MODE);
		gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
		break;
	}
}

//===========================================================================
// RL_RenderAllLists
//===========================================================================
void RL_RenderAllLists()
{
	// Multiplicative lights?
	boolean muldyn = !dlBlend && !whitefog; 

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

	// Mask the sky in the Z-buffer.
	RL_RenderLists(LID_SKYMASK, ptr_mask_rlists, NUM_RLSKY);

/*	if(!renderTextures) gl.Disable(DGL_TEXTURING);*/

	BEGIN_PROF( PROF_RL_RENDER_NORMAL );

	// Render the real surfaces of the visible world.
	RL_RenderLists(muldyn? LID_NORMAL_DLIT : LID_NORMAL,
		rlists, numrlists);

	END_PROF( PROF_RL_RENDER_NORMAL );

	// Render object shadows.
	RL_RenderLists(LID_SHADOWS, ptr_shadow_rlist, 1);

	BEGIN_PROF( PROF_RL_RENDER_LIGHT );

	// Render dynamic lights.
	if(dlBlend != 3)
		RL_RenderLists(LID_DYNAMIC_LIGHTS, ptr_dl_rlists, NUM_RLDYN);

	// Apply the dlit pass?
	if(muldyn) RL_RenderLists(LID_DLIT_TEXTURED, rlists, numrlists);

	END_PROF( PROF_RL_RENDER_LIGHT );

	// Render the detail texture pass?
	if(r_detail) RL_RenderLists(LID_DETAILS, rlists, numrlists);

	BEGIN_PROF( PROF_RL_RENDER_MASKED );

	// Draw masked walls, sprites and models.
	Rend_DrawMasked();

	// Draw particles.
	PG_Render();

	END_PROF( PROF_RL_RENDER_MASKED );

	END_PROF( PROF_RL_RENDER_ALL );

/*	if(!renderTextures) gl.Enable(DGL_TEXTURING);*/
}

