// Command line argument services.
#ifndef __COMMAND_LINE_ARGS_H__
#define __COMMAND_LINE_ARGS_H__

void		ArgInit(char *cmdline);
void		ArgShutdown(void);
void		ArgAbbreviate(char *longname, char *shortname);

int			Argc(void);
char *		Argv(int i);
char **		ArgvPtr(int i);
char *		ArgNext(void);
int			ArgCheck(char *check);
int			ArgCheckWith(char *check, int num);
int			ArgExists(char *check);
int			ArgIsOption(int i);
int			ArgRecognize(char *first, char *second);

#endif //__COMMAND_LINE_ARGS_H__