
// AM_map.c

#include <stdio.h>

#include "Doomdef.h"
#include "P_local.h"
#include "Am_map.h"
#include "AM_data.h"
#include "settings.h"

#define LINEHEIGHT_A 10

vertex_t KeyPoints[NUMKEYS];

// Counter Cheat flags.
#define CCH_KILLS			0x1
#define CCH_ITEMS			0x2
#define CCH_SECRET			0x4
#define CCH_KILLS_PRCNT		0x8
#define CCH_ITEMS_PRCNT		0x10
#define CCH_SECRET_PRCNT	0x20

#define NUMALIAS 3 // Number of antialiased lines.

/*char LevelNames[][64] =
{
	// EPISODE 1 - THE CITY OF THE DAMNED
	"E1M1:  THE DOCKS",
 	"E1M2:  THE DUNGEONS",
 	"E1M3:  THE GATEHOUSE",
 	"E1M4:  THE GUARD TOWER",
 	"E1M5:  THE CITADEL",
 	"E1M6:  THE CATHEDRAL",
 	"E1M7:  THE CRYPTS",
 	"E1M8:  HELL'S MAW",
 	"E1M9:  THE GRAVEYARD",
	// EPISODE 2 - HELL'S MAW
	"E2M1:  THE CRATER",
 	"E2M2:  THE LAVA PITS",
 	"E2M3:  THE RIVER OF FIRE",
 	"E2M4:  THE ICE GROTTO",
 	"E2M5:  THE CATACOMBS",
 	"E2M6:  THE LABYRINTH",
 	"E2M7:  THE GREAT HALL",
 	"E2M8:  THE PORTALS OF CHAOS",
 	"E2M9:  THE GLACIER",
	// EPISODE 3 - THE DOME OF D'SPARIL
 	"E3M1:  THE STOREHOUSE",
 	"E3M2:  THE CESSPOOL",
 	"E3M3:  THE CONFLUENCE",
 	"E3M4:  THE AZURE FORTRESS",
 	"E3M5:  THE OPHIDIAN LAIR",
 	"E3M6:  THE HALLS OF FEAR",
 	"E3M7:  THE CHASM",
 	"E3M8:  D'SPARIL'S KEEP",
 	"E3M9:  THE AQUIFER",
	// EPISODE 4: THE OSSUARY
	"E4M1:  CATAFALQUE",
	"E4M2:  BLOCKHOUSE",
	"E4M3:  AMBULATORY",
	"E4M4:  SEPULCHER",
	"E4M5:  GREAT STAIR",
	"E4M6:  HALLS OF THE APOSTATE",
	"E4M7:  RAMPARTS OF PERDITION",
	"E4M8:  SHATTERED BRIDGE",
	"E4M9:  MAUSOLEUM",
	// EPISODE 5: THE STAGNANT DEMESNE
	"E5M1:  OCHRE CLIFFS",
	"E5M2:  RAPIDS",
	"E5M3:  QUAY",
	"E5M4:  COURTYARD",
	"E5M5:  HYDRATYR",
	"E5M6:  COLONNADE",
	"E5M7:  FOETID MANSE",
	"E5M8:  FIELD OF JUDGEMENT",
	"E5M9:  SKEIN OF D'SPARIL"
};*/

int	cheating = 0;
static int grid = 0;

static int leveljuststarted = 1; // kluge until AM_LevelInit() is called

boolean    automapactive = false;
static int finit_width = SCREENWIDTH;
static int finit_height = SCREENHEIGHT-42;
static int f_x, f_y; // location of window on screen
static int f_w, f_h; // size of window on screen
static int lightlev; // used for funky strobing effect
//static byte *fb; // pseudo-frame buffer
static int amclock;

static mpoint_t m_paninc; // how far the window pans each tic (map coords)
static fixed_t mtof_zoommul; // how far the window zooms in each tic (map coords)
static fixed_t ftom_zoommul; // how far the window zooms in each tic (fb coords)

static fixed_t m_x, m_y;   // LL x,y where the window is on the map (map coords)
static fixed_t m_x2, m_y2; // UR x,y where the window is on the map (map coords)

// width/height of window on map (map coords)
static fixed_t m_w, m_h;
static fixed_t min_x, min_y; // based on level size
static fixed_t max_x, max_y; // based on level size
static fixed_t max_w, max_h; // max_x-min_x, max_y-min_y
static fixed_t min_w, min_h; // based on player size
static fixed_t min_scale_mtof; // used to tell when to stop zooming out
static fixed_t max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static fixed_t old_m_w, old_m_h;
static fixed_t old_m_x, old_m_y;

// old location used by the Follower routine
static mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
static fixed_t scale_mtof = INITSCALEMTOF;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static fixed_t scale_ftom;

static player_t *plr; // the player represented by an arrow
static vertex_t oldplr;

//static patch_t *marknums[10]; // numbers used for marking by the automap
//static mpoint_t markpoints[AM_NUMMARKPOINTS]; // where the points are
//static int markpointnum = 0; // next point to be assigned

static int followplayer = 1; // specifies whether to follow the player around

static char cheat_amap[] = { 'r','a','v','m','a','p' };

static byte cheatcount=0;

extern boolean viewactive;

static int maplumpnum;
static short mapystart=0; // y-value for the start of the map
						  // bitmap...used in the paralax stuff.
static short mapxstart=0; //x-value for the bitmap.

// Functions

void DrawWuLine(int X0, int Y0, int X1, int Y1, byte *BaseColor,
	int NumLevels, unsigned short IntensityBits);

// Calculates the slope and slope according to the x-axis of a line
// segment in map coordinates (with the upright y-axis n' all) so
// that it can be used with the brain-dead drawing stuff.

// Ripped out for Heretic
/*
void AM_getIslope(mline_t *ml, islope_t *is)
{
  int dx, dy;

  dy = ml->a.y - ml->b.y;
  dx = ml->b.x - ml->a.x;
  if (!dy) is->islp = (dx<0?-MAXINT:MAXINT);
  else is->islp = FixedDiv(dx, dy);
  if (!dx) is->slp = (dy<0?-MAXINT:MAXINT);
  else is->slp = FixedDiv(dy, dx);
}
*/

void AM_activateNewScale(void)
{
  m_x += m_w/2;
  m_y += m_h/2;
  m_w = FTOM(f_w);
  m_h = FTOM(f_h);
  m_x -= m_w/2;
  m_y -= m_h/2;
  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;
}

void AM_saveScaleAndLoc(void)
{
  old_m_x = m_x;
  old_m_y = m_y;
  old_m_w = m_w;
  old_m_h = m_h;
}

void AM_restoreScaleAndLoc(void)
{

  m_w = old_m_w;
  m_h = old_m_h;
  if (!followplayer)
  {
    m_x = old_m_x;
    m_y = old_m_y;
  } else {
    m_x = plr->plr->mo->x - m_w/2;
    m_y = plr->plr->mo->y - m_h/2;
  }
  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;

  // Change the scaling multipliers
  scale_mtof = FixedDiv(f_w<<FRACBITS, m_w);
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

// adds a marker at the current location

/*
void AM_addMark(void)
{
  markpoints[markpointnum].x = m_x + m_w/2;
  markpoints[markpointnum].y = m_y + m_h/2;
  markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;

}
*/
void AM_findMinMaxBoundaries(void)
{
  int i;
  fixed_t a, b;

  min_x = min_y = MAXINT;
  max_x = max_y = -MAXINT;
  for (i=0;i<numvertexes;i++)
  {
    if (vertexes[i].x < min_x) min_x = vertexes[i].x;
    else if (vertexes[i].x > max_x) max_x = vertexes[i].x;
    if (vertexes[i].y < min_y) min_y = vertexes[i].y;
    else if (vertexes[i].y > max_y) max_y = vertexes[i].y;
  }
  max_w = max_x - min_x;
  max_h = max_y - min_y;
  min_w = 2*PLAYERRADIUS;
  min_h = 2*PLAYERRADIUS;

  a = FixedDiv(f_w<<FRACBITS, max_w);
  b = FixedDiv(f_h<<FRACBITS, max_h);
  min_scale_mtof = a < b ? a : b;

  max_scale_mtof = FixedDiv(f_h<<FRACBITS, 2*PLAYERRADIUS);

}

void AM_changeWindowLoc(void)
{
  if (m_paninc.x || m_paninc.y)
  {
    followplayer = 0;
    f_oldloc.x = MAXINT;
  }

  m_x += m_paninc.x;
  m_y += m_paninc.y;

  if (m_x + m_w/2 > max_x)
  {
  		m_x = max_x - m_w/2;
		m_paninc.x=0;
  }
  else if (m_x + m_w/2 < min_x)
  {
  		m_x = min_x - m_w/2;
		m_paninc.x=0;
  }
  if (m_y + m_h/2 > max_y)
  {
  		m_y = max_y - m_h/2;
		m_paninc.y=0;
  }
  else if (m_y + m_h/2 < min_y)
  {
  		m_y = min_y - m_h/2;
		m_paninc.y=0;
  }
/*
  mapxstart += MTOF(m_paninc.x+FRACUNIT/2);
  mapystart -= MTOF(m_paninc.y+FRACUNIT/2);
  if(mapxstart >= finit_width)
		mapxstart -= finit_width;
  if(mapxstart < 0)
		mapxstart += finit_width;
  if(mapystart >= finit_height)
		mapystart -= finit_height;
  if(mapystart < 0)
		mapystart += finit_height;
*/
  m_x2 = m_x + m_w;
  m_y2 = m_y + m_h;
}

void AM_initVariables(void)
{
  int pnum;
	thinker_t *think;
	mobj_t *mo;

  //static event_t st_notify = { ev_keyup, AM_MSGENTERED };

  automapactive = true;
//  fb = screen;

  f_oldloc.x = MAXINT;
  amclock = 0;
  lightlev = 0;

  m_paninc.x = m_paninc.y = 0;
  ftom_zoommul = FRACUNIT;
  mtof_zoommul = FRACUNIT;

  m_w = FTOM(f_w);
  m_h = FTOM(f_h);

  // find player to center on initially
  if (!players[pnum = consoleplayer].plr->ingame)
    for (pnum=0;pnum<MAXPLAYERS;pnum++) if (players[pnum].plr->ingame) break;
  plr = &players[pnum];
  oldplr.x = plr->plr->mo->x;
  oldplr.y = plr->plr->mo->y;
  m_x = plr->plr->mo->x - m_w/2;
  m_y = plr->plr->mo->y - m_h/2;
  AM_changeWindowLoc();

  // for saving & restoring
  old_m_x = m_x;
  old_m_y = m_y;
  old_m_w = m_w;
  old_m_h = m_h;

	// load in the location of keys, if in baby mode

	memset(KeyPoints, 0, sizeof(vertex_t)*3);
	if(gameskill == sk_baby)
	{
		for(think = thinkercap.next; think != &thinkercap; think = think->next)
		{
			if(think->function != P_MobjThinker)
			{ //not a mobj
				continue;
			}
			mo = (mobj_t *)think;
			if(mo->type == MT_CKEY)
			{
				KeyPoints[0].x = mo->x;
				KeyPoints[0].y = mo->y;
			}
			else if(mo->type == MT_AKYY)
			{
				KeyPoints[1].x = mo->x;
				KeyPoints[1].y = mo->y;
			}
			else if(mo->type == MT_BKYY)
			{
				KeyPoints[2].x = mo->x;
				KeyPoints[2].y = mo->y;
			}
		}
	}

  // inform the status bar of the change
//c  ST_Responder(&st_notify);
}

void AM_loadPics(void)
{
  //int i;
  //char namebuf[9];
/*  for (i=0;i<10;i++)
  {
    sprintf(namebuf, "AMMNUM%d", i);
    marknums[i] = W_CacheLumpName(namebuf, PU_STATIC);
  }*/
  maplumpnum = W_GetNumForName("AUTOPAGE");
}

/*void AM_unloadPics(void)
{
  int i;
  for (i=0;i<10;i++) Z_ChangeTag(marknums[i], PU_CACHE);

}*/

/*
void AM_clearMarks(void)
{
  int i;
  for (i=0;i<AM_NUMMARKPOINTS;i++) markpoints[i].x = -1; // means empty
  markpointnum = 0;
}
*/

// should be called at the start of every level
// right now, i figure it out myself

void AM_LevelInit(void)
{
  leveljuststarted = 0;

  f_x = f_y = 0;
  f_w = finit_width;
  f_h = finit_height;
	mapxstart = mapystart = 0;


//  AM_clearMarks();

  AM_findMinMaxBoundaries();
  scale_mtof = FixedDiv(min_scale_mtof, (int) (0.7*FRACUNIT));
  if (scale_mtof > max_scale_mtof) scale_mtof = min_scale_mtof;
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

static boolean stopped = true;

void AM_Stop (void)
{
  //static event_t st_notify = { 0, ev_keyup, AM_MSGEXITED };

//  AM_unloadPics();
	automapactive = false;
//  ST_Responder(&st_notify);
	stopped = true;
	//BorderNeedRefresh = true;
	GL_Update(DDUF_BORDER);
}

void AM_Start (void)
{
  static int lastlevel = -1, lastepisode = -1;

  if (!stopped) AM_Stop();
  stopped = false;
  if(gamestate != GS_LEVEL)
  {
		return; // don't show automap if we aren't in a game!
  }
  if (lastlevel != gamemap || lastepisode != gameepisode)
  {
    AM_LevelInit();
    lastlevel = gamemap;
    lastepisode = gameepisode;
  }
  AM_initVariables();
  AM_loadPics();
}

// set the window scale to the maximum size

void AM_minOutWindowScale(void)
{
  scale_mtof = min_scale_mtof;
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
  AM_activateNewScale();
}

// set the window scale to the minimum size

void AM_maxOutWindowScale(void)
{
  scale_mtof = max_scale_mtof;
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
  AM_activateNewScale();
}

boolean AM_Responder (event_t *ev)
{
	int rc;
	static int cheatstate=0;
	static int bigstate=0;
	
	rc = false;
	if (!automapactive)
	{
		if (ev->type == ev_keydown && ev->data1 == AM_STARTKEY
			&& gamestate == GS_LEVEL)
		{
			AM_Start ();
			viewactive = false;
			//      viewactive = true;
			rc = true;
		}
	}
	else if (ev->type == ev_keydown || ev->type == ev_keyrepeat)
	{
		rc = true;
		switch(ev->data1)
		{
		case AM_PANRIGHTKEY: // pan right
			if (!followplayer) m_paninc.x = FTOM(F_PANINC);
			else rc = false;
			break;
		case AM_PANLEFTKEY: // pan left
			if (!followplayer) m_paninc.x = -FTOM(F_PANINC);
			else rc = false;
			break;
		case AM_PANUPKEY: // pan up
			if (!followplayer) m_paninc.y = FTOM(F_PANINC);
			else rc = false;
			break;
		case AM_PANDOWNKEY: // pan down
			if (!followplayer) m_paninc.y = -FTOM(F_PANINC);
			else rc = false;
			break;
		case AM_ZOOMOUTKEY: // zoom out
			mtof_zoommul = M_ZOOMOUT;
			ftom_zoommul = M_ZOOMIN;
			break;
		case AM_ZOOMINKEY: // zoom in
			mtof_zoommul = M_ZOOMIN;
			ftom_zoommul = M_ZOOMOUT;
			break;
		case AM_ENDKEY:
			bigstate = 0;
			viewactive = true;
			AM_Stop ();
			break;
		case AM_GOBIGKEY:
			bigstate = !bigstate;
			if (bigstate)
			{
				AM_saveScaleAndLoc();
				AM_minOutWindowScale();
			}
			else AM_restoreScaleAndLoc();
			break;
		case AM_FOLLOWKEY:
			followplayer = !followplayer;
			f_oldloc.x = MAXINT;
			P_SetMessage(plr, followplayer ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF, true);
			break;
			/*
			case AM_GRIDKEY:
			grid = !grid;
			plr->message = grid ? AMSTR_GRIDON : AMSTR_GRIDOFF;
			break;
			case AM_MARKKEY:
			sprintf(buffer, "%s %d", AMSTR_MARKEDSPOT, markpointnum);
			plr->message = buffer;
			AM_addMark();
			break;
			case AM_CLEARMARKKEY:
			AM_clearMarks();
			plr->message = AMSTR_MARKSCLEARED;
			break;
			*/
		default:
			cheatstate=0;
			rc = false;
		}
		if(cheat_amap[cheatcount]==ev->data1 && !IS_NETGAME)
			cheatcount++;
		else
			cheatcount=0;
		if(cheatcount==6)
		{
			cheatcount=0;
			rc = false;
			cheating = (cheating+1) % 3;
		}
	}
	else if (ev->type == ev_keyup)
	{
		rc = false;
		switch (ev->data1)
		{
		case AM_PANRIGHTKEY:
			if (!followplayer) m_paninc.x = 0;
			break;
		case AM_PANLEFTKEY:
			if (!followplayer) m_paninc.x = 0;
			break;
		case AM_PANUPKEY:
			if (!followplayer) m_paninc.y = 0;
			break;
		case AM_PANDOWNKEY:
			if (!followplayer) m_paninc.y = 0;
			break;
		case AM_ZOOMOUTKEY:
		case AM_ZOOMINKEY:
			mtof_zoommul = FRACUNIT;
			ftom_zoommul = FRACUNIT;
			break;
		}
	}
	
	return rc;
	
}

void AM_changeWindowScale(void)
{

  // Change the scaling multipliers
  scale_mtof = FixedMul(scale_mtof, mtof_zoommul);
  scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

  if (scale_mtof < min_scale_mtof) AM_minOutWindowScale();
  else if (scale_mtof > max_scale_mtof) AM_maxOutWindowScale();
  else AM_activateNewScale();
}

void AM_doFollowPlayer(void)
{
  if (f_oldloc.x != plr->plr->mo->x || f_oldloc.y != plr->plr->mo->y)
  {
//  m_x = FTOM(MTOF(plr->mo->x - m_w/2));
//  m_y = FTOM(MTOF(plr->mo->y - m_h/2));
//  m_x = plr->mo->x - m_w/2;
//  m_y = plr->mo->y - m_h/2;
    m_x = FTOM(MTOF(plr->plr->mo->x)) - m_w/2;
    m_y = FTOM(MTOF(plr->plr->mo->y)) - m_h/2;
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;

  	 // do the parallax parchment scrolling.
/*
	 dmapx = (MTOF(plr->mo->x)-MTOF(f_oldloc.x)); //fixed point
	 dmapy = (MTOF(f_oldloc.y)-MTOF(plr->mo->y));

	 if(f_oldloc.x == MAXINT) //to eliminate an error when the user first
		dmapx=0;  //goes into the automap.
	 mapxstart += dmapx;
	 mapystart += dmapy;

  	 while(mapxstart >= finit_width)
			mapxstart -= finit_width;
    while(mapxstart < 0)
			mapxstart += finit_width;
    while(mapystart >= finit_height)
			mapystart -= finit_height;
    while(mapystart < 0)
			mapystart += finit_height;
*/
	 f_oldloc.x = plr->plr->mo->x;
    f_oldloc.y = plr->plr->mo->y;
  }
}

// Ripped out for Heretic
/*
void AM_updateLightLev(void)
{
  static nexttic = 0;
//static int litelevels[] = { 0, 3, 5, 6, 6, 7, 7, 7 };
  static int litelevels[] = { 0, 4, 7, 10, 12, 14, 15, 15 };
  static int litelevelscnt = 0;

  // Change light level
  if (amclock>nexttic)
  {
    lightlev = litelevels[litelevelscnt++];
    if (litelevelscnt == sizeof(litelevels)/sizeof(int)) litelevelscnt = 0;
    nexttic = amclock + 6 - (amclock % 6);
  }
}
*/

void AM_Ticker (void)
{

  if (!automapactive) return;

  amclock++;

  if (followplayer) AM_doFollowPlayer();

  // Change the zoom if necessary
  if (ftom_zoommul != FRACUNIT) AM_changeWindowScale();

  // Change x,y location
  if (m_paninc.x || m_paninc.y) AM_changeWindowLoc();
  // Update light level
// AM_updateLightLev();

}

void AM_clearFB(int color)
{
	int /*i, j, */lump;
	int dmapx;
	int dmapy;
	float scaler;

	if(followplayer)
	{
		dmapx = (MTOF(plr->plr->mo->x)-MTOF(oldplr.x)); //fixed point
		dmapy = (MTOF(oldplr.y)-MTOF(plr->plr->mo->y));

		oldplr.x = plr->plr->mo->x;
		oldplr.y = plr->plr->mo->y;
//		if(f_oldloc.x == MAXINT) //to eliminate an error when the user first
//			dmapx=0;  //goes into the automap.
		mapxstart += dmapx>>1;
		mapystart += dmapy>>1;

	  	while(mapxstart >= finit_width)
			mapxstart -= finit_width;
	   while(mapxstart < 0)
			mapxstart += finit_width;
	   while(mapystart >= finit_height)
			mapystart -= finit_height;
	   while(mapystart < 0)
			mapystart += finit_height;
	}
	else
	{
		mapxstart += (MTOF(m_paninc.x)>>1);
		mapystart -= (MTOF(m_paninc.y)>>1);
		if(mapxstart >= finit_width)
			mapxstart -= finit_width;
		if(mapxstart < 0)
			mapxstart += finit_width;
		if(mapystart >= finit_height)
		mapystart -= finit_height;
		if(mapystart < 0)
		mapystart += finit_height;
	}

	//blit the automap background to the screen.
/*	j=mapystart*finit_width;
	for(i=0;i<finit_height;i++)
	{
		memcpy(screen+i*finit_width, maplump+j+mapxstart, finit_width-mapxstart);
		memcpy(screen+i*finit_width+finit_width-mapxstart, maplump+j, mapxstart);
		j += finit_width;
		if(j >= finit_height*finit_width)
			j=0;
	}*/

	GL_SetColorAndAlpha(1, 1, 1, 1);
	
	GL_SetFlat(R_FlatNumForName("FLOOR04"));
	scaler = cfg.sbarscale/20.0f;
	GL_DrawCutRectTiled(0, finit_height+4, 320, 200-finit_height-4, 64, 64, 
		160-160*scaler+1, finit_height, 320*scaler-2, 200-finit_height);

	GL_SetPatch(lump=W_GetNumForName("bordb"));
	GL_DrawCutRectTiled(0, finit_height, 320, 4, 
		16, 4,
		160-160*scaler+1, finit_height, 320*scaler-2, 4);

	GL_SetRawImage(maplumpnum, 0);	// We only want the left portion.
	GL_DrawRectTiled(0, 0, finit_width, finit_height, 128, 100);
}

void AM_drawMline(mline_t *ml, int color)
{
	GL_SetColor(color);
	gl.Vertex2f(FIX2FLT(CXMTOFX(ml->a.x)), FIX2FLT(CYMTOFX(ml->a.y))/1.2);
	gl.Vertex2f(FIX2FLT(CXMTOFX(ml->b.x)), FIX2FLT(CYMTOFX(ml->b.y))/1.2);
}

void AM_drawGrid(int color)
{
/*  fixed_t x, y;
  fixed_t start, end;
  mline_t ml;
  
  gl.Begin(DGL_LINES);

  // Figure out start of vertical gridlines
  start = m_x;
  if ((start-bmaporgx)%(MAPBLOCKUNITS<<FRACBITS))
    start += (MAPBLOCKUNITS<<FRACBITS)
      - ((start-bmaporgx)%(MAPBLOCKUNITS<<FRACBITS));
  end = m_x + m_w;

  // draw vertical gridlines
  ml.a.y = m_y;
  ml.b.y = m_y+m_h;
  for (x=start; x<end; x+=(MAPBLOCKUNITS<<FRACBITS))
  {
    ml.a.x = x;
    ml.b.x = x;
    AM_drawMline(&ml, color);
  }

  // Figure out start of horizontal gridlines
  start = m_y;
  if ((start-bmaporgy)%(MAPBLOCKUNITS<<FRACBITS))
    start += (MAPBLOCKUNITS<<FRACBITS)
      - ((start-bmaporgy)%(MAPBLOCKUNITS<<FRACBITS));
  end = m_y + m_h;

  // draw horizontal gridlines
  ml.a.x = m_x;
  ml.b.x = m_x + m_w;
  for (y=start; y<end; y+=(MAPBLOCKUNITS<<FRACBITS))
  {
    ml.a.y = y;
    ml.b.y = y;
    AM_drawMline(&ml, color);
  }
  gl.End();*/
}

void AM_drawWalls(void)
{
	int i;
	static mline_t l;

	gl.Begin(DGL_LINES);
	for(i=0; i<numlines; i++)
	{
		l.a.x = lines[i].v1->x;
		l.a.y = lines[i].v1->y;
		l.b.x = lines[i].v2->x;
		l.b.y = lines[i].v2->y;
		if (cheating || (lines[i].flags & ML_MAPPED))
		{
			if ((lines[i].flags & LINE_NEVERSEE) && !cheating)
				continue;
			if (!lines[i].backsector)
			{	
				AM_drawMline(&l, WALLCOLORS+lightlev);
			} 
			else 
			{
				if(lines[i].special == 39)
				{ // teleporters
					AM_drawMline(&l, WALLCOLORS+WALLRANGE/2);
				} 
				else if (lines[i].flags & ML_SECRET) // secret door
				{
					if (cheating) AM_drawMline(&l, 0);
					else AM_drawMline(&l, WALLCOLORS+lightlev);
				}
				else if(lines[i].special > 25 && lines[i].special < 35)
				{
					switch(lines[i].special)
					{
					case 26:
					case 32:
						AM_drawMline(&l, BLUEKEY);
						break;
					case 27:
					case 34:
						AM_drawMline(&l, YELLOWKEY);
						break;
					case 28:
					case 33:
						AM_drawMline(&l, GREENKEY);
						break;
					default:
						break;
					}
				}
				else if (lines[i].backsector->floorheight != lines[i].frontsector->floorheight) 
				{
					AM_drawMline(&l, FDWALLCOLORS + lightlev); // floor level change
				} 
				else if (lines[i].backsector->ceilingheight != lines[i].frontsector->ceilingheight) 
				{
					AM_drawMline(&l, CDWALLCOLORS+lightlev); // ceiling level change
				} 
				else if (cheating) 
				{
					AM_drawMline(&l, TSWALLCOLORS+lightlev);
				}
			}
		} 
		else if (plr->powers[pw_allmap])
		{
			if(!(lines[i].flags & LINE_NEVERSEE)) AM_drawMline(&l, GRAYS+3);
		}
	}
	gl.End();
}

void AM_rotate(fixed_t *x, fixed_t *y, angle_t a)
{
  fixed_t tmpx;

  tmpx = FixedMul(*x,finecosine[a>>ANGLETOFINESHIFT])
       - FixedMul(*y,finesine[a>>ANGLETOFINESHIFT]);
  *y   = FixedMul(*x,finesine[a>>ANGLETOFINESHIFT])
       + FixedMul(*y,finecosine[a>>ANGLETOFINESHIFT]);
  *x = tmpx;
}

void AM_drawLineCharacter(mline_t *lineguy, int lineguylines, fixed_t scale,
  angle_t angle, int color, fixed_t x, fixed_t y)
{
  int i;
  mline_t l;

  for (i=0;i<lineguylines;i++)
  {
    l.a.x = lineguy[i].a.x;
    l.a.y = lineguy[i].a.y;
    if (scale)
    {
      l.a.x = FixedMul(scale, l.a.x);
      l.a.y = FixedMul(scale, l.a.y);
    }
    if (angle) AM_rotate(&l.a.x, &l.a.y, angle);
    l.a.x += x;
    l.a.y += y;

    l.b.x = lineguy[i].b.x;
    l.b.y = lineguy[i].b.y;
    if (scale)
    {
      l.b.x = FixedMul(scale, l.b.x);
      l.b.y = FixedMul(scale, l.b.y);
    }
    if (angle) AM_rotate(&l.b.x, &l.b.y, angle);
    l.b.x += x;
    l.b.y += y;

    AM_drawMline(&l, color);
  }

}

void AM_drawPlayers(void)
{
	int i;
	player_t *p;
	static int their_colors[] = { GREENKEY, YELLOWKEY, BLOODRED, BLUEKEY };
	//int their_color = -1;
	int color;

	if (!IS_NETGAME)
	{
		/*
		if (cheating) AM_drawLineCharacter(cheat_player_arrow, NUMCHEATPLYRLINES, 0,
		plr->mo->angle, WHITE, plr->mo->x, plr->mo->y);
		*/ //cheat key player pointer is the same as non-cheat pointer..
	
		gl.Begin(DGL_LINES);
		AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0, 
			plr->plr->clAngle,//mo->angle,
			WHITE, plr->plr->mo->x, plr->plr->mo->y);
		gl.End();
		return;
	}

	gl.Begin(DGL_LINES);
	for(i=0; i<MAXPLAYERS; i++)
	{
//		their_color++;
		p = &players[i];
		if(deathmatch && !singledemo && p != plr) continue;
	    if(!players[i].plr->ingame) continue;
		if(p->powers[pw_invisibility]) 
			color = 102; // *close* to the automap color
		else 
			color = their_colors[/*their_color*/ cfg.PlayerColor[i] ];
		
		AM_drawLineCharacter(player_arrow, NUMPLYRLINES, 0, 
			i==consoleplayer? p->plr->clAngle : p->plr->mo->angle,
			color, p->plr->mo->x, p->plr->mo->y);
	}
	gl.End();
}

void AM_drawThings(int colors, int colorrange)
{
	int i;
	mobj_t *t;

	gl.Begin(DGL_LINES);
	for(i=0; i<numsectors; i++)
	{
		t = sectors[i].thinglist;
		while (t)
		{
			AM_drawLineCharacter(thintriangle_guy, NUMTHINTRIANGLEGUYLINES,
				16<<FRACBITS, t->angle, colors+lightlev, t->x, t->y);
			t = t->snext;
		}
	}
	gl.End();
}

/*
void AM_drawMarks(void)
{
  int i, fx, fy, w, h;

  for (i=0;i<AM_NUMMARKPOINTS;i++)
  {
    if (markpoints[i].x != -1)
    {
      w = SHORT(marknums[i]->width);
      h = SHORT(marknums[i]->height);
      fx = CXMTOF(markpoints[i].x);
      fy = CYMTOF(markpoints[i].y);
      if (fx >= f_x && fx <= f_w - w && fy >= f_y && fy <= f_h - h)
  			V_DrawPatch(fx, fy, marknums[i]);
    }
  }
}
*/

void AM_drawkeys(void)
{
	gl.Begin(DGL_LINES);
	if(KeyPoints[0].x != 0 || KeyPoints[0].y != 0)
	{
		AM_drawLineCharacter(keysquare, NUMKEYSQUARELINES, 0, 0, YELLOWKEY,
			KeyPoints[0].x, KeyPoints[0].y);
	}
	if(KeyPoints[1].x != 0 || KeyPoints[1].y != 0)
	{
		AM_drawLineCharacter(keysquare, NUMKEYSQUARELINES, 0, 0, GREENKEY,
			KeyPoints[1].x, KeyPoints[1].y);
	}
	if(KeyPoints[2].x != 0 || KeyPoints[2].y != 0)
	{
		AM_drawLineCharacter(keysquare, NUMKEYSQUARELINES, 0, 0, BLUEKEY,
			KeyPoints[2].x, KeyPoints[2].y);
	}
	gl.End();
}

void AM_drawCrosshair(int color)
{
//	fb[(f_w*(f_h+1))/2] = color; // single point for now
}

int scissorState[5];

void AM_OGL_SetupState()
{
	int scrwidth = Get(DD_SCREEN_WIDTH);
	int scrheight = Get(DD_SCREEN_HEIGHT);
	float /*xs = scrwidth/320.0f,*/ ys = scrheight/200.0f;

	// Let's set up a scissor box to clip the map lines and stuff.
	// Store the old scissor state.
	gl.GetIntegerv(DGL_SCISSOR_TEST, scissorState);
	gl.GetIntegerv(DGL_SCISSOR_BOX, scissorState+1);
	gl.Scissor(0, 0, scrwidth, finit_height*ys);
	gl.Enable(DGL_SCISSOR_TEST);
}

void AM_OGL_RestoreState()
{
	if(!scissorState[0]) gl.Disable(DGL_SCISSOR_TEST);
	gl.Scissor(scissorState[1], scissorState[2], scissorState[3], scissorState[4]);
}

void AM_drawCounters(void)
{
	float scale = cfg.counterCheatScale;
	int x = 1/scale, y = LINEHEIGHT_A*3/2/scale;
	char buf[40], tmp[20];
	
	gl.Color3f(1, 1, 1);

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.Scalef(scale, scale, 1);

	// Kills.
	if(cfg.counterCheat & (CCH_KILLS | CCH_KILLS_PRCNT))
	{
		strcpy(buf, "Kills: ");
		if(cfg.counterCheat & CCH_KILLS)
		{
			sprintf(tmp, "%i/%i ", plr->killcount, totalkills);
			strcat(buf, tmp);
		}
		if(cfg.counterCheat & CCH_KILLS_PRCNT)
		{
			sprintf(tmp, "%s%i%%%s", 
				cfg.counterCheat & CCH_KILLS? "(" : "",
				totalkills? plr->killcount*100/totalkills
				: 100,
				cfg.counterCheat & CCH_KILLS? ")" : "");
			strcat(buf, tmp);
		}
		MN_DrTextA_CS(buf, x, y);
		y += LINEHEIGHT_A;
	}
	// Items.
	if(cfg.counterCheat & (CCH_ITEMS | CCH_ITEMS_PRCNT))
	{
		strcpy(buf, "Items: ");
		if(cfg.counterCheat & CCH_ITEMS)
		{
			sprintf(tmp, "%i/%i ", plr->itemcount, totalitems);
			strcat(buf, tmp);
		}
		if(cfg.counterCheat & CCH_ITEMS_PRCNT)
		{
			sprintf(tmp, "%s%i%%%s", 
				cfg.counterCheat & CCH_ITEMS? "(" : "",
				totalitems? plr->itemcount*100/totalitems
				: 100,
				cfg.counterCheat & CCH_ITEMS? ")" : "");
			strcat(buf, tmp);
		}
		MN_DrTextA_CS(buf, x, y);
		y += LINEHEIGHT_A;
	}
	// Secrets.
	if(cfg.counterCheat & (CCH_SECRET | CCH_SECRET_PRCNT))
	{
		strcpy(buf, "Secret: ");
		if(cfg.counterCheat & CCH_SECRET)
		{
			sprintf(tmp, "%i/%i ", plr->secretcount, totalsecret);
			strcat(buf, tmp);
		}
		if(cfg.counterCheat & CCH_SECRET_PRCNT)
		{
			sprintf(tmp, "%s%i%%%s", 
				cfg.counterCheat & CCH_SECRET? "(" : "",
				totalsecret? plr->secretcount*100/totalsecret
				: 100,
				cfg.counterCheat & CCH_SECRET? ")" : "");
			strcat(buf, tmp);
		}
		MN_DrTextA_CS(buf, x, y);
		y += LINEHEIGHT_A;
	}

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
}

void AM_Drawer(void)
{
//	int highestEpisode;

	if (!automapactive) return;

	finit_height = SCREENHEIGHT - SBARHEIGHT*cfg.sbarscale/20;

	GL_Update(DDUF_FULLSCREEN);
	AM_clearFB(BACKGROUND);
	
	AM_OGL_SetupState();
	gl.Disable(DGL_TEXTURING);

	if (grid) AM_drawGrid(GRIDCOLORS);
	AM_drawWalls();
	AM_drawPlayers();
	if (cheating==2) AM_drawThings(THINGCOLORS, THINGRANGE);
//  AM_drawCrosshair(XHAIRCOLORS);

//  AM_drawMarks();
	if(gameskill == sk_baby)
	{
		AM_drawkeys();
	}
	gl.Enable(DGL_TEXTURING);
	if((gameepisode < (ExtendedWAD ? 6 : 4)) && gamemap < 10)
	{
		char *name = P_GetLevelName(gameepisode, gamemap);
		MN_DrTextA(name, (320 - MN_TextAWidth(name))/2, 
			145 + SBARHEIGHT*(20 - cfg.sbarscale)/20);
	}

	AM_OGL_RestoreState();

	if(cfg.counterCheat) AM_drawCounters();
}


