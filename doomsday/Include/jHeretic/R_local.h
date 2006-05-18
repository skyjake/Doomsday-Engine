// R_local.h

#ifndef __R_LOCAL__
#define __R_LOCAL__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#define ANGLETOSKYSHIFT     22     // sky map is 256*128*4 maps

#define BASEYCENTER         100

#define MAXWIDTH            1120
#define MAXHEIGHT           832

#define PI                  3.141592657

//
// lighting constants
//
#define LIGHTLEVELS         16
#define LIGHTSEGSHIFT       4
#define MAXLIGHTSCALE       48
#define LIGHTSCALESHIFT     12
#define MAXLIGHTZ           128
#define LIGHTZSHIFT         20
#define NUMCOLORMAPS        32     // number of diminishing
#define INVERSECOLORMAP     32

extern int      centerx, centery;
extern int      flyheight;



#endif
