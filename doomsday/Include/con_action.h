//===========================================================================
// CON_ACTION.H
//===========================================================================
#ifndef __DOOMSDAY_CONSOLE_ACTION_H__
#define __DOOMSDAY_CONSOLE_ACTION_H__

void Con_DefineActions(action_t *acts);
void Con_ClearActions(void);
// The command begins with a '+' or a '-'.
// Returns true if the action was changed successfully.
int Con_ActionCommand(char *cmd, boolean has_prefix);

#endif 