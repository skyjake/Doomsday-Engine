
//**************************************************************************
//**
//** S_MUS.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_audio.h"
#include "de_misc.h"

#include "sys_audio.h"
#include "r_extres.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct interface_info_s
{
	musinterface_generic_t **ip;
	const char *name;
}
interface_info_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int mus_preference = MUSP_EXT;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean mus_avail = false;

// The interfaces.
static musinterface_mus_t *imus = 0;
static musinterface_ext_t *iext = 0;
static musinterface_cd_t *icd = 0;

// The interface list. Used to access the common features of all the
// interfaces conveniently.
static interface_info_t interfaces[] =
{
	{ (musinterface_generic_t**) &imus,	"Mus" },
	{ (musinterface_generic_t**) &iext,	"Ext" },
	{ (musinterface_generic_t**) &icd,	"CD" },
	{ 0, 0 }
};

// CODE --------------------------------------------------------------------

//===========================================================================
// Mus_Init
//	Initialize the Mus module and choose the interfaces to use. Returns
//	true if no errors occur.
//===========================================================================
boolean Mus_Init(void)
{
	int i;
	
	if(mus_avail || ArgExists("-nomusic")) return true;

	// The Win driver is always initialized.
	if(musd_win.Init())
	{
		// Use Win's Mus interface.
		imus = &musd_win_imus;
	}
	else
	{
		Con_Message("Mus_Init: Failed to initialize Win driver.\n");
	}

	// Can we use FMOD?
	if(!ArgExists("-nofmod") && musd_fmod.Init())
	{
		// FMOD has been successfully initialized.
		// We get the CD and Ext interfaces.
		iext = &musd_fmod_iext;
		icd = &musd_fmod_icd;
	}
	else
	{
		// FMOD is either disabled or the init failed. 
		// Must rely on Windows, then, without an Ext interface.
		icd = &musd_win_icd;
	}

	// Initialize the chosen interfaces.
	for(i = 0; interfaces[i].ip; i++)
		if(*interfaces[i].ip && !(*interfaces[i].ip)->Init())
		{
			Con_Message("Mus_Init: Failed to initialize %s interface.\n",
				interfaces[i].name);
		}

	// Print a list of the chosen interfaces.
	if(verbose)
	{
		char buf[40];
		Con_Printf("Mus_Init: Interfaces: ");
		for(i = 0; interfaces[i].ip; i++)
			if(*interfaces[i].ip)
			{
				if(!(*interfaces[i].ip)->Get(MUSIP_ID, buf)) 
					strcpy(buf, "?");
				Con_Printf("%s%s", i? ", " : "", buf);
			}

		Con_Printf("\n");
	}

	mus_avail = true;
	return true;
}

//===========================================================================
// Mus_Shutdown
//===========================================================================
void Mus_Shutdown(void)
{
	if(!mus_avail) return;
	mus_avail = false;

	// Shut down the drivers. They shut down their interfaces automatically.
	musd_fmod.Shutdown();
	musd_win.Shutdown();

	// No more interfaces.
	imus = 0;
	iext = 0;
	icd = 0;
}

//===========================================================================
// Mus_StartFrame
//	Called on each frame by S_StartFrame.
//===========================================================================
void Mus_StartFrame(void)
{
	int i;

	if(!mus_avail) return;

	// Update all interfaces.
	for(i = 0; interfaces[i].ip; i++)
		if(*interfaces[i].ip) (*interfaces[i].ip)->Update();
}

//===========================================================================
// Mus_SetVolume
//	Set the general music volume. Affects all music played by all 
//	interfaces.
//===========================================================================
void Mus_SetVolume(float vol)
{
	int i;

	if(!mus_avail) return;

	// Set volume of all available interfaces.
	for(i = 0; interfaces[i].ip; i++)
		if(*interfaces[i].ip) (*interfaces[i].ip)->Set(MUSIP_VOLUME, vol);
}

//===========================================================================
// Mus_Pause
//	Pauses or resumes the music.
//===========================================================================
void Mus_Pause(boolean do_pause)
{
	int i;
	
	if(!mus_avail) return;

	// Pause all interfaces.
	for(i = 0; interfaces[i].ip; i++)
		if(*interfaces[i].ip) (*interfaces[i].ip)->Pause(do_pause);
}

//===========================================================================
// Mus_Stop
//===========================================================================
void Mus_Stop(void)
{
	int i;

	if(!mus_avail) return;

	// Stop all interfaces.
	for(i = 0; interfaces[i].ip; i++)
		if(*interfaces[i].ip) (*interfaces[i].ip)->Stop();
}

//===========================================================================
// Mus_IsMUSLump
//	Returns true if the specified lump contains a MUS song.
//===========================================================================
boolean Mus_IsMUSLump(int lump)
{
	char buf[8];

	W_ReadLumpSection(lump, buf, 0, 4);
	// ASCII "MUS" and CTRL-Z (hex 4d 55 53 1a)
	return !strncmp(buf, "MUS\x01a", 4);
}

//===========================================================================
// Mus_GetMUS
//	The lump may contain non-MUS data. Returns true if successful.
//===========================================================================
int Mus_GetMUS(ded_music_t *def)
{
	int len, lumpnum;
	void *ptr;

	if(!mus_avail || !imus) return false;

	lumpnum = W_CheckNumForName(def->lumpname);
	if(lumpnum < 0) return false; // No such lump.

	// Is this MUS data or what?
	if(!Mus_IsMUSLump(lumpnum)) return false;

	ptr = imus->SongBuffer(len = W_LumpLength(lumpnum));
	memcpy(ptr, W_CacheLumpNum(lumpnum, PU_CACHE), len);
	return true;
}

/*
 * Returns true if there is an external file name (that exists!).
 * Ext songs can be either in external files or non-MUS lumps.
 */
int Mus_GetExt(ded_music_t *def, char *path)
{
	char buf[300];
	int lumpnum, len;
	void *ptr;

	if(!mus_avail || !iext) return false;
	if(path) strcpy(path, "");

	// All external music files are specified relative to the base path.
	if(def->path.path[0])
	{
		M_PrependBasePath(def->path.path, buf);
		if(F_Access(buf))
		{
			// Return the real file name if not just checking.
			if(path) strcpy(path, buf);
			return true;		
		}
		Con_Message("Mus_GetExt: Song %s: %s not found.\n",
			def->id, def->path.path);
	}

	// Try the resource locator.
	if(R_FindResource(RC_MUSIC, def->lumpname, NULL, path)) 
		return true; // Got it!

	lumpnum = W_CheckNumForName(def->lumpname);
	if(lumpnum < 0) return false; // No such lump.

	if(Mus_IsMUSLump(lumpnum)) return false; // It's MUS!
	
	// Take a copy. Might be a big one (since it could be an MP3), so 
	// use the standard memory allocation routines.
	ptr = iext->SongBuffer(len = W_LumpLength(lumpnum));
	memcpy(ptr, W_CacheLumpNum(lumpnum, PU_CACHE), len);
	return true;
}

/*
 * Mus_GetCD
 *	Returns the track number if successful. Otherwise returns zero.
 */
int Mus_GetCD(ded_music_t *def)
{
	if(!mus_avail || !icd) return 0;
	if(def->cdtrack) return def->cdtrack;
	if(!strnicmp(def->path.path, "cd:", 3))
		return atoi(def->path.path + 3);
	return 0;
}

/*
 * Start playing a song. The chosen interface depends on what's available
 * and what kind of resources have been associated with the song. Returns
 * true if the song is successfully played. Any previously playing song
 * is stopped.
 */
int Mus_Start(ded_music_t *def, boolean looped)
{
	char path[300];
	int order[3], i;

	if(!mus_avail) return false;

	// Stop the currently playing song.
	Mus_Stop();

	// Choose the order in which to try to start the song.
	order[0] = mus_preference;
	switch(mus_preference)
	{
	case MUSP_CD:
		order[1] = MUSP_EXT;
		order[2] = MUSP_MUS;
		break;

	case MUSP_EXT:
		order[1] = MUSP_MUS;
		order[2] = MUSP_CD;
		break;

	default: // MUSP_MUS
		order[1] = MUSP_EXT;
		order[2] = MUSP_CD;
		break;
	}

	// Try to start the song.
	for(i = 0; i < 3; i++)
	{
		switch(order[i])
		{
		case MUSP_CD:
			if(Mus_GetCD(def)) return icd->Play(Mus_GetCD(def), looped);
			break;
			
		case MUSP_EXT:
			if(Mus_GetExt(def, path))
			{
				if(path[0])
				{
					if(verbose) Con_Printf("Mus_Start: %s\n", path);
					return iext->PlayFile(path, looped);
				}
				else
					return iext->PlayBuffer(looped);
			}
			break;
			
		case MUSP_MUS:
			if(Mus_GetMUS(def))	return imus->Play(looped);
			break;
		}
	}

	// The song was not started.
	return false;
}

//===========================================================================
// CCmdPlayMusic
//===========================================================================
int CCmdPlayMusic(int argc, char **argv)
{
	int i, len;
	void *ptr;
	char buf[300];

	if(!mus_avail)
	{
		Con_Printf("The Mus module is not available.\n");
		return false;
	}

	switch(argc)
	{
	default:
		Con_Printf("Usage:\n  %s (music-def)\n", argv[0]);
		Con_Printf("  %s lump (lumpname)\n", argv[0]);
		Con_Printf("  %s file (filename)\n", argv[0]);
		Con_Printf("  %s cd (track)\n", argv[0]);
		break;

	case 2:
		i = Def_GetMusicNum(argv[1]);
		if(i < 0)
		{
			Con_Printf("Music '%s' not defined.\n", argv[1]);
			return false;
		}
		Mus_Start(&defs.music[i], true);
		break;

	case 3:
		if(!stricmp(argv[1], "lump"))
		{
			i = W_CheckNumForName(argv[2]);
			if(i < 0) return false; // No such lump.
			Mus_Stop();
			if(Mus_IsMUSLump(i) && imus)
			{
				ptr = imus->SongBuffer(len = W_LumpLength(i));
				memcpy(ptr, W_CacheLumpNum(i, PU_CACHE), len);
				return imus->Play(true);
			}
			else if(!Mus_IsMUSLump(i) && iext)
			{
				ptr = iext->SongBuffer(len = W_LumpLength(i));
				memcpy(ptr, W_CacheLumpNum(i, PU_CACHE), len);
				return iext->PlayBuffer(true);
			}
		}
		else if(!stricmp(argv[1], "file"))
		{
			Mus_Stop();
			M_TranslatePath(argv[2], buf);
			if(iext) return iext->PlayFile(buf, true);
		}
		else if(!stricmp(argv[1], "cd"))
		{
			Mus_Stop();
			if(icd) return icd->Play(atoi(argv[2]), true);
		}
		break;
	}
	return true;
}

//===========================================================================
// CCmdPlayExt
//===========================================================================
int CCmdPlayExt(int argc, char **argv)
{
	char buf[300];

	if(argc != 2)
	{
		Con_Printf("Usage: %s (filename)\n", argv[0]);
		return true;
	}
	Mus_Stop();
	M_TranslatePath(argv[1], buf);
	if(iext) return iext->PlayFile(buf, true);
	return true;
}

//===========================================================================
// CCmdStopMusic
//===========================================================================
int CCmdStopMusic(int argc, char **argv)
{
	Mus_Stop();
	return true;
}