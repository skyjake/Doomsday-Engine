//
// Chat mode
//

// FIXME: No need to queue anything, just respond in CT_Responder().

#include <string.h>
#include <ctype.h>
#include "Doomdef.h"
#include "P_local.h"
#include "Soundst.h"
#include "settings.h"

#define QUEUESIZE	128
#define MESSAGESIZE	128
#define MESSAGELEN 	265

#define CT_PLR_GREEN	1
#define CT_PLR_YELLOW	2
#define CT_PLR_RED		3
#define CT_PLR_BLUE		4
#define CT_PLR_ALL		5

/*#define CT_KEY_GREEN	'g'
#define CT_KEY_YELLOW	'y'
#define CT_KEY_RED		'r'
#define CT_KEY_BLUE		'b'
#define CT_KEY_ALL		't'*/
#define CT_ESCAPE		6

// Public data

void CT_Init(void);
void CT_Drawer(void);
boolean CT_Responder(event_t *ev);
void CT_Ticker(void);
char CT_dequeueChatChar(void);

boolean chatmodeon;

// Private data

void CT_queueChatChar(char ch);
void CT_ClearChatMessage();
void CT_AddChar(char c);
void CT_BackSpace();

int head;
int tail;
byte ChatQueue[QUEUESIZE];
int chat_dest; 
char chat_msg[MESSAGESIZE];
char plr_lastmsg[MESSAGESIZE+9]; // add in the length of the pre-string
int msgptr;
int msglen;

boolean cheated;

static int FontABaseLump;

char *CT_FromPlrText[MAXPLAYERS] =
{
	"GREEN:  ",
	"YELLOW:  ",
	"RED:  ",
	"BLUE:  "
};

char *chat_macros[10];/* = 
{
	HUSTR_CHATMACRO0,
	HUSTR_CHATMACRO1,
	HUSTR_CHATMACRO2,
	HUSTR_CHATMACRO3,
	HUSTR_CHATMACRO4,
	HUSTR_CHATMACRO5,
	HUSTR_CHATMACRO6,
	HUSTR_CHATMACRO7,
	HUSTR_CHATMACRO8,
	HUSTR_CHATMACRO9
};*/

boolean altdown;
boolean shiftdown;

int chatchar = 0;


//===========================================================================
//
// CT_Init
//
// 	Initialize chat mode data
//===========================================================================

void CT_Init(void)
{
	int i;

	for(i=0; i<10; i++)
		chat_macros[i] = GET_TXT(TXT_HUSTR_CHATMACRO0 + i);

	head = 0; //initialize the queue index
	tail = 0;
	chatmodeon = false;
	memset(ChatQueue, 0, QUEUESIZE);
	chat_dest = 0;
	msgptr = 0;
	memset(plr_lastmsg, 0, MESSAGESIZE);
	memset(chat_msg, 0, MESSAGESIZE);
	FontABaseLump = W_GetNumForName("FONTA_S")+1;
	return;
}

//===========================================================================
//
// CT_Stop
//
//===========================================================================

void CT_Stop(void)
{
	chatmodeon = false;
	return;
}

//===========================================================================
//
// CT_Responder
//
//===========================================================================

boolean CT_Responder(event_t *ev)
{
	char *macro;

	if(!IS_NETGAME)
	{
		return false;
	}
	if(ev->data1 == DDKEY_RALT)
	{
		altdown = (ev->type == ev_keydown || ev->type == ev_keyrepeat);
		return false;
	}
	if(ev->data1 == DDKEY_RSHIFT)
	{
		shiftdown = (ev->type == ev_keydown || ev->type == ev_keyrepeat);
		return false;
	}
	if(ev->type != ev_keydown && ev->type != ev_keyrepeat)
	{
		return false;
	}
	// Chatmode is begun with the 'BeginChat' command.
	if(!chatmodeon) return false;

	if(altdown)
	{
		if(ev->data1 >= '0' && ev->data1 <= '9')
		{
			if(ev->data1 == '0')
			{ // macro 0 comes after macro 9
				ev->data1 = '9'+1;
			}
			macro = chat_macros[ev->data1-'1'];
			CT_queueChatChar(DDKEY_ENTER); //send old message
			//CT_queueChatChar(chat_dest); // chose the dest.
			while(*macro)
			{
				CT_queueChatChar(toupper(*macro++));
			}
			CT_queueChatChar(DDKEY_ENTER); //send it off...
			CT_Stop();
			return true;
		}
	}
	if(ev->data1 == DDKEY_ENTER)
	{
		CT_queueChatChar(DDKEY_ENTER);
		CT_Stop();
		return true;
	}
	else if(ev->data1 == DDKEY_ESCAPE)
	{
		CT_queueChatChar(CT_ESCAPE);
		CT_Stop();
		return true;
	}
	else if(ev->data1 >= 'a' && ev->data1 <= 'z')
	{
		CT_queueChatChar(ev->data1-32);
		return true;
	}
	else if(shiftdown)
	{
		if(ev->data1 == '1')
		{
			CT_queueChatChar('!');
			return true;
		}
		else if(ev->data1 == '/')
		{
			CT_queueChatChar('?');
			return true;
		}
	}
	else
	{
		if(ev->data1 == ' ' || ev->data1 == ',' || ev->data1 == '.'
			|| (ev->data1 >= '0' && ev->data1 <= '9') || ev->data1 == '\''
			|| ev->data1 == DDKEY_BACKSPACE || ev->data1 == '-' || ev->data1 == '=')
		{
			CT_queueChatChar(ev->data1);
			return true;
		}
	}
	return false;
}

void strcatQuoted(char *dest, char *src)
{
	int	k = strlen(dest)+1, i;

	strcat(dest, "\"");
	for(i=0; src[i]; i++)
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

void CT_SendMsg(int destplr, char *msg)
{
	char buff[256];

	if(destplr < 0)
		strcpy(buff, "chat ");
	else
		sprintf(buff, "chatNum %i ", destplr);
	strcatQuoted(buff, msg);
	Con_Execute(buff, false);
}

//===========================================================================
//
// CT_Ticker
//
//===========================================================================

void CT_Ticker(void)
{
	int j;
	char c;
	int numplayers;

	chatchar = CT_dequeueChatChar();
	if((c = chatchar) != 0)
	{
		if(c == CT_ESCAPE)
		{
			CT_ClearChatMessage();
		}
		else if(c == DDKEY_ENTER)
		{
			numplayers = 0;
			for(j = 0; j < MAXPLAYERS; j++)
				numplayers += players[j].plr->ingame;
			CT_AddChar(0); // set the end of message character
			strcpy(plr_lastmsg, chat_msg);
			if(*chat_msg)
			{
				if(numplayers > 1)
				{
					// Send the message to the other players explicitly,
					// chatting is no more synchronized.
					if(chat_dest == CT_PLR_ALL)
					{
						CT_SendMsg(-1, plr_lastmsg);
					}
					else
					{
						// Send to all of the destination color.
						for(j=0; j<MAXPLAYERS; j++)
							if(players[j].plr->ingame && cfg.PlayerColor[j]+1 == chat_dest)
								CT_SendMsg(j, plr_lastmsg);
					}
					S_LocalSound(sfx_chat, NULL);
				}
				else
				{
					P_SetMessage(&players[consoleplayer],
						"THERE ARE NO OTHER PLAYERS IN THE GAME!", true);
					S_LocalSound(sfx_chat, NULL);
				}
			}
			CT_ClearChatMessage();
		}
		else if(c == DDKEY_BACKSPACE)
		{
			CT_BackSpace();
		}
		else
		{
			CT_AddChar(c);
		}
	}
}

//===========================================================================
//
// CT_Drawer
//
//===========================================================================

void CT_Drawer(void)
{
	int i;
	int x;
	patch_t *patch;

	if(chatmodeon)
	{
		x = 25;
		for(i = 0; i < msgptr; i++)
		{
			if(chat_msg[i] < 33)
			{
				x += 6;
			}
			else
			{
				int pnum = FontABaseLump + chat_msg[i]-33;
				patch = W_CacheLumpNum(pnum, PU_CACHE);
				GL_DrawPatch(x, 10, pnum);
				x += patch->width;
			}
		}
		GL_DrawPatch(x, 10, W_GetNumForName("FONTA59"));
		GL_Update(DDUF_TOP | DDUF_MESSAGES);
	}
}

//===========================================================================
//
// CT_queueChatChar
//
//===========================================================================

void CT_queueChatChar(char ch)
{
	if(((tail+1)&(QUEUESIZE-1)) == head)
	{ // the queue is full
		return;
	}
	ChatQueue[tail] = ch;
	tail = (tail+1)&(QUEUESIZE-1);
}

//===========================================================================
//
// CT_dequeueChatChar
//
//===========================================================================

char CT_dequeueChatChar(void)
{
	byte temp;

	if(head == tail)
	{ // queue is empty
		return 0;
	}
	temp = ChatQueue[head];
	head = (head+1)&(QUEUESIZE-1);
	return temp;
}

//===========================================================================
//
// CT_AddChar
//
//===========================================================================

void CT_AddChar(char c)
{
	patch_t *patch;

	if(msgptr+1 >= MESSAGESIZE || msglen >= MESSAGELEN)
	{ // full.
		return;
	}
	chat_msg[msgptr] = c;
	msgptr++;
	if(c < 33)
	{
		msglen += 6;
	}
 	else
	{
		patch = W_CacheLumpNum(FontABaseLump+c-33, PU_CACHE);
		msglen += patch->width;
	}
}

//===========================================================================
//
// CT_BackSpace
//
// 	Backs up a space, when the user hits (obviously) backspace
//===========================================================================

void CT_BackSpace()
{
	patch_t *patch;
	char c;

	if(msgptr == 0)
	{ // message is already blank
		return;
	}
	msgptr--;
	c = chat_msg[msgptr];
	if(c < 33)
	{
		msglen -= 6;
	}
	else
	{
		patch = W_CacheLumpNum(FontABaseLump+c-33, PU_CACHE);
		msglen -= patch->width;
	}
	chat_msg[msgptr] = 0;
}

//===========================================================================
//
// CT_ClearChatMessage
//
// 	Clears out the data for the chat message, but the player's message
//		is still saved in plrmsg.
//===========================================================================

void CT_ClearChatMessage()
{
	memset(chat_msg, 0, MESSAGESIZE);
	msgptr = 0;
	msglen = 0;
}

//===========================================================================
//
// Console Commands To Control Chatting
//
//===========================================================================

int CCmdBeginChat(int argc, char **argv)
{
	if(!IS_NETGAME || chatmodeon) return false;
	if(argc == 2)
	{
		chat_dest = atoi(argv[1])+1;
		if(chat_dest < 1 || chat_dest > 4)
			return false; // Bad destination.
	}
	else chat_dest = CT_PLR_ALL;
	chatmodeon = true;
	return true;
}
