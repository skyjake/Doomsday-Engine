//===========================================================================
// R_DRAW.H
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_DRAW_H__
#define __DOOMSDAY_REFRESH_DRAW_H__

extern	boolean BorderNeedRefresh;
extern	boolean BorderTopRefresh;
extern  byte    *translationtables;
extern  byte    *dc_translation;

void    R_InitTranslationTables (void);
void	R_UpdateTranslationTables (void);
void	R_InitViewBorder();
void	R_SetBorderGfx(char *gfx[9]);
void	R_DrawViewBorder (void);
void	R_DrawTopBorder (void);

#endif 