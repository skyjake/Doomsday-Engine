#ifndef __DOOMSDAY_WINCONSOLE_H__
#define __DOOMSDAY_WINCONSOLE_H__

void Sys_ConInit();
void Sys_ConShutdown();
void Sys_ConPostEvents();
void Sys_ConPrint(int clflags, char *text);
void Sys_ConUpdateCmdLine(char *text);

#endif