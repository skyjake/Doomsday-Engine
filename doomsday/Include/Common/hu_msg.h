#ifndef __HUD_MESSAGES_H__
#define __HUD_MESSAGES_H__

#ifdef __JDOOM__
#  include "d_event.h"
#  include "../jDoom/r_defs.h"
#elif __JHERETIC__
#  include "../jHeretic/Doomdef.h"
#  include "../jHeretic/R_local.h"
#elif __JHEXEN__
#  include "../jHexen/h2def.h"
#  include "../jHexen/R_local.h"
#elif __JSTRIFE__
#  include "../jStrife/h2def.h"
#  include "../jStrife/R_local.h"
#endif

#define HU_MSGREFRESH	DDKEY_ENTER
#define HU_MSGX		0
#define HU_MSGY		0
#define HU_MSGWIDTH	64			   // in characters
#define HU_MSGHEIGHT	1		   // in lines

#define HU_MSGTIMEOUT	(4*TICRATE)

#define MAX_MESSAGES	8
#define MAX_LINELEN		140
#define IN_RANGE(x)		((x)>=MAX_MESSAGES? (x)-MAX_MESSAGES : (x)<0? (x)+MAX_MESSAGES : (x))

void            HUMsg_Message(char *msg, int msgtics);
void            HUMsg_Drawer(void);
void            HUMsg_Ticker(void);
void            HUMsg_Start(void);
void            HUMsg_Init(void);

#endif
