//===========================================================================
// R_MAIN.H
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_MAIN_H__
#define __DOOMSDAY_REFRESH_MAIN_H__

extern  fixed_t     viewx, viewy, viewz;
extern	float		viewfrontvec[3], viewupvec[3], viewsidevec[3];
extern  fixed_t		viewxOffset, viewyOffset, viewzOffset;
extern  angle_t     viewangle;
extern	float		viewpitch;
extern  ddplayer_t  *viewplayer;
extern  angle_t     clipangle;
extern  fixed_t		finetangent[FINEANGLES/2];

extern	int			validcount;
extern	int			viewwidth, viewheight, viewwindowx, viewwindowy;
extern	boolean		setsizeneeded;
extern	int			framecount;
extern	int			viewangleoffset;
extern	int			extralight;
extern	fixed_t		viewcos, viewsin;
extern	int			skyflatnum;
extern	int			rend_info_tris;
extern	int			rend_camera_smooth;

void		R_Init (void);
void		R_Update (void);
void		R_Shutdown(void);
void		R_RenderPlayerView (ddplayer_t *player);
void		R_ResetViewer(void);
void		R_ViewWindow(int x, int y, int w, int h);

#endif 