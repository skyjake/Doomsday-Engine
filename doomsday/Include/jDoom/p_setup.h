/* $Id$
 * Map setup routines.
 */
#ifndef __P_SETUP__
#define __P_SETUP__

// Called by startup code.
void	P_Init (void);

boolean	P_MapExists(int episode, int map);
void	P_SetupLevel(int episode, int map, int playermask, skill_t skill);

#endif
