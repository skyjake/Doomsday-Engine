
//**************************************************************************
//**
//** HU_MSG.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "hu_stuff.h"
#include "d_config.h"
#include "m_menu.h"
#include "Mn_def.h"

// MACROS ------------------------------------------------------------------

#define MAX_MESSAGES	8
#define MAX_LINELEN		140
#define IN_RANGE(x)		((x)>=MAX_MESSAGES? (x)-MAX_MESSAGES : (x)<0? (x)+MAX_MESSAGES : (x))

// TYPES -------------------------------------------------------------------

typedef struct
{
	char text[MAX_LINELEN];
	int time;
} 
message_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static message_t messages[MAX_MESSAGES];
static int firstmsg, lastmsg, msgcount = 0;
static float yoffset = 0; // Scroll-up offset.

// CODE --------------------------------------------------------------------

//===========================================================================
// HUMsg_Clear
//	Clears the message buffer.
//===========================================================================
void HUMsg_Clear(void)
{
	// The message display is empty.
	firstmsg = lastmsg = 0;
	msgcount = 0;
}

//===========================================================================
// HUMsg_Message
//	Add a new message.
//===========================================================================
void HUMsg_Message(char *msg)
{
	messages[lastmsg].time = cfg.msgUptime; 
	strcpy(messages[lastmsg].text, msg);
	lastmsg = IN_RANGE(lastmsg + 1);
	if(msgcount == MAX_MESSAGES)
		firstmsg = lastmsg;
	else if(msgcount == cfg.msgCount)
		firstmsg = IN_RANGE(firstmsg + 1);
	else
		msgcount++;
}

//===========================================================================
// HUMsg_DropLast
//	Removes the oldest message.
//===========================================================================
void HUMsg_DropLast(void)
{
	if(!msgcount) return;
	firstmsg = IN_RANGE(firstmsg + 1);
	if(messages[firstmsg].time < 10) messages[firstmsg].time = 10;
	msgcount--;
}

//===========================================================================
// HUMsg_Drawer
//===========================================================================
void HUMsg_Drawer(void)
{
	int m, num, y, lh = LINEHEIGHT_A, td;
	float col[4];

	// Apply a scaling.
	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.Scalef(cfg.msgScale, cfg.msgScale, 1);
	gl.Translatef(0, -yoffset, 0);

	// How many messages should we print?
	num = msgcount;

	// First 'num' messages starting from the last one.
	for(y = (num-1)*lh, m = IN_RANGE(lastmsg - 1); num; y -= lh, num--,
		m = IN_RANGE(m-1))
	{
		td = cfg.msgUptime - messages[m].time;
		col[3] = 1;
		if(td < 6 && td & 2 && cfg.msgBlink)
		{
			// Flash?
			col[0] = col[1] = col[2] = 1;
		}
		else 
		{
			if(m == firstmsg && messages[m].time <= LINEHEIGHT_A) 
				col[3] = messages[m].time/(float)LINEHEIGHT_A * 0.9f;
			// Use normal HUD color.
			memcpy(col, cfg.hudColor, sizeof(cfg.hudColor));
		}
		gl.Color4fv(col);
		M_WriteText2(1, 1 + y, messages[m].text, hu_font_a, -1, -1, -1);
	}

	gl.PopMatrix();
}

//===========================================================================
// HUMsg_Ticker
//===========================================================================
void HUMsg_Ticker(void)
{
	int i;

	// Countdown to scroll-up.
	for(i = 0; i < MAX_MESSAGES; i++) messages[i].time--;
	if(msgcount)
	{
		yoffset = 0;
		if(messages[firstmsg].time >= 0 
			&& messages[firstmsg].time <= LINEHEIGHT_A)
		{
			yoffset = LINEHEIGHT_A - messages[firstmsg].time;												
		}
		else if(messages[firstmsg].time < 0) HUMsg_DropLast();
	}
}

