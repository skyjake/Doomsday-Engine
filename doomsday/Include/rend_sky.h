#ifndef __DOOMSDAY_SKY_H__
#define __DOOMSDAY_SKY_H__

// Sky hemispheres.
#define SKYHEMI_UPPER		0x1
#define SKYHEMI_LOWER		0x2
#define SKYHEMI_JUST_CAP	0x4	// Just draw the top or bottom cap.
#define SKYHEMI_FADEOUT_BG	0x8	// Draw the fadeout bg when drawing the cap.

typedef struct
{
	float	rgb[3];				// The RGB values.
	short	set, use;			// Is this set? Should be used?
	float	limit;				// .3 by default.
} fadeout_t;

// Sky layer flags.
#define SLF_ENABLED		0x1		// Layer enabled.
#define SLF_MASKED		0x2		// Mask the layer texture.

typedef struct
{
	int			flags;	
	int			texture;		// Not the OpenGL texture.
	float		offset;	
	fadeout_t	fadeout;
} skylayer_t;

extern int skyDetail, skyRows, simpleSky;
extern float skyDist;
extern int r_fullsky;

// Functions:
void Rend_InitSky();
void Rend_ShutdownSky();
void Rend_SkyDetail(int quarterDivs, int rows);
void Rend_SkyParams(int layer, int parm, float value);
void Rend_RenderSky(int hemis);

#endif
