#ifndef __MY_BAMS_MATH_H__
#define __MY_BAMS_MATH_H__

#define BAMS_BITS	16

#if BAMS_BITS == 32

typedef unsigned long	binangle_t;

// Some predefined angles.
#define	BANG_0		0				// To the east.
#define BANG_45		0x20000000		// To the northeast.
#define BANG_90		0x40000000		// To the north.
#define BANG_135	0x60000000		// To the northwest.
#define BANG_180	0x80000000		// To the west.
#define BANG_225	0xa0000000		// To the southwest.
#define BANG_270	0xc0000000		// To the south.
#define BANG_315	0xe0000000		// To the southeast.
#define BANG_360	0x100000000		// Actually the same as angle 0.
#define BANG_MAX	0xffffffff		// The largest possible angle.

#elif BAMS_BITS == 16

typedef unsigned short	binangle_t;

// Some predefined angles.
#define	BANG_0		0			// To the east.
#define BANG_45		0x2000		// To the northeast.
#define BANG_90		0x4000		// To the north.
#define BANG_135	0x6000		// To the northwest.
#define BANG_180	0x8000		// To the west.
#define BANG_225	0xa000		// To the southwest.
#define BANG_270	0xc000		// To the south.
#define BANG_315	0xe000		// To the southeast.
#define BANG_360	0x10000		// Actually the same than angle 0.
#define BANG_MAX	0xffff		// The largest possible angle.

#else // Byte-sized.

typedef unsigned char	binangle_t;

// Some predefined angles.
#define	BANG_0		0			// To the east.
#define BANG_45		0x20		// To the northeast.
#define BANG_90		0x40		// To the north.
#define BANG_135	0x60		// To the northwest.
#define BANG_180	0x80		// To the west.
#define BANG_225	0xa0		// To the southwest.
#define BANG_270	0xc0		// To the south.
#define BANG_315	0xe0		// To the southeast.
#define BANG_360	0x100		// Actually the same than angle 0.
#define BANG_MAX	0xff		// The largest possible angle.

#endif

#define BAMS_PI				3.14159265359
#define RAD2BANG(x)			((binangle_t)((x)/BAMS_PI*BANG_180))
#define BANG2RAD(x)			((float)((x)/(float)BANG_180*BAMS_PI))
#define BANG2DEG(x)			((float)((x)/(float)BANG_180*180))

// Compass directions, for convenience.
#define BANG_EAST		BANG_0
#define BANG_NORTHEAST	BANG_45
#define BANG_NORTH		BANG_90
#define BANG_NORTHWEST	BANG_135
#define BANG_WEST		BANG_180
#define BANG_SOUTHWEST	BANG_225
#define BANG_SOUTH		BANG_270
#define BANG_SOUTHEAST	BANG_315

void bamsInit();	// Fill in the tables.
binangle_t bamsAtan2(int y,int x);

#endif