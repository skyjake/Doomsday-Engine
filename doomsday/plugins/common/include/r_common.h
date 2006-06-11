//===========================================================================
// R_COMMON.H
//===========================================================================
#ifndef __GAME_COMMON_REFRESH_H__
#define __GAME_COMMON_REFRESH_H__

void            R_PrecachePSprites(void);
void            R_SetViewWindowTarget(int x, int y, int w, int h);
void            R_ViewWindowTicker(void);
void            R_GetViewWindow(float* x, float* y, float* w, float* h);
boolean         R_IsFullScreenViewWindow(void);

#endif
