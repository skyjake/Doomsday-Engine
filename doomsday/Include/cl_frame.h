//===========================================================================
// CL_FRAME.H
//===========================================================================
#ifndef __DOOMSDAY_CLIENT_FRAME_H__
#define __DOOMSDAY_CLIENT_FRAME_H__

void Cl_InitFrame(void);
void Cl_ResetFrame(void);
void Cl_FrameReceived(void);
void Cl_Frame2Received(int packetType);

#endif 