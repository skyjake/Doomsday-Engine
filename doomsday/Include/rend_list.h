//===========================================================================
// REND_LIST.H
//===========================================================================
#ifndef __DOOMSDAY_REND_LIST_H__
#define __DOOMSDAY_REND_LIST_H__

#define RL_MAX_POLY_SIDES	64
#define RL_MAX_DIVS			64

// Rendpoly flags.
#define RPF_MASKED		0x0001	// Use the special list for masked textures.
#define RPF_SKY_MASK	0x0004	// A sky mask polygon.
#define RPF_LIGHT		0x0008	// A dynamic light.
#define RPF_DLIT		0x0010	// Normal list: poly is dynamically lit.
#define RPF_GLOW		0x0020	// Multiply original vtx colors.
#define RPF_DETAIL		0x0040	// Render with detail (incl. vtx distances)
#define RPF_WALL_GLOW	0x0080	// Used with RPF_LIGHT.
#define RPF_SHADOW		0x0100

typedef enum {
	rp_none,
	rp_quad,					// Wall segment.
	rp_divquad,					// Divided wall segment.
	rp_flat						// Floor or ceiling.
} rendpolytype_t;

typedef enum {
	rl_quads,					// Normal or divided wall segments.
	rl_flats					// Planes (floors or ceilings).
} rendlisttype_t;

typedef struct {
	float pos[2];				// X and Y coordinates.
	gl_rgb_t color;				// Color of the vertex.
	float dist;					// Distance to the vertex.
} rendpoly_vertex_t;

// rendpoly_t is only for convenience; the data written in the rendering
// list data buffer is taken from this struct.
typedef struct {
	rendpolytype_t type;		
	short flags;				// RPF_*.
	int texw, texh;				// Texture width and height.
	float texoffx, texoffy;		// Texture coordinates for left/top (in real texcoords).
	DGLuint tex;				// Used for masked textures.
	detailinfo_t *detail;		// Detail texture name and dimensions.
	struct lumobj_s *light;		// Used with RPF_LIGHT.
	float top, bottom;			
	float length;
	int numvertices;			// Number of vertices for the poly.
	rendpoly_vertex_t vertices[RL_MAX_POLY_SIDES];
	struct div_t {
		int num;
		float pos[RL_MAX_DIVS];
	} divs[2];					// For wall segments (two vertices).
} rendpoly_t;

// PrepareFlat directions.
#define RLPF_NORMAL		0
#define RLPF_REVERSE	1

extern float	rend_light_wall_angle;
extern float	detailFactor, detailMaxDist, detailScale;

void RL_Init();
void RL_ClearLists();
void RL_DeleteLists();
void RL_AddPoly(rendpoly_t *poly);
void RL_PrepareFlat(rendpoly_t *poly, int numvrts, fvertex_t *vrts, 
					int dir, subsector_t *subsector);
void RL_VertexColors(rendpoly_t *poly, int lightlevel, byte *rgb);
void RL_RenderAllLists();

#endif