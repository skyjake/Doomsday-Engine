#ifndef __JHERETIC_GAME_H__
#define __JHERETIC_GAME_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

void            G_ReadDemoTiccmd(player_t *pl);
void            G_WriteDemoTiccmd(ticcmd_t * cmd);
void            G_SpecialButton(player_t *pl);

#endif
