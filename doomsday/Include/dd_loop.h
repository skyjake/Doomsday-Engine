//===========================================================================
// DD_LOOP.H
//===========================================================================
#ifndef __DOOMSDAY_BASELOOP_H__
#define __DOOMSDAY_BASELOOP_H__

void DD_GameLoop(void);
void DD_DrawAndBlit(void);
void DD_TryRunTics(void);
void DD_StartTic(void);
void DD_StartFrame(void);
void DD_EndFrame(void);
int DD_GetFrameRate(void);

#endif 