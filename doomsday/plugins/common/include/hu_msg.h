#ifndef __HUD_MESSAGES_H__
#define __HUD_MESSAGES_H__

#if   __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#define HU_MSGREFRESH   DDKEY_ENTER
#define HU_MSGX     0
#define HU_MSGY     0
#define HU_MSGWIDTH 64             // in characters
#define HU_MSGHEIGHT    1          // in lines

#define HU_MSGTIMEOUT   (4*TICRATE)

#define MAX_MESSAGES    8
#define MAX_LINELEN     140
#define IN_RANGE(x)     ((x)>=MAX_MESSAGES? (x)-MAX_MESSAGES : (x)<0? (x)+MAX_MESSAGES : (x))

void            HUMsg_Register(void);
void            HUMsg_SendMessage(char *msg);
boolean         HUMsg_SendMacro(int num);

void            HUMsg_Message(char *msg, int msgtics);
void            HUMsg_Drawer(void);
void            HUMsg_Ticker(void);
void            HUMsg_Start(void);
void            HUMsg_Init(void);
void            HUMsg_Clear(void);

#endif
