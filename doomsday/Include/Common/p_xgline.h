// Extended Generalized Line Types
#ifndef __XG_LINETYPE_H__
#define __XG_LINETYPE_H__

#include "xgclass.h"
//#include "p_mobj.h"

enum // Line events.
{
	XLE_CHAIN,
	XLE_CROSS,
	XLE_USE,
	XLE_SHOOT,
	XLE_HIT,
	XLE_TICKER
};

// Time conversion.
#define FLT2TIC(x)	( (int) ((x)*35) )
#define TIC2FLT(x)	( (x)/35.0f )

// Line type definition flags.
#define LTF_ACTIVE			0x00000001	// Active in the beginning of a level.

// Activation method. Line is activated if any of the following
// situations take place.
#define LTF_PLAYER_USE_A	0x00000002	// Player uses.
#define LTF_OTHER_USE_A		0x00000004	// Nonplayers can activate with use.
#define LTF_PLAYER_SHOOT_A	0x00000008	// Player shoots it.
#define LTF_OTHER_SHOOT_A	0x00000010	// Non-player thing shoots it.
#define LTF_ANY_CROSS_A		0x00000020	// Any mobj.
#define LTF_MONSTER_CROSS_A	0x00000040	// Flagged Countkill.
#define LTF_PLAYER_CROSS_A	0x00000080	// Player crosses the line.
#define LTF_MISSILE_CROSS_A	0x00000100	// Missile crosses the line.
#define LTF_PLAYER_HIT_A	0x00000200	// Player hits the line (walks into).
#define LTF_OTHER_HIT_A		0x00000400	// Nonplayer hits the line.
#define LTF_MONSTER_HIT_A	0x00000800	// Monster hits the line.
#define LTF_MISSILE_HIT_A	0x00001000	// Missile collides with the line.
#define LTF_ANY_HIT_A		0x00002000	// Any mobj collides with the line.

// Deactivating by colliding with the line.
#define LTF_PLAYER_USE_D	0x00004000	// Player uses.
#define LTF_OTHER_USE_D		0x00008000	// Nonplayers can activate with use.
#define LTF_PLAYER_SHOOT_D	0x00010000	// Player shoots it.
#define LTF_OTHER_SHOOT_D	0x00020000	// Non-player thing shoots it.
#define LTF_ANY_CROSS_D		0x00040000	// Any mobj.
#define LTF_MONSTER_CROSS_D 0x00080000	// Flagged Countkill.
#define LTF_PLAYER_CROSS_D	0x00100000	// Player crosses the line.
#define LTF_MISSILE_CROSS_D 0x00200000	// Missile crosses the line.
#define LTF_PLAYER_HIT_D	0x00400000	// Player hits the line (walks into).
#define LTF_OTHER_HIT_D		0x00800000	// Nonplayer hits the line.
#define LTF_MONSTER_HIT_D	0x01000000	// Monster hits the line.
#define LTF_MISSILE_HIT_D	0x02000000	// Missile collides with the line.
#define LTF_ANY_HIT_D		0x04000000	// Any mobj collides with the line.

// A+D activation methods. (A and D flags combined.)
#define LTF_PLAYER_USE		0x00004002	// Player uses.
#define LTF_OTHER_USE		0x00008004	// Non-player uses.
#define LTF_PLAYER_SHOOT	0x00010008	// Player shoots it.
#define LTF_OTHER_SHOOT		0x00020010	// Non-player thing shoots it.
#define LTF_ANY_CROSS		0x00040020	// Any mobj.
#define LTF_MONSTER_CROSS	0x00080040	// Flagged Countkill.
#define LTF_PLAYER_CROSS	0x00100080	// Player crosses the line.
#define LTF_MISSILE_CROSS	0x00200100	// Missile crosses the line.
#define LTF_PLAYER_HIT		0x00400200	// Player hits the line (walks into).
#define LTF_OTHER_HIT		0x00800400	// Nonplayer hits the line.
#define LTF_MONSTER_HIT		0x01000800	// Monster hits the line.
#define LTF_MISSILE_HIT		0x02001000	// Missile collides with the line.
#define LTF_ANY_HIT			0x04002000	// Any mobj collides with the line.

// Special activation methods/requirements.
#define LTF_TICKER_A			0x08000000	// Activate on ticker.
#define LTF_TICKER_D			0x10000000	// Deactivate on ticker.
#define LTF_TICKER				0x18000000	// A+D on ticker.
#define LTF_MOBJ_GONE			0x20000000	// a9 specifies mobj type
#define LTF_NO_OTHER_USE_SECRET 0x40000000	// Nonplr can't use line if secret
#define LTF_ACTIVATOR_TYPE		0x80000000	// a9 specifies mobj type

// When to do effect?
#define LTF2_WHEN_ACTIVATED		0x00000001
#define LTF2_WHEN_DEACTIVATED	0x00000002
#define LTF2_WHEN_ACTIVE		0x00000004
#define LTF2_WHEN_INACTIVE		0x00000008
#define LTF2_WHEN_LAST			0x00000010	// Only do effects when count=1.

// Activation requirements.
#define LTF2_KEY(n)			(1<<(5+n))
#define LTF2_KEY1			0x00000020
#define LTF2_KEY2			0x00000040
#define LTF2_KEY3			0x00000080
#define LTF2_KEY4			0x00000100
#define LTF2_KEY5			0x00000200
#define LTF2_KEY6			0x00000400
#define LTF2_LINE_ACTIVE	0x00000800	// line ref: a4 + a5
#define LTF2_LINE_INACTIVE	0x00001000	// line ref: a6 + a7
#define LTF2_COLOR			0x00002000	// a8: activator's color.

// Continued in flags2.
#define LTF2_HEALTH_ABOVE	0x00004000	// a0 (activator health)
#define LTF2_HEALTH_BELOW	0x00008000	// a1 
#define LTF2_POWER_ABOVE	0x00010000	// a2 (activator power)
#define LTF2_POWER_BELOW	0x00020000	// a3 
#define LTF2_SINGLEPLAYER	0x00040000
#define LTF2_COOPERATIVE	0x00080000
#define LTF2_DEATHMATCH		0x00100000
#define LTF2_ANY_MODE		0x001c0000	// Singleplayer, coop and DM combined.
#define LTF2_EASY			0x00200000
#define LTF2_MED			0x00400000
#define LTF2_HARD			0x00800000
#define LTF2_ANY_SKILL		0x00e00000	// Easy/med/hard combined.
#define LTF2_SKILL_SHIFT	21			// 1<<this == easy

// Extra features.
#define LTF2_MULTIPLE		0x01000000	// Copy act state to tagged lines
#define LTF2_TWOSIDED		0x02000000	// Allow use/shoot from both sides.
#define LTF2_GLOBAL_A_MSG	0x04000000	// Actmsg to all players.
#define LTF2_GLOBAL_D_MSG	0x08000000	// Deactmsg to all players.
#define LTF2_GLOBAL_MSG		0x0c000000	// A+D msg to all players.
#define LTF2_GROUP_ACT		0x10000000	// Act all tag-matching lines
#define LTF2_GROUP_DEACT	0x20000000	// Deact all tag-matching lines

#define LTACT_CNT_INFINITE	-1			// Activate infinite number of times.

enum // Activation types.
{
	// When on, count to off. Can be activated when off.
	LTACT_COUNTED_OFF,			

	// When off, count to on. Can be activated when on.
	LTACT_COUNTED_ON,			
	
	// Flip between on/off. Can be activated at any time.
	LTACT_FLIP,
	
	// When on, count to off. Can be (de)activated at any time.
	LTACT_FLIP_COUNTED_OFF,

	// When off, count to on. Can be (de)activated at any time.
	LTACT_FLIP_COUNTED_ON,
};

enum // Wall sections.
{
	LWS_NONE,
	LWS_MID,
	LWS_UPPER,
	LWS_LOWER
};

enum // Line reference type.
{
	LREF_SELF,
	LREF_TAGGED,
	LREF_LINE_TAGGED,
	LREF_ACT_TAGGED,
	LREF_INDEX,
	LREF_ALL
};

enum // Line -> Plane reference type.
{
	LPREF_NONE,

	LPREF_MY_FLOOR,
	LPREF_TAGGED_FLOORS,
	LPREF_LINE_TAGGED_FLOORS,
	LPREF_ACT_TAGGED_FLOORS,
	LPREF_INDEX_FLOOR,
	LPREF_ALL_FLOORS,

	LPREF_MY_CEILING,
	LPREF_TAGGED_CEILINGS,
	LPREF_LINE_TAGGED_CEILINGS,
	LPREF_ACT_TAGGED_CEILINGS,
	LPREF_INDEX_CEILING,
	LPREF_ALL_CEILINGS,

	LPREF_SPECIAL,		// 2nd param of reference treated in a special way.

	// Line -> Sector references (same as ->Plane, really).
	LSREF_MY = LPREF_MY_FLOOR,
	LSREF_TAGGED,
	LSREF_LINE_TAGGED,
	LSREF_ACT_TAGGED,
	LSREF_INDEX,
	LSREF_ALL
};

enum // Sector -> Plane reference type.
{
	SPREF_NONE,
	SPREF_MY_FLOOR,
	SPREF_MY_CEILING,
	SPREF_ORIGINAL_FLOOR,
	SPREF_ORIGINAL_CEILING,
	SPREF_CURRENT_FLOOR,
	SPREF_CURRENT_CEILING,
	SPREF_HIGHEST_FLOOR,
	SPREF_HIGHEST_CEILING,
	SPREF_LOWEST_FLOOR,
	SPREF_LOWEST_CEILING,
	SPREF_NEXT_HIGHEST_FLOOR,
	SPREF_NEXT_HIGHEST_CEILING,
	SPREF_NEXT_LOWEST_FLOOR,
	SPREF_NEXT_LOWEST_CEILING,
	SPREF_MIN_BOTTOM_TEXTURE,
	SPREF_MIN_MID_TEXTURE,
	SPREF_MIN_TOP_TEXTURE,
	SPREF_MAX_BOTTOM_TEXTURE,
	SPREF_MAX_MID_TEXTURE,
	SPREF_MAX_TOP_TEXTURE,
	SPREF_SECTOR_TAGGED_FLOOR,
	SPREF_LINE_TAGGED_FLOOR,
	SPREF_TAGGED_FLOOR,
	SPREF_ACT_TAGGED_FLOOR,
	SPREF_INDEX_FLOOR,
	SPREF_SECTOR_TAGGED_CEILING,
	SPREF_LINE_TAGGED_CEILING,
	SPREF_TAGGED_CEILING,
	SPREF_ACT_TAGGED_CEILING,
	SPREF_INDEX_CEILING
};

enum // Special lightlevel sources.
{
	LIGHTREF_NONE,
	LIGHTREF_MY,			// Actline's front sector.
	LIGHTREF_ORIGINAL,		// Original light level of the sector.
	LIGHTREF_CURRENT,		// Current light level of the sector.
	LIGHTREF_HIGHEST,		// Highest surrounding.
	LIGHTREF_LOWEST,		// Lowest surrounding.
	LIGHTREF_NEXT_HIGHEST,	// Next highest surrounding.
	LIGHTREF_NEXT_LOWEST	// Next lowest surrounding.
};

// Chain sequence flags.
#define CHSF_DEACTIVATE_WHEN_DONE	0x1
#define CHSF_LOOP					0x2

// State data for each line.
typedef struct
{
	linetype_t info;			// Type definition.	
	boolean active;
	boolean disabled;			// If true, skip all processing.
	int timer;
	int ticker_timer;
	void *activator;
	int idata;
	float fdata;
	int chidx;					// Chain sequence index.
	float chtimer;				// Chain sequence timer.
} xgline_t;
		
// Used as the activator if there is no real activator.
extern struct mobj_s dummything;

// Initialize extended lines for the map.
void XL_Init(void);

// Think for each extended line.
void XL_Ticker(void);

// Called when reseting engine state.
void XL_Update(void);

void XL_SetLineType(struct line_s *line, int id);

linetype_t *XL_GetType(int id);
int XL_LineEvent(int evtype, int linetype, struct line_s *line, int sidenum, 
				 void *data);
void XL_ActivateLine(boolean activating, linetype_t *info, struct line_s *line,
					 int sidenum, struct mobj_s *data);
int XL_TraverseLines(struct line_s *line, int reftype, int ref, int data, 
					 void *context, int (*func)(struct line_s *line, int data,
					 void *context));
int XL_TraversePlanes(struct line_s *line, int reftype, int ref, int data,
					  void *context, int (*func)(struct sector_s *sector, 
					  boolean ceiling, int data, void *context));

// Return false if the event was processed.
int XL_CrossLine(struct line_s *line, int sidenum, struct mobj_s *thing);
int XL_UseLine(struct line_s *line, int sidenum, struct mobj_s *thing);
int XL_ShootLine(struct line_s *line, int sidenum, struct mobj_s *thing);
int XL_HitLine(struct line_s *line, int sidenum, struct mobj_s *thing);

int XG_RandomInt(int min, int max);

void SV_WriteXGLine(struct line_s *li);
void SV_ReadXGLine(struct line_s *li);
void XL_UnArchiveLines(void);

#endif


