
//**************************************************************************
//**
//** REND_HALO.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

//#define Z_TEST_BIAS		.00005
#define NUM_FLARES		5

// TYPES -------------------------------------------------------------------

typedef struct flare_s
{
	float offset;
	float size;
	float alpha;
	int texture;		// -1=dlight, 0=flare, 1=brflare, 2=bigflare
} 
flare_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

flare_t flares[NUM_FLARES] =
{
#if 0
	{ 0,	0.85f,	.6f,	-1 /*2*/ }, 	// Primary flare.
	{ 1,	.35f,	.3f,	0 },			// Main secondary flare.
	{ 1.5f,	.25f,	.2f,	1 },
	{ -.6f,	.2f,	.2f,	0 },
	{ .4f,	.25f,	.15f,	0 }
#endif

	{ 0,	1,		1,		0 },		 	// Primary flare.
	{ 1,	.41f,	.5f,	0 },			// Main secondary flare.
	{ 1.5f,	.29f,	.333f,	1 },
	{ -.6f,	.24f,	.333f,	0 },
	{ .4f,	.29f,	.25f,	0 }
};

/*	{ 0, 1 }, 		// Primary flare.
	{ 1, 2 },		// Main secondary flare.
	{ 1.5f, 4 },
	{ -.6f, 4 },
	{ .4f, 4 }
*/

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int				haloMode = 5, haloBright = 50, haloSize = 50;
int				haloOccludeSpeed = 48;
float			haloZMagDiv = 100, haloMinRadius = 20;
float			haloDimStart = 10, haloDimEnd = 100;
/*
gl_fc3vertex_t	*haloData = 0;
int				dataSize = 0, usedData;
*/
float			haloFadeMax = 0, haloFadeMin = 0, minHaloSize = 1;
//float			flareZOffset = 1;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// H_Register
//===========================================================================
void H_Register(void)
{
	cvar_t cvars[] =
	{
		"rend-halo-zmag-div",	CVF_NO_MAX,	CVT_FLOAT,	&haloZMagDiv,	1, 1,	"Halo Z magnification.",
		"rend-halo-radius-min", CVF_NO_MAX, CVT_FLOAT,	&haloMinRadius,	0, 0,	"Minimum halo radius.",
		"rend-halo-dim-near",	CVF_NO_MAX,	CVT_FLOAT,	&haloDimStart,	0, 0,	"Halo dimming relative start distance.",
		"rend-halo-dim-far",	CVF_NO_MAX,	CVT_FLOAT,	&haloDimEnd,	0, 0,	"Halo dimming relative end distance.",
		NULL
	};
	Con_AddVariableList(cvars);
}

#if 0
//===========================================================================
// H_Clear
//	Free the halo data. Done at shutdown.
//===========================================================================
void H_Clear()
{
	if(haloData) Z_Free(haloData);
	haloData = 0;
	dataSize = 0;
	usedData = 0;
}

//===========================================================================
// H_InitForNewFrame
//	Prepares the list of halos to render. Goes through the luminousList.
//===========================================================================
void H_InitForNewFrame()
{
	extern float vx, vy, vz;
	float		vpos[3] = { vx, vy, vz };
	int			i, c, neededSize = numLuminous;
	lumobj_t	*lum;
	gl_fc3vertex_t *vtx;
	float		factor;

	// Allocate a large enough buffer for the halo data.
	if(neededSize > dataSize)
	{
		dataSize = neededSize;
		if(haloData) Z_Free(haloData);
		haloData = Z_Malloc(sizeof(*haloData) * dataSize, PU_STATIC, 0);
	}

	// Is there something to do?
	if(!dataSize || !haloData || !neededSize)
	{
		usedData = 0;
		return;
	}

	for(i = 0, vtx = haloData, lum = luminousList; i < numLuminous; i++, lum++)
	{
		if(!(lum->flags & LUMF_RENDERED)
			|| lum->flags & LUMF_NOHALO) continue;
		// Insert the data into the halodata buffer.
		vtx->pos[VX] = FIX2FLT(lum->thing->x) + FIX2FLT(viewsin) * lum->xoff;
		vtx->pos[VY] = FIX2FLT(lum->thing->z) + lum->center;
		vtx->pos[VZ] = FIX2FLT(lum->thing->y) - FIX2FLT(viewcos) * lum->xoff;

		// Do some offseting for viewaligned sprites.
		if(1/*lum->thing->ddflags & DDMF_VIEWALIGN || alwaysAlign || useModels*/)
		{
			float dst[3], d;
			for(c=0; c<3; c++) dst[c] = vtx->pos[c] - vpos[c];
			d = sqrt(dst[0]*dst[0] + dst[1]*dst[1] + dst[2]*dst[2]);
			if(d == 0) d = .0001f;
			for(c=0; c<3; c++) dst[c] /= d;
			for(c=0; c<3; c++)
			{
				vtx->pos[c] -= dst[c] 
					* (lum->thing->radius>>FRACBITS)
					* lum->xyscale // From model def.
					* flareZOffset;
			}
		}

		if(flareFadeMax && FIX2FLT(lum->distance) < flareFadeMax)
		{
			if(FIX2FLT(lum->distance) < flareFadeMin) continue;
			factor = (FIX2FLT(lum->distance) - flareFadeMin) 
				/ (flareFadeMax - flareFadeMin);
		}
		else
		{
			factor = 1;
		}
		for(c = 0; c < 3; c++) vtx->color[c] = lum->rgb[c] / 255.0f * factor;
		vtx->color[CA] = lum->flaresize / 255.0f;
		vtx++;
	}
	// Project the vertices.
	usedData = gl.Project(vtx - haloData, haloData, haloData);
}

static void H_DrawRect(float x, float y, float size)
{
	gl.TexCoord2f(0, 0);
	gl.Vertex2f(x-size, y-size);

	gl.TexCoord2f(1, 0);
	gl.Vertex2f(x+size, y-size);

	gl.TexCoord2f(1, 1);
	gl.Vertex2f(x+size, y+size);

	gl.TexCoord2f(0, 1);
	gl.Vertex2f(x-size, y+size);
}

//===========================================================================
// H_Render
//	This must be called when the renderer is in player sprite rendering 
//	mode: 2D projection to the game view. What will happen is that each 
//	point in the haloData buffer gets a halo if it passes the Z buffer 
//	test. 
//
//	Can only be called once per frame!
//===========================================================================
void H_Render(void)
{
	int			k, i, num = usedData, tex, numRenderedFlares;
	float		centerX = viewpx + viewpw/2.0f;
	float		centerY = viewpy + viewph/2.0f;
	gl_fc3vertex_t *vtx;
	float		radius, size, clipRange = farClip-nearClip, secDiff[2];
	float		factor = flareBoldness/100.0f;
	//float		scaler = 2 - flareSize/10.0f;
	float		viewscaler = viewwidth/320.0f;
	float		flareSizeFactor, secondaryAlpha;
	boolean		needScissor = false;
	int			*readData, *iter;
	float		*zValues;
	int			oldFog;

	if(!num || !haloData || !dataSize) return;

	gl.MatrixMode(DGL_MODELVIEW);
	gl.LoadIdentity();
	gl.MatrixMode(DGL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

	// Setup the appropriate state.
	gl.Disable(DGL_DEPTH_TEST);
	gl.Disable(DGL_CULL_FACE);
	gl.GetIntegerv(DGL_FOG, &oldFog);
	if(oldFog) gl.Disable(DGL_FOG);

	// Do we need to scissor the flares to the view window?
	if(viewph != screenHeight)
	{
		needScissor = true;
		gl.Enable(DGL_SCISSOR_TEST);
		gl.Scissor(viewpx, viewpy, viewpw, viewph);
	}

	// First determine which of the halos are worth rendering.
	// Fill the Single Pixels inquiry.
	readData = Z_Malloc(sizeof(int)*(2 + num*2), PU_STATIC, 0);
	zValues = Z_Malloc(sizeof(float) * num, PU_STATIC, 0);
	readData[0] = DGL_SINGLE_PIXELS;
	readData[1] = num;
	for(i = 0, vtx = haloData, iter = readData + 2; i<num; 
		i++, vtx++, iter += 2)
	{
		iter[0] = (int) vtx->pos[VX];
		iter[1] = (int) vtx->pos[VY];
	}
/*		ptr = haloData + i*8;
		// Read the depth value.
		glReadPixels(ptr[HDO_X], ptr[HDO_Y], 1, 1, GL_DEPTH_COMPONENT,
			GL_FLOAT, &depth);

		if(depth < ptr[HDO_Z]-.0001) ptr[HDO_TOKEN] = 0;

		// Obviously, this test is not the best way to do things.
		// First of all, the sprite itself may be in front of the
		// flare when the sprite is viewed from up or down. Also,
		// the Z buffer is not the most accurate thing in the world,
		// so errors may occur.
	}*/
	gl.ReadPixels(readData, DGL_DEPTH_COMPONENT, zValues);
	for(i=0, vtx=haloData; i<num; i++, vtx++)
	{
		//printf( "%i: %f\n", i, zValues[i]);
		if(zValues[i] < vtx->pos[VZ] - Z_TEST_BIAS) 
			vtx->pos[VX] = -1;	// This marks the halo off-screen.
	}
	Z_Free(readData);
	Z_Free(zValues);

	// Additive blending.
	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);

	gl.MatrixMode(DGL_TEXTURE);
	gl.PushMatrix();
	for(i=0, vtx=haloData; i<num; i++, vtx++)
	{
		if(vtx->pos[VX] == -1) continue; // Blocked flare?

		radius = vtx->color[CA] * 10;
		if(radius > 1.5) radius = 1.5;

		// Determine the size of the halo.
		flareSizeFactor = (farClip - clipRange*vtx->pos[VZ]) * radius;
		size = flareSizeFactor * screenHeight/2000.0f * viewscaler;
	
		// Determine the offset to the main secondary flare.
		secDiff[0] = 2 * (centerX - vtx->pos[VX]);
		secDiff[1] = 2 * (centerY - vtx->pos[VY]);

		// The rotation (around the center of the flare texture).
		gl.LoadIdentity();
		gl.Translatef(.5, .5, 0);
		gl.Rotatef(BANG2DEG(bamsAtan2(2*secDiff[1], -2*secDiff[0])), 0, 0, 1);
		gl.Translatef(-.5, -.5, 0);

		// If the source object is small enough, don't render
		// secondary flares.
		if(flareSizeFactor < minFlareSize*.667f) // Two thirds, completely vanish.
		{
			numRenderedFlares = 1;
			// No secondary flares rendered.
		}
		else if(flareSizeFactor < minFlareSize)
		{
			numRenderedFlares = haloMode;
			secondaryAlpha = (3*flareSizeFactor - 2*minFlareSize) / minFlareSize;
		}
		else
		{
			numRenderedFlares = haloMode;
			secondaryAlpha = 1;
		}
		if(numRenderedFlares > 1)
		{
			float dim1 = fabs(secDiff[0] / viewpw);
			float dim2 = fabs(secDiff[1] / viewph);
			// Diminish secondary flares as they approach view borders.
			secondaryAlpha *= 1 - (dim1 > dim2? dim1 : dim2);
		}
		//if(radius < .3 && flare->size < 1) continue;
		for(k = 0; k < numRenderedFlares; k++)
		{
			flare_t *flare = flares + k;

			// The color and the alpha.
			vtx->color[CA] = factor * flare->alpha;
			if(k) vtx->color[CA] *= secondaryAlpha;
			
			tex = flare->texture;
			if(!k && tex == 2)
			{
				if(radius < .9 && radius >= .5) tex = 1;
			}
			GL_BindTexture(tex < 0? GL_PrepareLightTexture() 
				: GL_PrepareFlareTexture(tex));
			
			// Don't wrap the texture.
			gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
			gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

			gl.Begin(DGL_QUADS);
			gl.Color4fv(vtx->color);
			H_DrawRect(vtx->pos[VX] + flare->offset*secDiff[0], 
				vtx->pos[VY] + flare->offset*secDiff[1], 
				size * flare->size);
			gl.End();
		}
	}
	// Pop back the original texture matrix.
	gl.PopMatrix();

	// Restore normal rendering state.
	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
	gl.Enable(DGL_DEPTH_TEST);
	gl.Enable(DGL_CULL_FACE);
	if(needScissor)	gl.Disable(DGL_SCISSOR_TEST);
	if(oldFog) gl.Enable(DGL_FOG);
}

#endif

//===========================================================================
// H_SetupState
//===========================================================================
void H_SetupState(boolean dosetup)
{
	if(dosetup)
	{
		if(whitefog) gl.Disable(DGL_FOG);
		gl.Disable(DGL_DEPTH_WRITE);
		gl.Disable(DGL_DEPTH_TEST);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
	}
	else
	{
		if(whitefog) gl.Enable(DGL_FOG);
		gl.Enable(DGL_DEPTH_WRITE);
		gl.Enable(DGL_DEPTH_TEST);
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
	}
}

//===========================================================================
// H_RenderHalo
//	The caller must check that the sourcevis really has a ->light!
//	If 'primary' is true, we'll draw the primary halo, otherwise the
//	secondary ones (which won't be clipped or occluded by anything; 
//	they're drawn after everything else, during a separate pass).
//	If 'primary' is false, the caller must setup the rendering state.
//===========================================================================
void H_RenderHalo(vissprite_t *sourcevis, boolean primary)
{
	float			viewpos[3] = { vx, vy, vz };
	float			viewtocenter[3], mirror[3], normalviewtocenter[3];
	float			leftoff[3], rightoff[3], center[3], radius;
	float			halopos[3], occlusionfactor;
	int				i, k, tex;
	lumobj_t		*lum = sourcevis->mo.light;
	mobj_t			*mo = lum->thing;
	float			cliprange = farClip - nearClip;
	float			color[4], radx, rady, scale, turnangle = 0;
	float			fadefactor = 1, secbold, secdimfactor;
	float			coloraverage, f, distancedim;
	flare_t			*fl;

	// Is this source eligible for a halo?
/*	if(!(lum->flags & LUMF_RENDERED) 
		|| lum->flags & LUMF_NOHALO
		|| sourcevis->distance > haloFadeMax) 
		return;*/

	//---DEBUG---
//	if(!primary) return;

	if(lum->flags & LUMF_NOHALO
		|| !sourcevis->distance
		|| haloFadeMax && sourcevis->distance > haloFadeMax) return; 

	if(haloFadeMax 
		&& haloFadeMax != haloFadeMin
		&& sourcevis->distance < haloFadeMax
		&& sourcevis->distance >= haloFadeMin) 
	{
		fadefactor = (sourcevis->distance - haloFadeMin) 
			/ (haloFadeMax - haloFadeMin);
	}

	// Set the high bit of halofactor if the light is clipped. This will 
	// make P_Ticker diminish the factor to zero. Take the first step here
	// and now, though.
	if(lum->flags & LUMF_CLIPPED)
	{
		if(mo->halofactor & 0x80)
		{
			i = (lum->thing->halofactor & 0x7f) - haloOccludeSpeed;
			if(i < 0) i = 0;
			lum->thing->halofactor = i;
		}
	}
	else
	{
		if(!(mo->halofactor & 0x80))
		{
			i = (lum->thing->halofactor & 0x7f) + haloOccludeSpeed;
			if(i > 127) i = 127;
			lum->thing->halofactor = 0x80 | i;
		}
	}

	occlusionfactor = (lum->thing->halofactor & 0x7f) / 127.0f;
	if(occlusionfactor == 0) return;
	occlusionfactor = (1 + occlusionfactor)/2;

	// viewsidevec is to the left.
	for(i = 0; i < 3; i++)
	{
		leftoff[i] = viewupvec[i] + viewsidevec[i];
		rightoff[i] = viewupvec[i] - viewsidevec[i];
		// Convert the color to floating point.
		color[i] = lum->rgb[i]/255.0f;
	}

	// Setup the proper DGL state.
	if(primary) H_SetupState(true);

	center[VX] = FIX2FLT( sourcevis->mo.gx ) + sourcevis->mo.visoff[VX];
	center[VZ] = FIX2FLT( sourcevis->mo.gy ) + sourcevis->mo.visoff[VY];
	center[VY] = FIX2FLT( sourcevis->mo.gz ) + lum->center 
		+ sourcevis->mo.visoff[VZ];

	// Apply the flare's X offset. (Positive is to the right.)
	for(i = 0; i < 3; i++) center[i] -= lum->xoff*viewsidevec[i];

	// Calculate the mirrored position.
	// Project viewtocenter vector onto viewsidevec.
	for(i = 0; i < 3; i++) 
		normalviewtocenter[i] = viewtocenter[i] = center[i] - viewpos[i];

	// Calculate the dimming factor for secondary flares.
	M_Normalize(normalviewtocenter);
	secdimfactor = M_DotProduct(normalviewtocenter, viewfrontvec);

	scale = M_DotProduct(viewtocenter, viewfrontvec)
		/ M_DotProduct(viewfrontvec, viewfrontvec);
	for(i = 0; i < 3; i++) 
		halopos[i] = mirror[i] = (viewfrontvec[i]*scale - viewtocenter[i])*2;
	// Now adding 'mirror' to a position will mirror it.

	// Calculate texture turn angle.
	if(M_Normalize(halopos)) 
	{
		// Now halopos is a normalized version of the mirror vector.
		// Both vectors are on the view plane.
		turnangle = M_DotProduct(halopos, viewupvec);
		if(turnangle > 1) turnangle = 1;
		if(turnangle < -1) turnangle = -1;
		turnangle = turnangle >= 1? 0 : turnangle <= -1? PI	: acos(turnangle);
		// On which side of the up vector (left or right)?
		if(M_DotProduct(halopos, viewsidevec) < 0)
			turnangle = -turnangle;
	}

	// Radius is affected by the precalculated 'flaresize' and the 
	// distance to the source.
	
	// Prepare the texture rotation matrix.
	gl.MatrixMode(DGL_TEXTURE);
	gl.PushMatrix();
	gl.LoadIdentity();
	// Rotate around the center of the texture.
	gl.Translatef(0.5f, 0.5f, 0);
	gl.Rotatef(turnangle/PI*180, 0, 0, 1);
	gl.Translatef(-0.5f, -0.5f, 0);

	// The overall brightness of the flare.
	coloraverage = (color[CR] + color[CG] + color[CB] + 1)/4;
	//overallbrightness = lum->flaresize/25 * coloraverage;

	// Small flares have stronger dimming.
	f = sourcevis->distance/lum->flaresize;
	if(haloDimStart 
		&& haloDimStart < haloDimEnd
		&& f > haloDimStart)
		distancedim = 1 - (f - haloDimStart)/(haloDimEnd - haloDimStart);
	else 
		distancedim = 1;
	
	for(i = 0, fl = flares; i < haloMode && i < NUM_FLARES; i++, fl++)
	{
		if(primary && i) break;
		if(!primary && !i) continue;

		f = 1;
		if(i)
		{
			// Secondary flare dimming?
			f = minHaloSize*lum->flaresize/sourcevis->distance;
			if(f > 1) f = 1;
		}
		f *= distancedim;

		// The color & alpha of the flare.
		color[CA] = f * (fl->alpha * occlusionfactor * fadefactor 
			+ coloraverage*coloraverage/5);
		
		radius = lum->flaresize * (1 - coloraverage/3) 
			+ sourcevis->distance/haloZMagDiv;
		if(radius < haloMinRadius) radius = haloMinRadius;
		radius *= occlusionfactor;
				
		/*
			* (farClip - clipRange/sourcevis->distance)
			+ sourcevis->distance/40);*/// * ((50 + haloSize)/100.0f);
		//if(lum->flaresize/sourcevis->distance < .01f) break; // Too small!
		secbold = coloraverage - 8*(1 - secdimfactor);

		/*if(i && color[CA] < .5f && lum->flareSize < minHaloSize)
			break; // Don't render secondary flares.*/

		color[CA] *= .8f * haloBright/100.0f;
		if(i) color[CA] *= secbold; // Secondary flare boldness.
		if(color[CA] <= 0) break; // Not visible.

/*		radius = lum->flaresize/255.0f * 10;
		if(radius > 1.5) radius = 1.5f;

		// Determine the size of the halo.
		flaresizefactor = (farClip - clipRange
			*((sourcevis->distance - nearClip)/clipRange)) * radius;
		size = flareSizeFactor * screenHeight/2000.0f * viewscaler;
*/
		gl.Color4fv(color);

		if(lum->flaresize > 45 
			|| coloraverage > .90 && lum->flaresize > 20)
		{
			// The "Very Bright" condition.
			radius *= .65f;
			if(!i)
				tex = GL_PrepareFlareTexture(2);
			else
				tex = GL_PrepareFlareTexture(fl->texture);
		}
		else 
		{
			if(!i)
				tex = GL_PrepareLightTexture();
			else 
				tex = GL_PrepareFlareTexture(fl->texture);
		}

		GL_BindTexture(tex);

		// Don't wrap the texture. Evidently some drivers can't just 
		// take a hint... (or then something's changing the wrapping
		// mode inadvertently)
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

		// The final radius.
		radx = radius * fl->size;
		rady = radx/1.2f;

		// Determine the final position of the halo.
		halopos[VX] = center[VX];
		halopos[VY] = center[VY];
		halopos[VZ] = center[VZ];
		if(i) // Secondary halos.
		{
			// Mirror it according to the flare table.
			for(k = 0; k < 3; k++)
				halopos[k] += mirror[k] * fl->offset;
		}

		gl.Begin(DGL_QUADS);
		gl.TexCoord2f(0, 0);
		gl.Vertex3f(halopos[VX] + radx * leftoff[VX],
					halopos[VY] + rady * leftoff[VY],
					halopos[VZ] + radx * leftoff[VZ]);
		gl.TexCoord2f(1, 0);
		gl.Vertex3f(halopos[VX] + radx * rightoff[VX],
					halopos[VY] + rady * rightoff[VY],
					halopos[VZ] + radx * rightoff[VZ]);
		gl.TexCoord2f(1, 1);
		gl.Vertex3f(halopos[VX] - radx * leftoff[VX],
					halopos[VY] - rady * leftoff[VY],
					halopos[VZ] - radx * leftoff[VZ]);
		gl.TexCoord2f(0, 1);
		gl.Vertex3f(halopos[VX] - radx * rightoff[VX],
					halopos[VY] - rady * rightoff[VY],
					halopos[VZ] - radx * rightoff[VZ]);
		gl.End();
	}

	gl.MatrixMode(DGL_TEXTURE);
	gl.PopMatrix();
	
	// Undo the changes to the DGL state.
	if(primary) H_SetupState(false);
}

//===========================================================================
// CCmdFlareConfig
//	flareconfig list
//	flareconfig (num) pos/size/alpha/tex (val)
//===========================================================================
int CCmdFlareConfig(int argc, char **argv)
{	
	int		i;
	float	val;

	if(argc == 2)
	{
		if(!stricmp(argv[1], "list"))
		{
			for(i=0; i<NUM_FLARES; i++)
			{
				Con_Message("%i: pos:%f s:%.2f a:%.2f tex:%i\n", 
					i, flares[i].offset, flares[i].size, flares[i].alpha, flares[i].texture);
			}
		}
	}
	else if(argc == 4)
	{
		i = atoi(argv[1]);
		val = strtod(argv[3], NULL);
		if(i < 0 || i >= NUM_FLARES) return false;
		if(!stricmp(argv[2], "pos"))
		{
			flares[i].offset = val;
		}
		else if(!stricmp(argv[2], "size"))
		{
			flares[i].size = val;
		}
		else if(!stricmp(argv[2], "alpha"))
		{
			flares[i].alpha = val;
		}
		else if(!stricmp(argv[2], "tex"))
		{
			flares[i].texture = (int) val;
		}
	}	
	else
	{
		Con_Printf("Usage:\n");
		Con_Printf("  %s list\n", argv[0]);
		Con_Printf("  %s (num) pos/size/alpha/tex (val)\n", argv[0]);
	}
	return true;
}