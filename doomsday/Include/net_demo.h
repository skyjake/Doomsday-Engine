#ifndef __DOOMSDAY_DEMO_H__
#define __DOOMSDAY_DEMO_H__

#include "con_decl.h"

extern int demotic, playback;

void Demo_Init(void);
void Demo_Ticker(void);

boolean Demo_BeginRecording(char *filename, int playernum);
void Demo_StopRecording(int playernum);
void Demo_PauseRecording(int playernum);
void Demo_ResumeRecording(int playernum);
void Demo_WritePacket(int playernum);
void Demo_BroadcastPacket(void);
void Demo_ReadLocalCamera(void); // pkt_democam

boolean Demo_BeginPlayback(char *filename);
boolean Demo_ReadPacket(void);
void Demo_StopPlayback(void);

// Console commands.
D_CMD( PlayDemo );
D_CMD( RecordDemo );
D_CMD( PauseDemo );
D_CMD( StopDemo );
D_CMD( DemoLump );

#endif