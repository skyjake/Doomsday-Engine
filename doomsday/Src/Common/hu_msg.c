// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
//
// DESCRIPTION:  heads-up text and input code
//
//		Compiles for jDoom, jHeretic, jHexen
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#ifdef __JDOOM__
# include "../jDoom/doomdef.h"
# include "../jDoom/d_config.h"
# include "../jDoom/m_menu.h"
# include "../jDoom/Mn_def.h"
# include "../jDoom/m_misc.h"
# include "../jDoom/s_sound.h"
# include "../jDoom/doomstat.h"
# include "../jDoom/r_local.h"
# include "../jDoom/p_local.h"
# include "../jDoom/dstrings.h"  // Data.
#elif __JHERETIC__
# include "../jHeretic/doomdef.h"
# include "../jHeretic/d_config.h"
# include "../jHeretic/mn_def.h"
# include "../jHeretic/s_sound.h"
# include "../jHeretic/Doomdata.h"
# include "../jHeretic/R_local.h"
# include "../jHeretic/p_local.h"
# include "../jHeretic/Dstrings.h"  // Data.
#elif __JHEXEN__
# include "../jHexen/h2def.h"
# include "../jHexen/d_config.h"
# include "../jHexen/mn_def.h"
# include "../jHexen/sounds.h"
# include "../jHexen/r_local.h"
# include "../jHexen/p_local.h"
# include "../jHexen/textdefs.h"  // Data.
#elif __JSTRIFE__
# include "../jStrife/h2def.h"
# include "../jStrife/d_config.h"
# include "../jStrife/mn_def.h"
# include "../jStrife/sounds.h"
# include "../jStrife/r_local.h"
# include "../jStrife/p_local.h"
# include "../jStrife/textdefs.h"  // Data.
#endif

#include "../Common/hu_stuff.h"
#include "../Common/hu_msg.h"
#include "../Common/hu_lib.h"

// MACROS ------------------------------------------------------------------

#define HU_INPUTTOGGLE	't'
#define HU_INPUTX	HU_MSGX
#define HU_INPUTY	(HU_MSGY + HU_MSGHEIGHT*(SHORT(hu_font[0].height) +1))
#define HU_INPUTWIDTH	64
#define HU_INPUTHEIGHT	1

// TYPES -------------------------------------------------------------------

typedef struct {
	char    text[MAX_LINELEN];
	int     time;
} message_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

#if __JDOOM__ || __JHERETIC__

char   *player_names[4];
int     player_names_idx[] = {
	TXT_HUSTR_PLRGREEN,
	TXT_HUSTR_PLRINDIGO,
	TXT_HUSTR_PLRBROWN,
	TXT_HUSTR_PLRRED
};

#else

enum {
	CT_PLR_BLUE = 1,
	CT_PLR_RED,
	CT_PLR_YELLOW,
	CT_PLR_GREEN,
	CT_PLR_PLAYER5,
	CT_PLR_PLAYER6,
	CT_PLR_PLAYER7,
	CT_PLR_PLAYER8
};

char   *player_names[8];
int     player_names_idx[] = {
	CT_PLR_BLUE,
	CT_PLR_RED,
	CT_PLR_YELLOW,
	CT_PLR_GREEN,
	CT_PLR_PLAYER5,
	CT_PLR_PLAYER6,
	CT_PLR_PLAYER7,
	CT_PLR_PLAYER8
};

#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static message_t messages[MAX_MESSAGES];
static int firstmsg, lastmsg, msgcount = 0;
static float yoffset = 0;		// Scroll-up offset.

byte    chatchar = 0;
static player_t *plr;

boolean shiftdown = false;
int     chat_to = 0;			// 0=all, 1=player 0, etc.

//static hu_textline_t w_title;

static char lastmessage[HU_MAXLINELENGTH + 1];

boolean chat_on;
static hu_itext_t w_chat;
static boolean always_off = false;

//static char           chat_dest[MAXPLAYERS];
static hu_itext_t w_inputbuffer[MAXPLAYERS];

static boolean message_on;
boolean message_dontfuckwithme;
static boolean message_nottobefuckedwith;
boolean message_noecho;

static hu_stext_t w_message;
static int message_counter;

const char *shiftxform;

const char french_shiftxform[] = {
	0,
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31,
	' ', '!', '"', '#', '$', '%', '&',
	'"',						// shift-'
	'(', ')', '*', '+',
	'?',						// shift-,
	'_',						// shift--
	'>',						// shift-.
	'?',						// shift-/
	'0',						// shift-0
	'1',						// shift-1
	'2',						// shift-2
	'3',						// shift-3
	'4',						// shift-4
	'5',						// shift-5
	'6',						// shift-6
	'7',						// shift-7
	'8',						// shift-8
	'9',						// shift-9
	'/',
	'.',						// shift-;
	'<',
	'+',						// shift-=
	'>', '?', '@',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'[',						// shift-[
	'!',						// shift-backslash - OH MY GOD DOES WATCOM SUCK
	']',						// shift-]
	'"', '_',
	'\'',						// shift-`
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'{', '|', '}', '~', 127
};

const char english_shiftxform[] = {

	0,
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31,
	' ', '!', '"', '#', '$', '%', '&',
	'"',						// shift-'
	'(', ')', '*', '+',
	'<',						// shift-,
	'_',						// shift--
	'>',						// shift-.
	'?',						// shift-/
	')',						// shift-0
	'!',						// shift-1
	'@',						// shift-2
	'#',						// shift-3
	'$',						// shift-4
	'%',						// shift-5
	'^',						// shift-6
	'&',						// shift-7
	'*',						// shift-8
	'(',						// shift-9
	':',
	':',						// shift-;
	'<',
	'+',						// shift-=
	'>', '?', '@',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'[',						// shift-[
	'!',						// shift-backslash - OH MY GOD DOES WATCOM SUCK
	']',						// shift-]
	'"', '_',
	'\'',						// shift-`
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'{', '|', '}', '~', 127
};

char    frenchKeyMap[128] = {
	0,
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31,
	' ', '!', '"', '#', '$', '%', '&', '%', '(', ')', '*', '+', ';', '-', ':',
	'!',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', 'M', '<', '=', '>',
	'?',
	'@', 'Q', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', ',', 'N',
	'O',
	'P', 'A', 'R', 'S', 'T', 'U', 'V', 'Z', 'X', 'Y', 'W', '^', '\\', '$', '^',
	'_',
	'@', 'Q', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', ',', 'N',
	'O',
	'P', 'A', 'R', 'S', 'T', 'U', 'V', 'Z', 'X', 'Y', 'W', '^', '\\', '$', '^',
	127
};

// CODE --------------------------------------------------------------------

/*
 *  ForeignTranslation
 *
 */
char ForeignTranslation(unsigned char ch)
{
	return ch < 128 ? frenchKeyMap[ch] : ch;
}

/*
 *  strcatQuoted
 *
 */
static void StrCatQuoted(char *dest, char *src)
{
	int     k = strlen(dest) + 1, i;

	strcat(dest, "\"");
	for(i = 0; src[i]; i++)
	{
		if(src[i] == '"')
		{
			strcat(dest, "\\\"");
			k += 2;
		}
		else
		{
			dest[k++] = src[i];
			dest[k] = 0;
		}
	}
	strcat(dest, "\"");
}

/*
 * HUMsg_Clear
 *
 *  	Clears the message buffer.
 */
void HUMsg_Clear(void)
{
	// The message display is empty.
	firstmsg = lastmsg = 0;
	msgcount = 0;
}

/*
 *  HUMsg_Message
 *
 *  	Add a new message.
 */
void HUMsg_Message(char *msg, int msgtics)
{
	messages[lastmsg].time = cfg.msgUptime + msgtics;
	strcpy(messages[lastmsg].text, msg);
	lastmsg = IN_RANGE(lastmsg + 1);
	if(msgcount == MAX_MESSAGES)
		firstmsg = lastmsg;
	else if(msgcount == cfg.msgCount)
		firstmsg = IN_RANGE(firstmsg + 1);
	else
		msgcount++;
}

/*
 *  HUMsg_DropLast
 *
 *		Removes the oldest message.
 */
void HUMsg_DropLast(void)
{
	if(!msgcount)
		return;
	firstmsg = IN_RANGE(firstmsg + 1);
	if(messages[firstmsg].time < 10)
		messages[firstmsg].time = 10;
	msgcount--;
}

/*
 *  HUMsg_Drawer
 *
 */
void HUMsg_Drawer(void)
{
	int     m, num, y, lh = LINEHEIGHT_A, td, x;
	float   col[4];

	// How many messages should we print?
	num = msgcount;

	if(cfg.msgAlign == ALIGN_LEFT)
		x = 0;
	else if (cfg.msgAlign == ALIGN_CENTER)
		x = 160;
	else
		x = 320;

    Draw_BeginZoom(cfg.msgScale, x, 0);
	gl.Translatef(0, -yoffset, 0);

	// First 'num' messages starting from the last one.
	for(y = (num - 1) * lh, m = IN_RANGE(lastmsg - 1); num;
		y -= lh, num--, m = IN_RANGE(m - 1))
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
				col[3] = messages[m].time / (float) LINEHEIGHT_A *0.9f;

			// Use normal msg color.
			memcpy(col, cfg.msgColor, sizeof(cfg.msgColor));
		}

		// draw using param text
		// messages may use the params to override eg colour (Hexen's important messages)  
		WI_DrawParamText(x, 1 + y, messages[m].text, hu_font_a, col[0], col[1], col[2], col[3], false, false, cfg.msgAlign);

	}

    Draw_EndZoom();

	HUlib_drawIText(&w_chat);
}

/*
 * HUMsg_Ticker
 *
 *		Called by HU_ticker()
 */
void HUMsg_Ticker(void)
{
	int     i;

	// Countdown to scroll-up.
	for(i = 0; i < MAX_MESSAGES; i++)
		messages[i].time--;
	if(msgcount)
	{
		yoffset = 0;
		if(messages[firstmsg].time >= 0 &&
		   messages[firstmsg].time <= LINEHEIGHT_A)
		{
			yoffset = LINEHEIGHT_A - messages[firstmsg].time;
		}
		else if(messages[firstmsg].time < 0)
			HUMsg_DropLast();
	}

	// tick down message counter if message is up
	if(message_counter && !--message_counter)
	{
		message_on = false;
		message_nottobefuckedwith = false;
	}

	if(cfg.msgShow || message_dontfuckwithme)
	{
		// display message if necessary
		if((plr->message && !message_nottobefuckedwith) ||
		   (plr->message && message_dontfuckwithme))
		{
			//HUlib_addMessageToSText(&w_message, 0, plr->message);
#if __JHEXEN__ || __JSTRIFE__
			HUMsg_Message(plr->message, plr->messageTics);
#else
			HUMsg_Message(plr->message, 0);
#endif
			plr->message = 0;
			message_on = true;
			message_counter = HU_MSGTIMEOUT;
			message_nottobefuckedwith = message_dontfuckwithme;
			message_dontfuckwithme = 0;
		}

	}							// else message_on = false;

	message_noecho = false;
}

/*
 *  HU_Erase
 *
 */
void HU_Erase(void)
{

	HUlib_eraseSText(&w_message);
	HUlib_eraseIText(&w_chat);

	//HUlib_eraseTextLine(&w_title);

}

/*
 *  HUMsg_Init
 *
 *		Called by HU_init()
 */
void HUMsg_Init(void)
{
	int     i;

	// Setup strings.
	for(i = 0; i < 10; i++)
		if(!cfg.chat_macros[i])	// Don't overwrite if already set.
			cfg.chat_macros[i] = GET_TXT(TXT_HUSTR_CHATMACRO0 + i);

#define INIT_STRINGS(x, x_idx) \
	for(i=0; i<sizeof(x_idx)/sizeof(int); i++) \
		x[i] = x_idx[i]==-1? "NEWLEVEL" : GET_TXT(x_idx[i]);

	INIT_STRINGS(player_names, player_names_idx);

	/*    if (french)
	   shiftxform = french_shiftxform;
	   else */

	shiftxform = english_shiftxform;
}

/*
 *  HUMsg_Start
 *
 *		Called by HU_Start()
 */
void HUMsg_Start(void)
{
	int     i;

	plr = &players[consoleplayer];
	message_on = false;
	message_dontfuckwithme = false;
	message_nottobefuckedwith = false;
	chat_on = false;

	// create the message widget
	HUlib_initSText(&w_message, HU_MSGX, HU_MSGY, HU_MSGHEIGHT, hu_font_a,
					HU_FONTSTART, &message_on);

	// create the chat widget
	HUlib_initIText(&w_chat, HU_INPUTX, HU_INPUTY, hu_font_a, HU_FONTSTART,
					&chat_on);

	// create the inputbuffer widgets
	for(i = 0; i < MAXPLAYERS; i++)
		HUlib_initIText(&w_inputbuffer[i], 0, 0, 0, 0, &always_off);
}

/*
 *  HU_sendMessage
 *
 */
void HU_sendMessage(char *msg)
{
	char    buff[256];
	int     i;

	strcpy(lastmessage, msg);
	// Send the message to the other players explicitly,
	// chatting is no more synchronized.
	if(chat_to == HU_BROADCAST)
	{
		strcpy(buff, "chat ");
		StrCatQuoted(buff, msg);
		Con_Execute(buff, false);
	}
	else
	{
		// Send to all of the destination color.
		for(i = 0; i < MAXPLAYERS; i++)
			if(players[i].plr->ingame && cfg.PlayerColor[i] == chat_to)
			{
				sprintf(buff, "chatNum %d ", i);
				StrCatQuoted(buff, msg);
				Con_Execute(buff, false);
			}
	}
#ifdef __JDOOM__
	if(gamemode == commercial)
		S_LocalSound(sfx_radio, 0);
	else
		S_LocalSound(sfx_tink, 0);
#endif
}

/*
 *  HU_Responder
 *
 */
boolean HU_Responder(event_t *ev)
{
	char   *macromessage;
	boolean eatkey = false;
	static boolean altdown = false;
	unsigned char c;
	int     i;
	int     numplayers;

	/*    static char       destination_keys[MAXPLAYERS] =
	   {
	   HUSTR_KEYGREEN,
	   HUSTR_KEYINDIGO,
	   HUSTR_KEYBROWN,
	   HUSTR_KEYRED
	   }; */

	//    static int        num_nobrainers = 0;

	numplayers = 0;
	for(i = 0; i < MAXPLAYERS; i++)
		numplayers += players[i].plr->ingame;

	if(ev->data1 == DDKEY_RSHIFT)
	{
		shiftdown = ev->type == ev_keydown || ev->type == ev_keyrepeat;
		return false;
	}
	else if(ev->data1 == DDKEY_RALT || ev->data1 == DDKEY_LALT)
	{
		altdown = ev->type == ev_keydown || ev->type == ev_keyrepeat;
		return false;
	}
	if(ev->type != ev_keydown && ev->type != ev_keyrepeat)
		return false;

	if(!chat_on)
	{
	}
	else
	{
		c = ev->data1;
		// send a macro
		if(altdown)
		{
			c = c - '0';
			if(c > 9)
				return false;
			// fprintf(stderr, "got here\n");
			macromessage = cfg.chat_macros[c];

			// leave chat mode and notify that it was sent
			chat_on = false;
			eatkey = true;
			HU_sendMessage(cfg.chat_macros[c]);
		}
		else
		{
			/*if (french) c = ForeignTranslation(c); */
			if(shiftdown || (c >= 'a' && c <= 'z'))
				c = shiftxform[c];
			eatkey = HUlib_keyInIText(&w_chat, c);
			if(eatkey)
			{
				// static unsigned char buf[20]; // DEBUG
				//HU_queueChatChar(c);

				// sprintf(buf, "KEY: %d => %d", ev->data1, c);
				//      plr->message = buf;
			}
			if(c == DDKEY_ENTER)
			{
				chat_on = false;
				if(w_chat.l.len)
				{
					HU_sendMessage(w_chat.l.l);
				}
			}
			else if(c == DDKEY_ESCAPE)
				chat_on = false;
		}
	}

	return eatkey;

}

/*
 *  CCmdBeginChat: Start a chat in a netgame
 *
 */
int CCmdBeginChat(int argc, char **argv)
{
	if(!IS_NETGAME || chat_on)
		return false;
	if(argc == 2)
	{
		chat_to = atoi(argv[1]);
		if(chat_to < 0 || chat_to > 3)
			return false;		// Bad destination.
	}
	else
		chat_to = HU_BROADCAST;
	chat_on = true;
	HUlib_resetIText(&w_chat);
	return true;
}

/*
 *  CCmdMsgRefresh
 *
 */
int CCmdMsgRefresh(int argc, char **argv)
{
	if(chat_on)
		return false;
	message_on = true;
	message_counter = HU_MSGTIMEOUT;
	return true;
}
