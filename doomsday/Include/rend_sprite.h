//===========================================================================
// REND_SPRITE.H
//===========================================================================
#ifndef __DOOMSDAY_RENDER_SPRITE_H__
#define __DOOMSDAY_RENDER_SPRITE_H__

extern float maxSpriteAngle;

void Rend_DrawMasked (void);
void Rend_DrawPlayerSprites(void);
void Rend_Draw3DPlayerSprites(void);
void Rend_Render3DVisSprite(vissprite_t *vismdl);
void Rend_SpriteTexCoord(int pnum, int x, int y);
void Rend_RenderSprite(vissprite_t *spr);

#endif 