//===========================================================================
// CON_BIND.H
//===========================================================================
#ifndef __DOOMSDAY_CONSOLE_BIND_H__
#define __DOOMSDAY_CONSOLE_BIND_H__

#include <stdio.h>

void B_Bind(event_t *event, char *command);
void B_EventBuilder(char *buff, event_t *ev, boolean to_event);
int B_BindingsForCommand(char *command, char *buffer);
void B_ClearBinding(char *command);
boolean B_Responder(event_t *ev);
void B_WriteToFile(FILE *file);
void B_Shutdown();

int DD_GetKeyCode(const char *key);

#endif 