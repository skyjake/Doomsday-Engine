// drOpenGL.dll
// The Doomsday graphics library driver for OpenGL
//
// list.c : List rendering

#include <stdlib.h>
#include "drOpenGL.h"

#define INLINE				__inline
#define LIST_EMPTY(rl)		(!(rl)->data || !*(rl)->data)
//#define FLAT_LIST(rl)		(((rl)->data[0] == rp_flat) || ((rl)->data[1] & RPF_MISSING_WALL))
#define FLAT_LIST(rl)		((rl)->type == rl_flats)

#define DETAIL_TEX_SIZE	128	// 128x128 pixels

#define DOF_COLORS		0x1
#define DOF_TEXCOORDS	0x2
#define DOF_DETAIL		0x4
#define DOF_JUST_DLIT	0x8

#define NORMALBIAS		1
#define DYNLIGHTBIAS	0
#define DLITBIAS		0
#define DETAILBIAS		0

rendlist_t *readlist;
detailtex_t *dtex;

// The rendering state for drawWall and drawFlat.
int with_tex, with_col, with_det;

void zBias(int level)
{
	glDepthRange(level*.0022f, 1);
}

static float DistFactor(rendpoly_t *cq, int vi)
{
	float f = 1 - cq->vertices[vi].dist 
		/ (dtex->maxdist? dtex->maxdist : detailMaxDist);
	if(f > 1) f = 1;
	if(f < 0) f = 0;
	return f;
}

static void drawWall(rendpoly_t *cq)
{
	int i, k, idx, side, other;
	byte *rgb[2] = { cq->vertices[0].color.rgb, cq->vertices[1].color.rgb };
	float *vtx[2] = { cq->vertices[0].pos, cq->vertices[1].pos };
	float z, tcS[2], tcT[2], zPos[2] = { cq->top, cq->bottom };
	float df, detcl[2][3]; // Detail color.
	
	// Calculate relative texture coordinates.
	if(with_tex || with_det)
	{
		tcS[1] = (tcS[0]=cq->texoffx/(float)cq->texw) + cq->length/cq->texw;
		tcT[1] = (tcT[0]=cq->texoffy/cq->texh) + (cq->top - cq->bottom)/cq->texh;
	}

	// If this is a detail pass, calculate detail texture coordinates.
	if(with_det)
	{
		tcS[1] = (tcS[0]=cq->texoffx/(float)DETAIL_TEX_SIZE) + cq->length/DETAIL_TEX_SIZE;
		tcT[1] = (tcT[0]=cq->texoffy/DETAIL_TEX_SIZE) + (cq->top - cq->bottom)/DETAIL_TEX_SIZE;
		
		tcS[0] *= dtex->scale * detailScale; 
		tcT[0] *= dtex->scale * detailScale;
		tcS[1] *= dtex->scale * detailScale; 
		tcT[1] *= dtex->scale * detailScale;

		// Also calculate the color for the detail texture.
		// It's <vertex-color> * <distance-factor> * <detail-factor>.
		for(i = 0; i < 2; i++)
		{
			df = DistFactor(cq, i);
			for(k = 0; k < 3; k++)
			{
				detcl[i][k] = cq->vertices[i].color.rgb[k]/255.0f 
					* df * detailFactor * dtex->strength;
			}
		}
	}
	
	// DivQuads are rendered as GL_TRIANGLES.
	if(cq->type == rp_divquad)
	{
		// A more general algorithm is used for divquads.
		for(side = 0; side < 2; side++)	// Left->right is side zero.
		{
			other = !side;
			for(i = 0; i <= cq->divs[other].num; i++)
			{
				polyCounter++;

				// The origin vertex.
				if(with_col) glColor3ubv(rgb[side]);
				if(with_tex || with_det) glTexCoord2f(tcS[side], tcT[side]);
				if(with_det) glColor3fv(detcl[side]);
				glVertex3f(vtx[side][VX], zPos[side], vtx[side][VY]);
				
				// The two vertices on the opposite side.
				if(with_col) glColor3ubv(rgb[other]);
				if(with_det) glColor3fv(detcl[other]);
				for(k = 0; k < 2; k++)
				{
					idx = i + k;	// We will use this vertex.
					if(!idx)
					{
						if(with_tex || with_det) 
							glTexCoord2f(tcS[other], tcT[side]);
						glVertex3f(vtx[other][VX], zPos[side], vtx[other][VY]);
					}
					else if(idx == cq->divs[other].num+1)
					{
						if(with_tex || with_det) 
							glTexCoord2f(tcS[other], tcT[other]);
						glVertex3f(vtx[other][VX], zPos[other], vtx[other][VY]);
					}
					else // A division vertex.
					{
						z = cq->divs[other].pos[idx - 1];
						if(with_tex || with_det) 
						{
							// Calculate the texture coordinate.
							glTexCoord2f(tcS[other], (z - cq->bottom) / 
								(cq->top - cq->bottom)
								* (tcT[0] - tcT[1]) + tcT[1]);
						}					
						glVertex3f(vtx[other][VX], z, vtx[other][VY]);
					}
				}
			}
		}
	}
	else
	{
		// Normal quads (GL_QUADS).
		if(with_col) glColor3ubv(rgb[0]);
		if(with_det) glColor3fv(detcl[0]);
		if(with_tex || with_det) glTexCoord2f(tcS[0], tcT[1]);
		glVertex3f(vtx[0][VX], cq->bottom, vtx[0][VY]);
		
		if(with_tex || with_det) glTexCoord2f(tcS[0], tcT[0]);
		glVertex3f(vtx[0][VX], cq->top, vtx[0][VY]);
		
		if(with_col) glColor3ubv(rgb[1]);
		if(with_det) glColor3fv(detcl[1]);
		if(with_tex || with_det) glTexCoord2f(tcS[1], tcT[0]);
		glVertex3f(vtx[1][VX], cq->top, vtx[1][VY]);
		
		if(with_tex || with_det) glTexCoord2f(tcS[1], tcT[1]);
		glVertex3f(vtx[1][VX], cq->bottom, vtx[1][VY]);

		polyCounter += 2;
	}
}

static void drawFlat(rendpoly_t *cq)
{
/*	if(cq->flags & RPF_MISSING_WALL)
	{
		float tcleft = 0, tctop = 0;
		float tcright, tcbottom, *v1, *v2;

		// This poly is REALLY a quad that originally had no texture. 
		// Render it as two triangles.
		if(with_tex)
		{
			tcright = cq->length/cq->texw;
			tcbottom = (cq->top - cq->bottom)/cq->texh;
		}

		v1 = cq->vertices[0].pos;
		v2 = cq->vertices[1].pos;
		
		if(with_col) glColor3ubv(cq->vertices[0].color.rgb);
		if(with_tex) 
		{
			glTexCoord2f(tcleft, tctop);
			//glMultiTexCoord2fARB(GL_TEXTURE1_ARB, tcleft, tctop);
		}
		glVertex3f(v1[VX], cq->top, v1[VY]);
		
		if(with_col) glColor3ubv(cq->vertices[1].color.rgb);
		if(with_tex) 
		{	
			glTexCoord2f(tcright, tctop);
			//glMultiTexCoord2fARB(GL_TEXTURE1_ARB, tcright, tctop);
		}
		glVertex3f(v2[VX], cq->top, v2[VY]);
		
		if(with_tex) 
		{
			glTexCoord2f(tcright, tcbottom);
			//glMultiTexCoord2fARB(GL_TEXTURE1_ARB, tcright, tcbottom);
		}
		glVertex3f(v2[VX], cq->bottom, v2[VY]);
		
		// The other triangle.
		if(with_tex) 
		{
			glTexCoord2f(tcright, tcbottom);
			//glMultiTexCoord2fARB(GL_TEXTURE1_ARB, tcright, tcbottom);
		}
		glVertex3f(v2[VX], cq->bottom, v2[VY]);
		
		if(with_col) glColor3ubv(cq->vertices[0].color.rgb);
		if(with_tex) 
		{
			glTexCoord2f(tcleft, tcbottom);
			//glMultiTexCoord2fARB(GL_TEXTURE1_ARB, tcleft, tcbottom);
		}
		glVertex3f(v1[VX], cq->bottom, v1[VY]);
		
		if(with_tex) 
		{
			glTexCoord2f(tcleft, tctop);
			//glMultiTexCoord2fARB(GL_TEXTURE1_ARB, tcleft, tctop);
		}
		glVertex3f(v1[VX], cq->top, v1[VY]);
	}
	else */
	
	if(cq->flags & RPF_LIGHT)
	{
#define FLATVTX_L(vtx)	glTexCoord2f((cq->texoffx - (vtx)->pos[VX])/cq->texw, \
							(cq->texoffy - (vtx)->pos[VY])/cq->texh); \
						glVertex3f((vtx)->pos[VX], cq->top, (vtx)->pos[VY]);

		rendpoly_vertex_t *first = cq->vertices, *vtx = first+1;
		int i = 1;

		// Render the polygon as a fan of triangles.
		for(; i < cq->numvertices-1; i++)
		{
			// All the triangles share the first vertex.
			FLATVTX_L(first);
			FLATVTX_L(vtx);
			vtx++;
			FLATVTX_L(vtx);
			polyCounter++;
		}
	}
	else
	{
#define FLATVTX(vtx)	if(with_col) glColor3ubv((vtx)->color.rgb); \
						if(with_tex) glTexCoord2f(((vtx)->pos[VX]+cq->texoffx)/cq->texw, \
							(-(vtx)->pos[VY]-cq->texoffy)/cq->texh); \
						if(with_det) { glColor3fv(detcl[(vtx)-cq->vertices]); \
							glTexCoord2f(((vtx)->pos[VX]+cq->texoffx)/DETAIL_TEX_SIZE*detailScale*dtex->scale, \
							(-(vtx)->pos[VY]-cq->texoffy)/DETAIL_TEX_SIZE*detailScale*dtex->scale); } \
						glVertex3f((vtx)->pos[VX], cq->top, (vtx)->pos[VY]);

		rendpoly_vertex_t *first = cq->vertices, *vtx = first+1;
		int i = 1;
		float df, detcl[DGL_MAX_POLY_SIDES][3];

		if(with_det)
		{
			int vi, c;
			// Calcuate the color for each vertex.
			for(vi = 0; vi < cq->numvertices; vi++)
			{
				df = DistFactor(cq, vi);
				for(c = 0; c < 3; c++)
				{
					detcl[vi][c] = cq->vertices[vi].color.rgb[c]/255.0f
						* df * detailFactor * dtex->strength;
				}
			}
		}

		// Render the polygon as a fan of triangles.
		for(; i < cq->numvertices-1; i++)
		{
			// All the triangles share the first vertex.
			FLATVTX(first);
			FLATVTX(vtx);
			vtx++;
			FLATVTX(vtx);
			polyCounter++;
		}
	}
}

void initForReading(rendlist_t *li)
{
	readlist = li;
	readlist->cursor = readlist->data;
}

INLINE byte readByte()
{
	return *readlist->cursor++;
}

INLINE short readShort()
{
	readlist->cursor += 2;
	return *(short*) (readlist->cursor-2);
}

INLINE int readLong()
{
	readlist->cursor += 4;
	return *(int*) (readlist->cursor-4);
}

INLINE float readFloat()
{
	readlist->cursor += 4;
	return *(float*) (readlist->cursor-4);
}

int readPoly(rendpoly_t *poly)
{
	int i, c, temp;
	rendpoly_vertex_t *vtx;

	poly->type = readByte();
	if(poly->type == rp_none) return DGL_FALSE;	// Time to stop.
	poly->flags = readByte();
	poly->texw = readShort();
	poly->texh = readShort();
	poly->texoffx = readFloat();
	poly->texoffy = readFloat();
	if(poly->flags & RPF_MASKED) poly->tex = readLong();
	poly->top = readFloat();
	if(poly->type == rp_quad || poly->type == rp_divquad) 
	{
		poly->bottom = readFloat();
		poly->length = readFloat();
		poly->numvertices = 2;
	}
	else
	{
		poly->numvertices = readByte();
	}
	for(i=0, vtx=poly->vertices; i<poly->numvertices; i++, vtx++)
	{
		vtx->pos[VX] = readFloat();
		vtx->pos[VY] = readFloat();
		for(c = 0; c < 3; c++)
		{
			temp = readByte();
			if(poly->flags & RPF_GLOW) 
			{
				temp = 255;
			}
			vtx->color.rgb[c] = temp;
		}
		if(poly->flags & RPF_DETAIL)
		{
			// A detail texture should be applied, so the distance to 
			// the vertex has been included.
			vtx->dist = readFloat();
		}
	}
	if(poly->type == rp_divquad)
	{
		for(i=0; i<2; i++)
		{
			poly->divs[i].num = readByte();
			for(c=0; c<poly->divs[i].num; c++)
				poly->divs[i].pos[c] = readFloat();
		}
	}
	return DGL_TRUE;
}

//===========================================================================
// doList
//	Draws the geometry of the list.
//===========================================================================
void doList(rendlist_t *rl, int mode)
{
	int flats;
	char divs = DGL_FALSE;
	rendpoly_t cq;

	with_tex = (mode & DOF_TEXCOORDS) != 0;
	with_col = (mode & DOF_COLORS) != 0;
	with_det = (mode & DOF_DETAIL) != 0;

	// Start reading through the list.
	initForReading(rl);
	flats = FLAT_LIST(rl);

	// Flats use triangles but walls are quads.
	glBegin(flats? GL_TRIANGLES : GL_QUADS);
	while(readPoly(&cq))
	{
		if(cq.type == rp_divquad) 
		{
			divs = DGL_TRUE;
			continue;
		}
		if(mode & DOF_JUST_DLIT && !(cq.flags & RPF_DLIT)) continue;
		if(mode & DOF_DETAIL && !(cq.flags & RPF_DETAIL)) continue;
		if(flats) drawFlat(&cq); else drawWall(&cq);
	}
	glEnd();
	
	// DivQuads are triangles.
	if(divs)
	{
		initForReading(rl);
		glBegin(GL_TRIANGLES);
		while(readPoly(&cq))
		{
			// Only draw the divquads.
			if(cq.type != rp_divquad) continue;
			if(mode & DOF_JUST_DLIT && !(cq.flags & RPF_DLIT)) continue;
			if(mode & DOF_DETAIL && !(cq.flags & RPF_DETAIL)) continue;
			drawWall(&cq);
		}
		glEnd();
	}
}

//===========================================================================
// renderList
//	This is only for solid, non-masked primitives.
//	This has become awfully complicated since this handles both walls and 
//	flats and supports divquads and dlit rendering. On top of everything 
//	else, after the list has been rendered, a detail pass is drawn for 
//	the quads that need it.
//===========================================================================
void renderList(rendlist_t *rl)
{
	rendpoly_t	cq;
	char		dlight = DGL_FALSE, divs = DGL_FALSE;
	char		details = DGL_FALSE;
	char		flats;

	if(LIST_EMPTY(rl)) return; // The list is empty.

	initForReading(rl);

/*	glActiveTextureARB(GL_TEXTURE1_ARB);
	
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
	
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
	
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 50);

	glActiveTextureARB(GL_TEXTURE0_ARB);*/

	// Disable alpha blending; some video cards think alpha zero is
	// is still translucent. And I guess things should render faster
	// with no blending...
	glDisable(GL_BLEND);	

	// Bind the right texture.
	glBindTexture(GL_TEXTURE_2D, currentTex = rl->tex);

	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	/*if(dlBlend || useFog)*/ zBias(NORMALBIAS);

	// Check what kind of primitives there are on the list.
	// There can only be one kind on each list.
	flats = FLAT_LIST(rl);

	with_tex = with_col = DGL_TRUE;
	with_det = DGL_FALSE;

	glBegin(flats? GL_TRIANGLES : GL_QUADS);
	while(readPoly(&cq))
	{
		if(cq.type == rp_divquad) divs = DGL_TRUE;
		if(cq.flags & RPF_DETAIL) details = DGL_TRUE;
		if(!dlBlend && cq.flags & RPF_DLIT && !useFog)
		{
			dlight = DGL_TRUE;
			continue;
		}
		if(cq.type == rp_divquad) continue;
		if(flats) drawFlat(&cq); else drawWall(&cq);
	}
	glEnd();
	// Need to draw some divided walls? They're drawn separately because
	// they're composed of triangles, not quads.
	if(divs)
	{
		initForReading(rl);
		glBegin(GL_TRIANGLES);
		while(readPoly(&cq))
		{
			// Only draw the divquads.
			if(!dlBlend && cq.flags & RPF_DLIT && !useFog) continue;
			if(cq.type == rp_divquad) drawWall(&cq);
		}
		glEnd();
	}

//	/*if(dlBlend || useFog)*/ zBias(0);

	// Need to draw some dlit polys? They're drawn blank, with no textures.
	// Dynlights are drawn on them and finally the textures (multiply-blend).
	if(dlight)
	{
		// Start a 2nd pass, of sorts.
//		zBias(2);
		glDisable(GL_TEXTURE_2D);
		doList(rl, DOF_JUST_DLIT | DOF_COLORS);
		glEnable(GL_TEXTURE_2D);
//		zBias(0);
	}

	// Enable alpha blending once more.
	glEnable(GL_BLEND);
	zBias(0);
}

void renderSkyMaskLists(rendlist_t *smrl, rendlist_t *skyw)
{
	rendpoly_t	cq;

	if(LIST_EMPTY(smrl) && LIST_EMPTY(skyw)) return; // Nothing to do here.

	// Instead of setting texture 0 we'll temporarily disable texturing.
	glDisable(GL_TEXTURE_2D);
	// This will effectively disable color buffer writes.
	glBlendFunc(GL_ZERO, GL_ONE);

	with_tex = with_col = with_det = DGL_FALSE;

	if(!LIST_EMPTY(smrl))
	{
		initForReading(smrl);
		glBegin(GL_TRIANGLES);
		while(readPoly(&cq)) drawFlat(&cq);
		glEnd();		
	}
	
	// Then the walls.
	if(!LIST_EMPTY(skyw))
	{
		initForReading(skyw);
		glBegin(GL_QUADS);
		while(readPoly(&cq)) drawWall(&cq);
		glEnd();
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
}

void renderDynLightLists(rendlist_t *frl, rendlist_t *wrl, rendlist_t *grl)
{
	rendpoly_t	cq;
	//GLfloat		projmat[16];

	//return;

	if(LIST_EMPTY(frl) && LIST_EMPTY(wrl) && LIST_EMPTY(grl)) 
		return; // Nothing to do.

	zBias(DYNLIGHTBIAS);

	// Adjust the projection matrix for Z bias.
	//setupZBias(projmat, 100);
	//glDepthRange(0, .99994f);

	// Setup the correct rendering state.	
	// Disable fog.
	if(useFog) glDisable(GL_FOG);

	// This'll allow multiple light quads to be rendered on top of each other.
	glDepthMask(GL_FALSE); 
	glDepthFunc(GL_LEQUAL);
	// Set up addition blending. The source is added to the destination.
	glBlendFunc(GL_ONE, GL_ONE);

	// The light texture.
	glBindTexture(GL_TEXTURE_2D, currentTex = lightTex);

	with_tex = DGL_TEXTURE;
	with_col = with_det = DGL_FALSE;

	// The flats.
	if(!LIST_EMPTY(frl))
	{
		initForReading(frl);
		glBegin(GL_TRIANGLES);
		while(readPoly(&cq))
		{
			// Set the color.
			glColor3ubv(cq.vertices[0].color.rgb);
			drawFlat(&cq);
		}
		glEnd();
	}	

	// The walls.
	if(!LIST_EMPTY(wrl))
	{
		float tctl[2], tcbr[2];	// Top left and bottom right.
		initForReading(wrl);
		glBegin(GL_QUADS);

		while(readPoly(&cq))
		{
			// Set the color.
			glColor3ubv(cq.vertices[0].color.rgb);

			// Calculate the texture coordinates.
			tcbr[VX] = (tctl[VX]=-cq.texoffx/cq.texw) + cq.length/cq.texw;
			tcbr[VY] = (tctl[VY]=cq.texoffy/cq.texh) + (cq.top - cq.bottom)/cq.texh;

			// The vertices.
			glTexCoord2f(tctl[VX], tcbr[VY]);
			glVertex3f(cq.vertices[0].pos[VX], cq.bottom, cq.vertices[0].pos[VY]);

			glTexCoord2f(tctl[VX], tctl[VY]);
			glVertex3f(cq.vertices[0].pos[VX], cq.top, cq.vertices[0].pos[VY]);

			glTexCoord2f(tcbr[VX], tctl[VY]);
			glVertex3f(cq.vertices[1].pos[VX], cq.top, cq.vertices[1].pos[VY]);

			glTexCoord2f(tcbr[VX], tcbr[VY]);
			glVertex3f(cq.vertices[1].pos[VX], cq.bottom, cq.vertices[1].pos[VY]);
		}
		glEnd();
	}

	// The glow dynlights (on walls only).
	if(!LIST_EMPTY(grl))
	{
		float yc[2];
		glBindTexture(GL_TEXTURE_2D, currentTex = glowTex);
		initForReading(grl);
		glBegin(GL_QUADS);
		while(readPoly(&cq))
		{
			// Set the color.
			glColor3ubv(cq.vertices[0].color.rgb);

			if(cq.texh > 0)
			{
				yc[0] = cq.texoffy/cq.texh;
				yc[1] = yc[0] + (cq.top - cq.bottom)/cq.texh;
			}
			else
			{
				yc[1] = -cq.texoffy/cq.texh;
				yc[0] = yc[1] - (cq.top - cq.bottom)/cq.texh;
			}

			glTexCoord2f(0, yc[1]);
			glVertex3f(cq.vertices[0].pos[VX], cq.bottom, cq.vertices[0].pos[VY]);

			glTexCoord2f(0, yc[0]);
			glVertex3f(cq.vertices[0].pos[VX], cq.top, cq.vertices[0].pos[VY]);

			glTexCoord2f(1, yc[0]);
			glVertex3f(cq.vertices[1].pos[VX], cq.top, cq.vertices[1].pos[VY]);

			glTexCoord2f(1, yc[1]);
			glVertex3f(cq.vertices[1].pos[VX], cq.bottom, cq.vertices[1].pos[VY]);
		}
		glEnd();
	}

	// Restore the normal rendering state.
	if(useFog) glEnable(GL_FOG);
	glDepthMask(GL_TRUE); 
	glDepthFunc(GL_LESS);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	zBias(0);
}

void renderDLitPass(rendlist_t *rl, int num)
{
	int			k;//, i;
//	GLfloat		fogColor[4];
	rendpoly_t	cq;
	char		flats, inited, divs;
	//GLfloat		projmat[16];

	if(useFog) return;

	// Adjust the projection matrix for Z bias.
	//setupZBias(projmat, 0);

	zBias(DLITBIAS);

	// This'll allow multiple light quads to be rendered on top of each other.
	//glDepthMask(GL_FALSE);	// No Z buffer writes.
	glDepthFunc(GL_LEQUAL);	
	// Multiply src and dest colors.
	glBlendFunc(GL_ZERO, GL_SRC_COLOR);

	// All textures are rendered fullbright.
	glColor3f(1, 1, 1);

	with_tex = DGL_TRUE;
	with_col = with_det = DGL_FALSE;

	for(k=0; k<num; k++, rl++)
	{
		if(LIST_EMPTY(rl)) continue;
		initForReading(rl);
		flats = FLAT_LIST(rl); 
		inited = DGL_FALSE;	// Only after the first dlit poly is found.
		divs = DGL_FALSE;
		while(readPoly(&cq))
		{
			if(!(cq.flags & RPF_DLIT)) continue;
			if(cq.type == rp_divquad) 
			{
				divs = DGL_TRUE;
				continue;
			}
			if(!inited) 
			{
				inited = DGL_TRUE;
				glBindTexture(GL_TEXTURE_2D, currentTex = rl->tex);
				glBegin(flats? GL_TRIANGLES : GL_QUADS);
			}
			if(flats) drawFlat(&cq); else drawWall(&cq);
		}
		// Need to draw some divs?
		if(divs)
		{
			if(!inited) // If not yet inited, do it now.
			{
				inited = DGL_TRUE;
				glBindTexture(GL_TEXTURE_2D, currentTex = rl->tex);
				glBegin(GL_TRIANGLES);
			}
			else if(!flats) // Otherwise change to GL_TRIANGLES.
			{
				glEnd();
				glBegin(GL_TRIANGLES);
			}
			initForReading(rl);
			while(readPoly(&cq))
			{
				// Only draw the divquads.
				if(!(cq.flags & RPF_DLIT) || cq.type != rp_divquad) continue;
				drawWall(&cq);
			}
			glEnd();
		}
		if(inited) glEnd();
	}

	// Restore the normal rendering state.
	glDepthFunc(GL_LESS);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	zBias(0);
}

// Requires ARB_texture_env_combine.
void renderDetailPass(rendlist_t *rl, int num)
{
	if(!useDetail || !ext_texenvcomb) return; // Details disabled.

	// Setup the proper texture environment and blending.
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_CONSTANT_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_PRIMARY_COLOR_ARB);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_ARB, GL_SRC_COLOR);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	glDepthFunc(GL_LEQUAL);
	zBias(DETAILBIAS);

	for(; num; num--, rl++)
	{
		if(LIST_EMPTY(rl)) continue;

		// First see if a detail texture has been assigned for the 
		// texture of this list.
		dtex = getDetail(rl->tex);
		if(!dtex) continue; // No details for this texture.

		// Bind and set as current.
		glBindTexture(GL_TEXTURE_2D, currentTex = dtex->detail);

		// Draw the geometry.
		doList(rl, DOF_DETAIL);	
	}

	// Restore normal state.
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(GL_LESS);
	zBias(0);
}