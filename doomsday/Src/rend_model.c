
//**************************************************************************
//**
//** REND_MODEL.C
//**
//** MD2/DMD renderer.
//**
//**************************************************************************

// Note: Light vectors and triangle normals are considered to be
// in a totally separate, right-handed coordinate system.

// There is some more confusion with Y and Z axes as the game uses 
// Z as the vertical axis and the rendering code and model definitions
// use the Y axis.

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

//#define RENDER_AS_TRIANGLES

#define MAX_MODEL_LIGHTS	10
#define DOTPROD(a, b)		(a[0]*b[0] + a[1]*b[1] + a[2]*b[2])
#define QATAN2(y,x)			qatan2(y,x)
#define QASIN(x)				asin(x) // FIXME: Precalculate arcsin.

// TYPES -------------------------------------------------------------------

typedef struct {
	boolean		used;
	fixed_t		dist;		// Only an approximation.
	lumobj_t	*lum;
	float		vector[3];	// Light direction vector.
	float		color[3];	// How intense the light is (0..1, RGB).
} mlight_t;

typedef struct {
	float		vertex[3];
	float		shinytexcoord[2];
	float		color[3];
	float		normal[3];
} mvertex_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gametic;
extern float vx, vy, vz;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int modelLight = 4;
int frameInter = true;
int rend_model_shiny_near = 500;
int rend_model_shiny_far = 750;
int model_tri_count;
float rend_model_lod = 256;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static float worldLight[3] = { .267261f, .534522f, .801783f };
static float ceilingLight[3] = { 0, 0, 1 };
static float floorLight[3] = { 0, 0, -1 };

static mlight_t lights[MAX_MODEL_LIGHTS] =
{
	// The first light is the world light.
	false, 0, NULL, { 0, 0, 0 }, { 1, 1, 1 }
};

// Parameters for the modelLighter.
// Global variables as parameters. Urgh.
static vissprite_t *mlSpr;

// CODE --------------------------------------------------------------------

static float __inline qatan2(float y, float x)
{
	float ang = BANG2RAD(bamsAtan2(y * 512, x * 512));
	if(ang > PI) ang -= 2*(float)PI;
	return ang;

	// This is slightly faster, I believe...
	//return atan2(y, x);
}

static void scaleAmbientRgb(float *out, byte *in, float mul)
{
	int i;
	
	if(mul < 0) mul = 0;
	if(mul > 1) mul = 1;
	for(i=0; i<3; i++) 
		if(out[i] < in[i] * mul/255)
			out[i] = in[i] * mul/255;
}

static void scaleFloatRgb(float *out, byte *in, float mul)
{
	memset(out, 0, sizeof(float)*3);
	scaleAmbientRgb(out, in, mul);
}

//===========================================================================
// modelLighter
//===========================================================================
static boolean modelLighter(lumobj_t *lum, fixed_t xy_dist)
{
	fixed_t		z_dist = ((mlSpr->mo.gz + mlSpr->mo.gzt) >> 1) 
					- (lum->thing->z + FRACUNIT*lum->center);
	fixed_t		dist = P_ApproxDistance(xy_dist, z_dist);
	int			i, maxIndex;
	fixed_t		maxDist = -1;
	mlight_t	*light;

	// If the light is too far away, skip it.
	if(dist > (dlMaxRad << FRACBITS)) return true;

	// See if this lumobj is close enough to make it to the list.
	// (In most cases it should be the case.)
	for(i = 1, light = lights+1; i < modelLight; i++, light++)
	{
		if(light->dist > maxDist)
		{
			maxDist = light->dist;
			maxIndex = i;
		}
	}
	// Now we know the farthest light on the current list.
	if(dist < maxDist)
	{
		// The new light is closer. Replace the old max.
		lights[maxIndex].lum = lum;
		lights[maxIndex].dist = dist;
	}
	return true;
}

//===========================================================================
// R_GetVisibleFrame
//===========================================================================
dmd_frame_t *R_GetVisibleFrame(modeldef_t *mf, int subnumber, int mobjid)
{
	model_t *mdl = modellist[mf->sub[subnumber].model];
	int index = mf->sub[subnumber].frame;
	
	if(mf->flags & MFF_IDFRAME)
		index += mobjid % mf->sub[subnumber].framerange;
	if(index >= mdl->info.numFrames)
	{
		Con_Error("R_GetVisibleFrame: Frame index out of bounds.\n"
			"  (Model: %s)\n", mdl->fileName);
	}
	return mdl->frames + index;
}

//===========================================================================
// R_RenderModel
//	This has swollen somewhat; a rewrite would be in order!
//===========================================================================
void Rend_RenderModel(vissprite_t *spr, int number)
{
	modeldef_t		*mf = spr->mo.mf, *nextmf = spr->mo.nextmf;
	submodeldef_t	*smf = &mf->sub[number];
	model_t			*mdl = modellist[smf->model];
	dmd_frame_t		*frame = R_GetVisibleFrame(mf, number, spr->mo.id);
	dmd_frame_t		*nextframe = NULL;
	byte			*ptr;
	int				pass, i, k, c, count, sectorlight;
	dmd_glCommandVertex_t *glc;
	dmd_vertex_t *vtx, *nextvtx;
#if RENDER_AS_TRIANGLES
	md2_triangle_t	*tri;
#endif
	int				additiveBlending = 0; // +1 or -1
	boolean			lightVertices = false;
	float			alpha, customAlpha, yawAngle, pitchAngle;
	rendpoly_t		tempquad;
	mlight_t		*light;
	mvertex_t		*vertices, *mVtx;
	int				numVertices;
	float			modelCenter[3];
	float			lightCenter[3], dist, intensity;
	lumobj_t		*lum;
	float			vtxLight[4], dot;
	int				mflags = smf->flags;
	int				useskin;
	int				numlights = 0;
	float			ambientColor[3] = { 0, 0, 0 };
	float			inv, interpos, endpos;
	float			shininess = mf->def->sub[number].shiny;
	float			*shinycol = mf->def->sub[number].shinycolor;
	float			sh_u, sh_v, sh_ang, sh_pnt, *nptr;
	float			delta[3], mdl_nyaw, mdl_npitch;
	float			lodFactor;
#if RENDER_AS_TRIANGLES
	float			skinWidth, skinHeight;
#endif
	int				activeLod = 0;

	// Distance reduces shininess (if not truly shiny).
	if(shininess > 0 && shininess < 1 
		&& spr->distance > rend_model_shiny_near)
	{
		if(spr->distance < rend_model_shiny_far)
		{
			shininess *= 1 - (spr->distance - rend_model_shiny_near) 
				/ (rend_model_shiny_far - rend_model_shiny_near);
		}
		else
		{
			// Too far away.
			shininess = 0;
		}
	}

	useskin = smf->skin;
	// Selskin overrides the skin range.
	if(mflags & MFF_SELSKIN)
	{
		i = (spr->mo.selector >> DDMOBJ_SELECTOR_SHIFT)
			& mf->def->sub[number].selskinbits[0]; // Selskin mask
		c = mf->def->sub[number].selskinbits[1]; // Selskin shift
		if(c > 0) i >>= c; else i <<= -c;
		if(i > 7) i = 7; // Maximum number of skins for selskin.
		if(i < 0) i = 0; // Improbable (impossible?), but doesn't hurt.
		useskin = mf->def->sub[number].selskins[i];
	}
	// Is there a skin range for this frame?
	// (During model setup skintics and skinrange are set to >0.)
	if(smf->skinrange > 1)
	{
		// What rule to use for determining the skin?
		useskin += (mflags & MFF_IDSKIN? spr->mo.id
			: gametic/mf->skintics) % smf->skinrange;
	}

	interpos = spr->mo.inter;

	// Scale interpos. Intermark becomes zero and endmark becomes one.
	// (Full sub-interpolation!) But only do it for the standard interrange.
	// If a custom one is defined, don't touch interpos.
	if(mf->interrange[0] == 0 && mf->interrange[1] == 1)
	{
		if(mf->internext) 
			endpos = mf->internext->intermark;
		else
			endpos = 1;
		interpos = (interpos - mf->intermark) / (endpos - mf->intermark);		
	}

	if(mf->scale[VX] == 0
		&& mf->scale[VY] == 0
		&& mf->scale[VZ] == 0) return;	// Why bother? It's infinitely small...

	// Check for possible interpolation.
	if(frameInter 
		&& nextmf
		&& !(smf->flags & MFF_DONT_INTERPOLATE))
	{
		if(nextmf->sub[number].model == smf->model)
		{
			//nextframe = mdl->frames + nextmf->sub[number].frame;
			nextframe = R_GetVisibleFrame(nextmf, number, spr->mo.id);
			nextvtx = nextframe->vertices;
		}
	}

	// Need translation?
	if(smf->flags & MFF_SKINTRANS) 
		useskin = (spr->mo.flags & DDMF_TRANSLATION) >> DDMF_TRANSSHIFT;

	// Submodel can define a custom Transparency level.
	customAlpha = 1 - smf->alpha/255.0f;

	// Light level.
	if(missileBlend && (spr->mo.flags & DDMF_BRIGHTSHADOW
		|| mflags & MFF_BRIGHTSHADOW))
	{
		alpha = .80f;	//204;	// 80 %.
		additiveBlending = 1;
	}
	else if(mflags & MFF_BRIGHTSHADOW2)
	{
		alpha = customAlpha; // NOTE: CustomAlpha is used here!!
		additiveBlending = 1;
	}
	else if(mflags & MFF_DARKSHADOW)
	{
		alpha = customAlpha; 
		additiveBlending = -1;
	}
	else if(spr->mo.flags & DDMF_SHADOW || mflags & MFF_SHADOW2)
		alpha = .2f; 
	else if(spr->mo.flags & DDMF_ALTSHADOW || mflags & MFF_SHADOW1)
		alpha = .62f;	
/*	else if(spr->alpha >= 0)
		alpha = spr->alpha * customAlpha; */
	else
		alpha = customAlpha;

	// More custom alpha?
	if(spr->mo.alpha >= 0) alpha *= spr->mo.alpha;

	if(alpha <= 0) return; // Fully transparent.

	// Init. Set the texture.
	if(shininess < 1) GL_BindTexture(GL_PrepareSkin(mdl, useskin));

	yawAngle = spr->mo.yaw;
	pitchAngle = spr->mo.pitch;

	// Coordinates to the center of the model.
	modelCenter[VX] = Q_FIX2FLT(spr->mo.gx) + mf->offset[VX] 
		+ spr->mo.visoff[VX];
	modelCenter[VY] = Q_FIX2FLT(spr->mo.gy) + mf->offset[VZ] 
		+ spr->mo.visoff[VY];
	modelCenter[VZ] = Q_FIX2FLT((spr->mo.gz+spr->mo.gzt) >> 1) 
		+ mf->offset[VY] + spr->mo.visoff[VZ];

	if(spr->mo.lightlevel < 0 || mflags & MFF_FULLBRIGHT) 
	{
		ambientColor[0] = ambientColor[1] = ambientColor[2] = 1;
		gl.Color4f(1, 1, 1, alpha);
	}
	else
	{
		tempquad.vertices[0].dist = Rend_PointDist2D(spr->mo.v1);
		tempquad.numvertices = 1;
		sectorlight = r_ambient > spr->mo.lightlevel? r_ambient 
			: spr->mo.lightlevel;
		RL_VertexColors(&tempquad, sectorlight, spr->mo.rgb);
		// This way the distance darkening has an effect.
		for(i = 0; i < 3; i++)
		{
			ambientColor[i] = tempquad.vertices[0].color.rgb[i] / 150.0f;
			if(ambientColor[i] > 1) ambientColor[i] = 1;
		}
		if(modelLight)
		{
			memset(lights, 0, sizeof(lights));
			lightVertices = true;
			// The model should be lit with world light. 
			numlights++;
			lights[0].used = true;
			// Set the correct intensity.
			for(i=0; i<3; i++) 
			{
				lights[0].vector[i] = worldLight[i];
				lights[0].color[i] = ambientColor[i];// * 1.8f;
				// Now we can diminish the actual ambient light that
				// hits the object. (Gotta have some contrast.)
				ambientColor[i] *= .5f; //.9f;
			}
			// Rotate the light direction to model space.
			M_RotateVector(lights[0].vector, -yawAngle, -pitchAngle);
			//ambientLight *= .9f;
			// Plane glow?
			if(spr->mo.hasglow)
			{
				if(spr->mo.ceilglow[0] 
					|| spr->mo.ceilglow[1] 
					|| spr->mo.ceilglow[2])
				{
					light = lights + numlights++;
					light->used = true;
					memcpy(light->vector, &ceilingLight, sizeof(ceilingLight));
					M_RotateVector(light->vector, -yawAngle, -pitchAngle);
					dist = 1 - (spr->mo.secceil - FIX2FLT(spr->mo.gzt)) / glowHeight;
					scaleFloatRgb(light->color, spr->mo.ceilglow, dist);
					scaleAmbientRgb(ambientColor, spr->mo.ceilglow, dist/3);
				}
				if(spr->mo.floorglow[0] 
					|| spr->mo.floorglow[1] 
					|| spr->mo.floorglow[2])
				{
					light = lights + numlights++;
					light->used = true;
					memcpy(light->vector, &floorLight, sizeof(floorLight));
					M_RotateVector(light->vector, -yawAngle, -pitchAngle);
					dist = 1 - (FIX2FLT(spr->mo.gz) - spr->mo.secfloor) / glowHeight;
					scaleFloatRgb(light->color, spr->mo.floorglow, dist);
					scaleAmbientRgb(ambientColor, spr->mo.floorglow, dist/3);
				}
			}
		}
		// Add extra light using dynamic lights.
		if(modelLight > numlights && dlInited)
		{
			// Find the nearest sources of light. They will be used to
			// light the vertices a bit later. First initialize the array.
			for(i=numlights; i<MAX_MODEL_LIGHTS; i++) 
				lights[i].dist = DDMAXINT;
			mlSpr = spr;
			DL_RadiusIterator(spr->mo.gx, spr->mo.gy, dlMaxRad << FRACBITS, modelLighter);

			// Calculate the directions and intensities of the lights.
			for(i=numlights, light=lights+1; i<modelLight; i++, light++)
			{
				if(!light->lum) continue;
				lum = light->lum;
			
				// This isn't entirely accurate, but who could notice?
				dist = FIX2FLT(light->dist);				

				// The intensity of the light.
				intensity = (1 - dist/(lum->radius*2)) * 2;
				if(intensity < 0) intensity = 0;
				if(intensity > 1) intensity = 1;

				if(intensity == 0)
				{
					// No point in lighting with this!
					light->lum = NULL;
					continue;
				}

				light->used = true;

				// The center of the light source.
				lightCenter[VX] = Q_FIX2FLT(lum->thing->x);
				lightCenter[VY] = Q_FIX2FLT(lum->thing->y);
				lightCenter[VZ] = Q_FIX2FLT(lum->thing->z) + lum->center;

				// Calculate the normalized direction vector, 
				// pointing out of the model.
				for(c=0; c<3; c++) 
				{
					light->vector[c] = (lightCenter[c] - modelCenter[c]) / dist;
					// ...and the color of the light.
					light->color[c] = lum->rgb[c]/255.0f * intensity;
				}

				// We must transform the light vector to model space.
				M_RotateVector(light->vector, -yawAngle, -pitchAngle);				
			}
			numlights = i;
		}
		if(!modelLight)
		{
			// Just the ambient light.
			gl.Color4ub(tempquad.vertices[0].color.rgb[CR],
				tempquad.vertices[0].color.rgb[CG],
				tempquad.vertices[0].color.rgb[CB],
				alpha * 255);
			lightVertices = false;
		}
	}

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();

	// Model space => World space
	gl.Translatef(spr->mo.v1[VX] + mf->offset[VX] + spr->mo.visoff[VX], 
		FIX2FLT(spr->mo.gz) + mf->offset[VY] + spr->mo.visoff[VZ] 
		- FIX2FLT(spr->mo.floorclip), 
		spr->mo.v1[VY] + mf->offset[VZ] + spr->mo.visoff[VY]);

	// Model rotation.
	gl.Rotatef(spr->mo.viewaligned? spr->mo.v2[VX] : yawAngle, 0, 1, 0);
	gl.Rotatef(spr->mo.viewaligned? spr->mo.v2[VY] : pitchAngle, 0, 0, 1); 

	// Scaling and model space offset.
	gl.Scalef(mf->scale[VX], mf->scale[VY], mf->scale[VZ]);
	gl.Translatef(smf->offset[VX], smf->offset[VY], smf->offset[VZ]);

	// Change the blending mode.
	if(additiveBlending > 0)		// Bright.
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
	else if(additiveBlending < 0)	// Dark.
		gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_ONE_MINUS_SRC_ALPHA);

	// Should we prepare for shiny rendering?
	if(shininess > 0)
	{
		// Calculate normalized (0,1) model yaw and pitch.
		mdl_nyaw = (spr->mo.viewaligned? spr->mo.v2[VX] : yawAngle) / 360;
		mdl_npitch = (spr->mo.viewaligned? spr->mo.v2[VY] : pitchAngle) / 360;
		
		// Are they in range?
		while(mdl_nyaw < 0) mdl_nyaw++;
		while(mdl_nyaw > 1) mdl_nyaw--;
		while(mdl_npitch < 0) mdl_npitch++;
		while(mdl_npitch > 1) mdl_npitch--;
		
		delta[VX] = modelCenter[VX] - vx;
		delta[VY] = modelCenter[VY] - vz;
		delta[VZ] = modelCenter[VZ] - vy;
		sh_ang = QATAN2(delta[VZ], M_ApproxDistancef(delta[VX], delta[VY]))
			/ PI + 0.5f; // sh_ang is [0,1]
		sh_pnt = QATAN2(delta[VY], delta[VX]) / (2*PI);
	}

	// Prepare the vertices (calculate interpolated coordinates,
	// lighting, texture coordinates, normals).
	numVertices = mdl->info.numVertices;
#if RENDER_AS_TRIANGLES
	skinWidth = mdl->header.skinWidth;
	skinHeight = mdl->header.skinHeight;
#endif
	vertices = Z_Malloc(sizeof(mvertex_t) * numVertices, PU_STATIC, 0);

	// Calculate the suitable LOD.
	if(mdl->info.numLODs > 1 && rend_model_lod != 0)
	{
		lodFactor = rend_model_lod * screenWidth/640.0f 
			/ (fieldOfView/90.0f);
		if(lodFactor) lodFactor = 1/lodFactor;
		
		// Determine the LOD we will be using.
		activeLod = (int) (lodFactor * spr->distance);
		if(activeLod < 0) activeLod = 0;
		if(activeLod >= mdl->info.numLODs) 
			activeLod = mdl->info.numLODs - 1;
	}
	else
	{
		// Why bother...
		lodFactor = 0;
		activeLod = 0;
	}

	// Calculate interpolated vertices, shiny texcoords and lighting.
	for(i = 0, mVtx = vertices, vtx = frame->vertices; 
		i < numVertices; i++, mVtx++, vtx++)
	{
		// Is this vertex used at this LOD?
		if(mdl->vertexUsage	&& !(mdl->vertexUsage[i] & (1 << activeLod)))
			continue; // Not used; can be skipped.
	
		if(nextframe)
		{
			inv = 1 - interpos;
			nextvtx = nextframe->vertices + i; 
		}

		// Calculate the surface normal at this vertex.
		memcpy(mVtx->normal, vtx->normal, sizeof(mVtx->normal));
		if(nextframe
			&& (vtx->normal[VX] != nextvtx->normal[VX]
				|| vtx->normal[VY] != nextvtx->normal[VY]
				|| vtx->normal[VZ] != nextvtx->normal[VZ]))
		{
			// Interpolate to next position.
			nptr = nextvtx->normal;
			mVtx->normal[VX] = mVtx->normal[VX]*inv + nptr[VX]*interpos;
			mVtx->normal[VY] = mVtx->normal[VY]*inv + nptr[VY]*interpos;
			mVtx->normal[VZ] = mVtx->normal[VZ]*inv + nptr[VZ]*interpos;
			// Normalize it (approximately).
			dist = M_ApproxDistancef(mVtx->normal[VZ],
				M_ApproxDistancef(mVtx->normal[VX], mVtx->normal[VY]));
			if(dist)
			{
				mVtx->normal[VX] /= dist;
				mVtx->normal[VY] /= dist;
				mVtx->normal[VZ] /= dist;
			}
		}

		// Shiny texture coordinates, too, if they're needed.
		if(shininess > 0)
		{
			// Calculate cylindrically mapped texcoords.
			// Quite far from perfect but very nice anyway.
			sh_u = QATAN2(mVtx->normal[VY], mVtx->normal[VX]) / (2*PI) - mdl_nyaw;
			// This'll hide the wrap-around behind the model.
			// Works more often than not.
			while(sh_u > sh_pnt) sh_u--;
			while(sh_u < sh_pnt-1) sh_u++;
			sh_u += sh_u - sh_pnt; 

			sh_v = QASIN(-mVtx->normal[VZ])/PI + 0.5f - mdl_npitch;
			sh_v += sh_v - sh_ang;
			
			mVtx->shinytexcoord[VX] = sh_u;
			mVtx->shinytexcoord[VY] = sh_v;
		}
				
		// Also calculate the light level at the vertex?
		if(lightVertices) 
		{
			// Begin with total darkness.
			vtxLight[0] = vtxLight[1] = vtxLight[2] = 0;
			
			// Add light from each source.
			for(k = 0, light = lights; k < numlights; k++, light++)
			{
				if(!light->used) continue;
				dot = DOTPROD(light->vector, mVtx->normal); 
				dot *= 1.25f; // Looks-good factor :-).
				if(dot <= 0) continue; // No light from the wrong side.
				if(dot > 1) dot = 1;
				for(c = 0; c < 3; c++)
					vtxLight[c] += dot * light->color[c];
			}
			
			// Check for ambient, too.
			for(k = 0; k < 3; k++)
			{
				if(vtxLight[k] < ambientColor[k]) 
					vtxLight[k] = ambientColor[k];

				// This is the final color.
				mVtx->color[k] = vtxLight[k];
			}
		}
				
		// The interpolated vertex coordinates.
		if(nextframe)
		{
			mVtx->vertex[VX] = vtx->vertex[VX] * inv 
				+ nextvtx->vertex[VX] * interpos;
			mVtx->vertex[VY] = vtx->vertex[VZ] * inv 
				+ nextvtx->vertex[VZ] * interpos;
			mVtx->vertex[VZ] = vtx->vertex[VY] * inv 
				+ nextvtx->vertex[VY] * interpos;
		}
		else
		{
			mVtx->vertex[VX] = vtx->vertex[VX];
			mVtx->vertex[VY] = vtx->vertex[VZ];
			mVtx->vertex[VZ] = vtx->vertex[VY];
		}		
	}

	// Render the model in two passes:
	// Pass 0 = regular
	// Pass 1 = shiny skin
	for(pass = 0; pass < 2; pass++)
	{
		//float starttime = I_GetSeconds();

		if(pass == 0 && shininess >= 1) continue;
		if(pass == 1 && shininess <= 0) continue;

		switch(pass)
		{
		case 0:
			break;

		case 1: // Initialize the shiny skin pass.
			// Change the texture.
			GL_BindTexture(GL_PrepareShinySkin(mf, number));

			// We're rendering on top of pass zero (usually).
			gl.Func(DGL_DEPTH_TEST, DGL_LEQUAL, 0);

			// Set blending mode, two choices: reflected and specular.
			gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, 
				mflags & MFF_SHINY_SPECULAR? DGL_ONE	// Specular.
				: DGL_ONE_MINUS_SRC_ALPHA);				// Reflected.

			if(mflags & MFF_SHINY_LIT) 
			{
				gl.Color4f(ambientColor[0] * shinycol[0],
					ambientColor[1] * shinycol[1], 
					ambientColor[2] * shinycol[2], shininess);
			}
			else
			{
				gl.Color4f(shinycol[0], shinycol[1], shinycol[2], shininess);
			}
			break;
		}

#if RENDER_AS_TRIANGLES
		// Render as 'separate' triangles.
		gl.Begin(DGL_TRIANGLES);
		count = mdl->header.numTriangles;
		model_tri_count += count; 
		for(i = 0, tri = mdl->triangles; i < count; i++, tri++)
			for(k = 0; k < 3; k++)
			{
				mVtx = vertices + tri->vertexIndices[k];
				if(pass == 0) 
				{
					// Regular pass: just use the texcoord at the vertex.
					// FIXME: These could be calculated when the model is
					// loaded, but I doubt it makes that big a difference.
					gl.TexCoord2f(
						mdl->texCoords[tri->textureIndices[k]].s/skinWidth,
						mdl->texCoords[tri->textureIndices[k]].t/skinHeight);
				}
				else if(pass == 1)
				{
					sh_u = mVtx->shinytexcoord[VX];
					gl.TexCoord2f(sh_u, mVtx->shinytexcoord[VY]);
				}
				
				// Also calculate the light level at the vertex?
				if(pass == 0 && lightVertices) 
				{
					memcpy(vtxLight, mVtx->color, sizeof(mVtx->color));
					vtxLight[3] = alpha;

					// Set the color.
					gl.Color4fv(vtxLight);								
				}
				gl.Vertex3fv(mVtx->vertex);
			}
		gl.End();

#else // Render as strips/fans (more efficient!).

		// Draw the triangles using the GL commands.
		ptr = (byte*) mdl->lods[activeLod].glCommands;
		while(*ptr)
		{
			count = *(int*) ptr;
			ptr += 4;
			gl.Begin(count > 0? DGL_TRIANGLE_STRIP : DGL_TRIANGLE_FAN);
			if(count < 0) count = -count;
			model_tri_count += count - 2;
			while(count--)
			{	
				glc = (dmd_glCommandVertex_t*) ptr;
				ptr += sizeof(dmd_glCommandVertex_t);
				mVtx = vertices + glc->vertexIndex;

				// Texture coordinate.
				if(pass == 0) 
				{
					// Regular pass: just use the texcoord at the vertex.
					gl.TexCoord2fv(&glc->s);//, glc->t);

					if(lightVertices) 
					{
						memcpy(vtxLight, mVtx->color, sizeof(mVtx->color));
						vtxLight[3] = alpha;
						// Set the color.
						gl.Color4fv(vtxLight);								
					}
				}
				else if(pass == 1)
				{
					gl.TexCoord2fv(mVtx->shinytexcoord);
				}
				gl.Vertex3fv(mVtx->vertex);
			}
			gl.End();
		}
		
#endif

		// Restore old rendering state.
		switch(pass)
		{
		case 0:
			break;

		case 1:
			gl.Func(DGL_DEPTH_TEST, DGL_LESS, 0);
			if(!additiveBlending)
				gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
			break;
		}

		//ST_Message("mdltime:%.2f sec\n", I_GetSeconds()-starttime);
	}
	
	Z_Free(vertices);

	// Restore renderer state.
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();

	if(additiveBlending)
	{
		// Change to normal blending.
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
	}

/*#if _DEBUG
	ST_Message("numseqs:%i avglen:%i\n", numseqs, 
		seqlen/(numseqs+(numseqs==0)));
#endif*/
}