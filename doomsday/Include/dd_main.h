//===========================================================================
// DD_MAIN.H
//===========================================================================
#ifndef __DOOMSDAY_MAIN_H__
#define __DOOMSDAY_MAIN_H__

// Verbose messages.
#define VERBOSE(code)	{ if(verbose >= 1) { code; } }
#define VERBOSE2(code)	{ if(verbose >= 2) { code; } }

extern int verbose;
extern int maxzone;	
extern int shareware;		// true if only episode 1 present
extern boolean cdrom;			// true if cd-rom mode active ("-cdrom")
extern boolean debugmode;		// checkparm of -debug
extern boolean nofullscreen;	// checkparm of -nofullscreen
extern boolean singletics;		// debug flag to cancel adaptiveness
extern FILE *debugfile;
extern int isDedicated;
extern char ddBasePath[];
extern char *defaultWads; // A list of wad names, whitespace in between (in .cfg).
extern directory_t ddRuntimeDir, ddBinDir; 

void DD_Main();
void DD_GameUpdate(int flags);
void DD_AddStartupWAD(const char *file);
void DD_AddIWAD(const char *path);
void DD_SetConfigFile(char *filename);
void DD_SetDefsFile(char *filename);
int	DD_GetInteger(int ddvalue);
void DD_SetInteger(int ddvalue, int parm);
ddplayer_t*	DD_GetPlayer(int number);
void DD_CheckTimeDemo(void);

#endif 