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
// DESCRIPTION:  Heads-up displays
//
//-----------------------------------------------------------------------------

#include <ctype.h>

#include "doomdef.h"

#include "m_swap.h"

#include "hu_stuff.h"
#include "hu_msg.h"
#include "hu_lib.h"
#include "Mn_def.h"
#include "m_misc.h"

#include "s_sound.h"

#include "doomstat.h"
#include "d_config.h"
#include "r_local.h"
#include "p_local.h"

// Data.
#include "dstrings.h"

//
// Locally used constants, shortcuts.
//
#define HU_TITLE	(mapnames[(gameepisode-1)*9+gamemap-1])
#define HU_TITLE2	(mapnames2[gamemap-1])
#define HU_TITLEP	(mapnamesp[gamemap-1])
#define HU_TITLET	(mapnamest[gamemap-1])
#define HU_TITLEHEIGHT	1

#define HU_INPUTTOGGLE	't'
#define HU_INPUTX	HU_MSGX
#define HU_INPUTY	(HU_MSGY + HU_MSGHEIGHT*(SHORT(hu_font[0].height) +1))
#define HU_INPUTWIDTH	64
#define HU_INPUTHEIGHT	1



char* chat_macros[10];
int chat_macros_idx[] =
{
    TXT_HUSTR_CHATMACRO0,
    TXT_HUSTR_CHATMACRO1,
    TXT_HUSTR_CHATMACRO2,
    TXT_HUSTR_CHATMACRO3,
    TXT_HUSTR_CHATMACRO4,
    TXT_HUSTR_CHATMACRO5,
    TXT_HUSTR_CHATMACRO6,
    TXT_HUSTR_CHATMACRO7,
    TXT_HUSTR_CHATMACRO8,
    TXT_HUSTR_CHATMACRO9
};

char*	player_names[4];
int player_names_idx[] =
{
    TXT_HUSTR_PLRGREEN,
    TXT_HUSTR_PLRINDIGO,
    TXT_HUSTR_PLRBROWN,
    TXT_HUSTR_PLRRED
};


boolean				hu_showallfrags = false;
byte				chatchar = 0; 
static player_t*	plr;
dpatch_t			hu_font[HU_FONTSIZE];
dpatch_t			hu_font_a[HU_FONTSIZE], hu_font_b[HU_FONTSIZE];
static hu_textline_t w_title;
boolean				chat_on;
static hu_itext_t	w_chat;
static boolean		always_off = false;
//static char			chat_dest[MAXPLAYERS];
static hu_itext_t	w_inputbuffer[MAXPLAYERS];

static boolean		message_on;
boolean				message_dontfuckwithme;
static boolean		message_nottobefuckedwith;
boolean				message_noecho;

static hu_stext_t	w_message;
static int			message_counter;

extern int			showMessages;
extern boolean		automapactive;

static boolean		headsupactive = false;

//
// Builtin map names.
// The actual names can be found in DStrings.h.
//

char*	mapnames[9*5]; 	// DOOM shareware/registered/retail (Ultimate) names.
int mapnames_idx[] =
{
    TXT_HUSTR_E1M1,
    TXT_HUSTR_E1M2,
    TXT_HUSTR_E1M3,
    TXT_HUSTR_E1M4,
    TXT_HUSTR_E1M5,
    TXT_HUSTR_E1M6,
    TXT_HUSTR_E1M7,
    TXT_HUSTR_E1M8,
    TXT_HUSTR_E1M9,

    TXT_HUSTR_E2M1,
    TXT_HUSTR_E2M2,
    TXT_HUSTR_E2M3,
    TXT_HUSTR_E2M4,
    TXT_HUSTR_E2M5,
    TXT_HUSTR_E2M6,
    TXT_HUSTR_E2M7,
    TXT_HUSTR_E2M8,
    TXT_HUSTR_E2M9,

    TXT_HUSTR_E3M1,
    TXT_HUSTR_E3M2,
    TXT_HUSTR_E3M3,
    TXT_HUSTR_E3M4,
    TXT_HUSTR_E3M5,
    TXT_HUSTR_E3M6,
    TXT_HUSTR_E3M7,
    TXT_HUSTR_E3M8,
    TXT_HUSTR_E3M9,

    TXT_HUSTR_E4M1,
    TXT_HUSTR_E4M2,
    TXT_HUSTR_E4M3,
    TXT_HUSTR_E4M4,
    TXT_HUSTR_E4M5,
    TXT_HUSTR_E4M6,
    TXT_HUSTR_E4M7,
    TXT_HUSTR_E4M8,
    TXT_HUSTR_E4M9,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1
};

char*	mapnames2[32]; 	// DOOM 2 map names.
int mapnames2_idx[] =
{
    TXT_HUSTR_1,
    TXT_HUSTR_2,
    TXT_HUSTR_3,
    TXT_HUSTR_4,
    TXT_HUSTR_5,
    TXT_HUSTR_6,
    TXT_HUSTR_7,
    TXT_HUSTR_8,
    TXT_HUSTR_9,
    TXT_HUSTR_10,
    TXT_HUSTR_11,
	
    TXT_HUSTR_12,
    TXT_HUSTR_13,
    TXT_HUSTR_14,
    TXT_HUSTR_15,
    TXT_HUSTR_16,
    TXT_HUSTR_17,
    TXT_HUSTR_18,
    TXT_HUSTR_19,
    TXT_HUSTR_20,
	
    TXT_HUSTR_21,
    TXT_HUSTR_22,
    TXT_HUSTR_23,
    TXT_HUSTR_24,
    TXT_HUSTR_25,
    TXT_HUSTR_26,
    TXT_HUSTR_27,
    TXT_HUSTR_28,
    TXT_HUSTR_29,
    TXT_HUSTR_30,
    TXT_HUSTR_31,
    TXT_HUSTR_32
};


char*	mapnamesp[32]; 	// Plutonia WAD map names.
int mapnamesp_idx[] =
{
    TXT_PHUSTR_1,
    TXT_PHUSTR_2,
    TXT_PHUSTR_3,
    TXT_PHUSTR_4,
    TXT_PHUSTR_5,
    TXT_PHUSTR_6,
    TXT_PHUSTR_7,
    TXT_PHUSTR_8,
    TXT_PHUSTR_9,
    TXT_PHUSTR_10,
    TXT_PHUSTR_11,
	
    TXT_PHUSTR_12,
    TXT_PHUSTR_13,
    TXT_PHUSTR_14,
    TXT_PHUSTR_15,
    TXT_PHUSTR_16,
    TXT_PHUSTR_17,
    TXT_PHUSTR_18,
    TXT_PHUSTR_19,
    TXT_PHUSTR_20,
	
    TXT_PHUSTR_21,
    TXT_PHUSTR_22,
    TXT_PHUSTR_23,
    TXT_PHUSTR_24,
    TXT_PHUSTR_25,
    TXT_PHUSTR_26,
    TXT_PHUSTR_27,
    TXT_PHUSTR_28,
    TXT_PHUSTR_29,
    TXT_PHUSTR_30,
    TXT_PHUSTR_31,
    TXT_PHUSTR_32
};


char *mapnamest[32]; 	// TNT WAD map names.
int mapnamest_idx[] =
{
    TXT_THUSTR_1,
    TXT_THUSTR_2,
    TXT_THUSTR_3,
    TXT_THUSTR_4,
    TXT_THUSTR_5,
    TXT_THUSTR_6,
    TXT_THUSTR_7,
    TXT_THUSTR_8,
    TXT_THUSTR_9,
    TXT_THUSTR_10,
    TXT_THUSTR_11,
	
    TXT_THUSTR_12,
    TXT_THUSTR_13,
    TXT_THUSTR_14,
    TXT_THUSTR_15,
    TXT_THUSTR_16,
    TXT_THUSTR_17,
    TXT_THUSTR_18,
    TXT_THUSTR_19,
    TXT_THUSTR_20,
	
    TXT_THUSTR_21,
    TXT_THUSTR_22,
    TXT_THUSTR_23,
    TXT_THUSTR_24,
    TXT_THUSTR_25,
    TXT_THUSTR_26,
    TXT_THUSTR_27,
    TXT_THUSTR_28,
    TXT_THUSTR_29,
    TXT_THUSTR_30,
    TXT_THUSTR_31,
    TXT_THUSTR_32
};

const char*	shiftxform;

const char french_shiftxform[] =
{
    0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31,
    ' ', '!', '"', '#', '$', '%', '&',
    '"', // shift-'
    '(', ')', '*', '+',
    '?', // shift-,
    '_', // shift--
    '>', // shift-.
    '?', // shift-/
    '0', // shift-0
    '1', // shift-1
    '2', // shift-2
    '3', // shift-3
    '4', // shift-4
    '5', // shift-5
    '6', // shift-6
    '7', // shift-7
    '8', // shift-8
    '9', // shift-9
    '/',
    '.', // shift-;
    '<',
    '+', // shift-=
    '>', '?', '@',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[', // shift-[
    '!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
    ']', // shift-]
    '"', '_',
    '\'', // shift-`
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '{', '|', '}', '~', 127

};

const char english_shiftxform[] =
{

    0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31,
    ' ', '!', '"', '#', '$', '%', '&',
    '"', // shift-'
    '(', ')', '*', '+',
    '<', // shift-,
    '_', // shift--
    '>', // shift-.
    '?', // shift-/
    ')', // shift-0
    '!', // shift-1
    '@', // shift-2
    '#', // shift-3
    '$', // shift-4
    '%', // shift-5
    '^', // shift-6
    '&', // shift-7
    '*', // shift-8
    '(', // shift-9
    ':',
    ':', // shift-;
    '<',
    '+', // shift-=
    '>', '?', '@',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[', // shift-[
    '!', // shift-backslash - OH MY GOD DOES WATCOM SUCK
    ']', // shift-]
    '"', '_',
    '\'', // shift-`
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '{', '|', '}', '~', 127
};

char frenchKeyMap[128]=
{
    0,
    1,2,3,4,5,6,7,8,9,10,
    11,12,13,14,15,16,17,18,19,20,
    21,22,23,24,25,26,27,28,29,30,
    31,
    ' ','!','"','#','$','%','&','%','(',')','*','+',';','-',':','!',
    '0','1','2','3','4','5','6','7','8','9',':','M','<','=','>','?',
    '@','Q','B','C','D','E','F','G','H','I','J','K','L',',','N','O',
    'P','A','R','S','T','U','V','Z','X','Y','W','^','\\','$','^','_',
    '@','Q','B','C','D','E','F','G','H','I','J','K','L',',','N','O',
    'P','A','R','S','T','U','V','Z','X','Y','W','^','\\','$','^',127
};

char ForeignTranslation(unsigned char ch)
{
    return ch < 128 ? frenchKeyMap[ch] : ch;
}

void HU_Init(void)
{
    int		i;
    int		j;
    char	buffer[9];

	// Setup strings.
#define INIT_STRINGS(x, x_idx) \
	for(i=0; i<sizeof(x_idx)/sizeof(int); i++) \
		x[i] = x_idx[i]==-1? "NEWLEVEL" : GET_TXT(x_idx[i]);

	INIT_STRINGS(chat_macros, chat_macros_idx);
	INIT_STRINGS(player_names, player_names_idx);
	INIT_STRINGS(mapnames, mapnames_idx);
	INIT_STRINGS(mapnames2, mapnames2_idx);
	INIT_STRINGS(mapnamesp, mapnamesp_idx);
	INIT_STRINGS(mapnamest, mapnamest_idx);

/*    if (french)
		shiftxform = french_shiftxform;
    else*/
	
	shiftxform = english_shiftxform;

    // load the heads-up fonts
    j = HU_FONTSTART;
    for (i=0; i<HU_FONTSIZE; i++, j++)
    {
		// The original small red font.
		sprintf(buffer, "STCFN%.3d", j);
		R_CachePatch(&hu_font[i], buffer);

		// Small white font.
		sprintf(buffer, "FONTA%.3d", j);
		R_CachePatch(&hu_font_a[i], buffer);
		
		// Large (12) white font.
		sprintf(buffer, "FONTB%.3d", j);
		R_CachePatch(&hu_font_b[i], buffer);
		if(hu_font_b[i].lump == -1)
		{
			// This character is missing! (the first character
			// is supposedly always found)
			memcpy(hu_font_b+i, hu_font_b, sizeof(dpatch_t));
		}
    }

}

void HU_Stop(void)
{
    headsupactive = false;
}

void HU_Start(void)
{

    int		i;
    char*	s;

    if (headsupactive)
	HU_Stop();

    plr = &players[consoleplayer];
    message_on = false;
    message_dontfuckwithme = false;
    message_nottobefuckedwith = false;
    chat_on = false;

    // create the message widget
    HUlib_initSText(&w_message,
		    HU_MSGX, HU_MSGY, HU_MSGHEIGHT,
		    hu_font_a, 
		    HU_FONTSTART, &message_on);

    // create the map title widget
    HUlib_initTextLine(&w_title,
		       HU_TITLEX, HU_TITLEY,
		       hu_font_a, 
		       HU_FONTSTART);

    if(Get(DD_MAP_NAME))
		s = (char*) Get(DD_MAP_NAME);
	else
	{
		switch ( gamemode )
		{
		case shareware:
		case registered:
		case retail:
			s = HU_TITLE;
			break;
		case commercial:
		default:
			s = HU_TITLE2;
			break;
		}
	}
	// Plutonia and TNT are a special case.
	if(gamemission == pack_plut) s = HU_TITLEP;
	if(gamemission == pack_tnt) s = HU_TITLET;

    while (*s) HUlib_addCharToTextLine(&w_title, *(s++));

    // create the chat widget
    HUlib_initIText(&w_chat,
		    HU_INPUTX, HU_INPUTY,
		    hu_font_a,
		    HU_FONTSTART, &chat_on);

    // create the inputbuffer widgets
    for (i=0 ; i<MAXPLAYERS ; i++)
		HUlib_initIText(&w_inputbuffer[i], 0, 0, 0, 0, &always_off);

    headsupactive = true;

}

void HU_Drawer(void)
{
	int i, k, x, y;
	char buf[80];

	HUMsg_Drawer();
    HUlib_drawIText(&w_chat);

	if(hu_showallfrags)
	{
		for(y=8, i=0; i<MAXPLAYERS; i++, y+= 10)
		{
			sprintf(buf, "%i%s", i, i==consoleplayer? "=" : ":");
			M_WriteText(0, y, buf);
			x = 20;
			for(k=0; k<MAXPLAYERS; k++, x+= 18)
			{
				sprintf(buf, "%i", players[i].frags[k]);			
				M_WriteText(x, y, buf);
			}
		}
	}

    if (automapactive)
	{
		// Position the map title according to sbarscale.
		w_title.y = HU_TITLEY + 32*(20-cfg.sbarscale)/20;
		HUlib_drawTextLine(&w_title, false);
	}
}

void HU_Erase(void)
{

    HUlib_eraseSText(&w_message);
    HUlib_eraseIText(&w_chat);
    HUlib_eraseTextLine(&w_title);

}

void HU_Ticker(void)
{
/*    int i, rc;
    char c;*/
	
	HUMsg_Ticker();

    // tick down message counter if message is up
    if (message_counter && !--message_counter)
    {
		message_on = false;
		message_nottobefuckedwith = false;
    }
	
    if(showMessages || message_dontfuckwithme)
    {
		// display message if necessary
		if ((plr->message && !message_nottobefuckedwith)
			|| (plr->message && message_dontfuckwithme))
		{
			//HUlib_addMessageToSText(&w_message, 0, plr->message);
			HUMsg_Message(plr->message);
			plr->message = 0;
			message_on = true;
			message_counter = HU_MSGTIMEOUT;
			message_nottobefuckedwith = message_dontfuckwithme;
			message_dontfuckwithme = 0;
		}
		
    } // else message_on = false;

	message_noecho = false;

#if 0
	chatchar = HU_dequeueChatChar();
    // check for incoming chat characters
    if (netgame)
    {
		for (i=0 ; i<MAXPLAYERS; i++)
		{
			if (!players[i].plr->ingame) continue;
			if (i != consoleplayer && (c = chatchar))
			{
				if (c <= HU_BROADCAST)
					chat_dest[i] = c;
				else
				{
					if (c >= 'a' && c <= 'z')
						c = (char) shiftxform[(unsigned char) c];
					rc = HUlib_keyInIText(&w_inputbuffer[i], c);
					if (rc && c == DDKEY_ENTER)
					{
						gi.Message( "Send message!\n");
						/*if (w_inputbuffer[i].l.len 
							&& (chat_dest[i] == consoleplayer+1 || chat_dest[i] == HU_BROADCAST))
						{
							HUlib_addMessageToSText(&w_message,
								player_names[i],
								w_inputbuffer[i].l.l);
							
							message_nottobefuckedwith = true;
							message_on = true;
							message_counter = HU_MSGTIMEOUT;
							if ( gamemode == commercial )
								S_StartSound(0, sfx_radio);
							else
								S_StartSound(0, sfx_tink);
						}
						HUlib_resetIText(&w_inputbuffer[i]);*/
					}
				}
				chatchar = 0;
			}
		}
    }
#endif
}

boolean	shiftdown = false;
int chat_to = 0;	// 0=all, 1=player 0, etc.

int CCmdBeginChat(int argc, char **argv)
{
	if(!IS_NETGAME || chat_on) return false;
	if(argc == 2)
	{
		chat_to = atoi(argv[1]);
		if(chat_to < 0 || chat_to > 3)
			return false; // Bad destination.
	}
	else chat_to = HU_BROADCAST;
	chat_on = true;
	HUlib_resetIText(&w_chat);
	return true;
}

int CCmdMsgRefresh(int argc, char **argv)
{
	if(chat_on) return false;
	message_on = true;
	message_counter = HU_MSGTIMEOUT;
	return true;
}

static char lastmessage[HU_MAXLINELENGTH+1];

void HU_sendMessage(char *msg)
{
	char buff[256];
	int i;

	strcpy(lastmessage, msg);
	// Send the message to the other players explicitly,
	// chatting is no more synchronized.
	if(chat_to == HU_BROADCAST)
	{
		strcpy(buff, "chat ");
		strcatQuoted(buff, msg);
		Con_Execute(buff, false);
	}
	else
	{
		// Send to all of the destination color.
		for(i = 0; i < MAXPLAYERS; i++)
			if(players[i].plr->ingame && cfg.PlayerColor[i] == chat_to)
			{
				sprintf(buff, "chatNum %d ", i);
				strcatQuoted(buff, msg);
				Con_Execute(buff, false);
			}
	}
	if(gamemode == commercial)
		S_LocalSound(sfx_radio, 0);
	else
		S_LocalSound(sfx_tink, 0);
}

boolean HU_Responder(event_t *ev)
{
	char*			macromessage;
    boolean			eatkey = false;
    static boolean	altdown = false;
    unsigned char 	c;
    int				i;
    int				numplayers;
    
/*    static char		destination_keys[MAXPLAYERS] =
    {
		HUSTR_KEYGREEN,
		HUSTR_KEYINDIGO,
		HUSTR_KEYBROWN,
		HUSTR_KEYRED
		};*/
    
//    static int		num_nobrainers = 0;
	
    numplayers = 0;
    for (i=0 ; i<MAXPLAYERS ; i++)
		numplayers += players[i].plr->ingame;
	
    if(ev->data1 == DDKEY_RSHIFT)
    {
		shiftdown = ev->type == ev_keydown || ev->type == ev_keyrepeat;
		return false;
    }
    else if (ev->data1 == DDKEY_RALT || ev->data1 == DDKEY_LALT)
    {
		altdown = ev->type == ev_keydown || ev->type == ev_keyrepeat;
		return false;
    }
    if (ev->type != ev_keydown && ev->type != ev_keyrepeat)
		return false;
	
    if (!chat_on)
    {
    }
    else
    {
		c = ev->data1;
		// send a macro
		if (altdown)
		{
			c = c - '0';
			if (c > 9)
				return false;
			// fprintf(stderr, "got here\n");
			macromessage = chat_macros[c];
			
			// leave chat mode and notify that it was sent
			chat_on = false;
			/*strcpy(lastmessage, chat_macros[c]);
			plr->message = lastmessage;*/
			eatkey = true;
			HU_sendMessage(chat_macros[c]);
		}
		else
		{
			/*if (french) c = ForeignTranslation(c);*/
			if (shiftdown || (c >= 'a' && c <= 'z'))
				c = shiftxform[c];
			eatkey = HUlib_keyInIText(&w_chat, c);
			if (eatkey)
			{
				// static unsigned char buf[20]; // DEBUG
				//HU_queueChatChar(c);
				
				// sprintf(buf, "KEY: %d => %d", ev->data1, c);
				//      plr->message = buf;
			}
			if (c == DDKEY_ENTER)
			{
				chat_on = false;
				if(w_chat.l.len)
				{
					HU_sendMessage(w_chat.l.l);
				}
			}
			else if (c == DDKEY_ESCAPE)
				chat_on = false;
		}
    }
	
    return eatkey;
	
}

