
//**************************************************************************
//**
//** R_EXTRES.C
//**
//** Routines for locating external resource files.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_EXTENSIONS		10

// TYPES -------------------------------------------------------------------

typedef struct resclass_s {
	char path[256];			
	char overridePath[256];
} resclass_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The base directory for all resource directories.
char dataPath[256];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Class paths.
static const char *defaultResourcePath[NUM_RESOURCE_CLASSES] = 
{
	"Textures\\",
	"Patches\\",
	"LightMaps\\",
	"Music\\",
	"Sfx\\"
};

// Recognized extensions (in order of importance). "*" means 'anything'.
static const char *classExtension[NUM_RESOURCE_CLASSES][MAX_EXTENSIONS] =
{
	// Graphics favor quality.
	{ ".png", ".tga", ".pcx", NULL },
	{ ".png", ".tga", ".pcx", NULL },
	{ ".png", ".tga", ".pcx", NULL },

	// Extension doesn't matter with music, FMOD will either recognize 
	// it or not.
	{ ".mp3", ".ogg", ".wav", ".mod", ".it", ".mid", "*", NULL },

	// Only WAV files for sound effects.
	{ ".wav", NULL }
};

static resclass_t classInfo[NUM_RESOURCE_CLASSES];

// CODE --------------------------------------------------------------------

/*
 * Set the initial path names.
 */
void R_InitExternalResources(void)
{
	R_SetDataPath("}Data\\");
}

/*
 * Set the data path. The game module is responsible for calling this.
 */
void R_SetDataPath(const char *path)
{
	int i;

	M_TranslatePath(path, dataPath);
	Dir_ValidDir(dataPath);
	VERBOSE( Con_Message("R_SetDataPath: %s\n", dataPath) );

	// Update the paths of each class.
	memset(classInfo, 0, sizeof(classInfo));
	for(i = 0; i < NUM_RESOURCE_CLASSES; i++)
	{
		// The -texdir option specifies a path to look for TGA textures.
		if(i == RC_TEXTURE && ArgCheckWith("-texdir", 1))
		{
			M_TranslatePath(ArgNext(), classInfo[i].path);
			Dir_ValidDir(classInfo[i].path);
		}
		else
		{
			// Build the path for the resource class using the default 
			// elements.
			strcpy(classInfo[i].path, dataPath);
			strcat(classInfo[i].path, defaultResourcePath[i]);
		}
		
		// The overriding path.
		if(i == RC_TEXTURE && ArgCheckWith("-texdir2", 1))
		{
			M_TranslatePath(ArgNext(), classInfo[i].overridePath);
			Dir_ValidDir(classInfo[i].overridePath);
		}

		VERBOSE2( Con_Message("  %i: %s (%s)\n", i, 
			classInfo[i].path, classInfo[i].overridePath) );
	}
}

/*
 * Callback function used in R_TryResourceFile.
 */
int R_FileFinder(const char *fn, int parm)
{
	char *buf = (char*) parm;

	// This'll do fine!
	if(buf) strcpy(buf, fn);

	// Return false to stop searching.
	return false;
}

/*
 * Check all possible extensions to see if the resource exists.
 * 'path' is complete, sans extension. Returns true if it's found.
 */
boolean R_TryResourceFile
	(resourceclass_t resClass, const char *path, char *foundFileName)
{
	const char **ext;
	char buf[256], *extPlace;
	int i;

	strcpy(buf, path);
	extPlace = buf + strlen(buf);
	
	for(i = 0, ext = classExtension[resClass]; *ext; ext++)
	{
		if(**ext == '*') // Anything goes?
		{
			strcpy(extPlace, ".*");
			if(!F_ForAll(buf, (int) foundFileName, R_FileFinder))
			{
				// A match was found.
				return true;
			}
		}
		else
		{
			strcpy(extPlace, *ext);
			if(F_Access(buf))
			{
				// Found it.
				if(foundFileName) strcpy(foundFileName, buf);
				return true;
			}
		}
	}

	// No hits.
	return false;
}

/*
 * Attempt to locate an external file for the specified resource. Returns
 * true if successful. The file name is returned in the fileName parameter.
 * The fileName parm can be NULL, which makes the routine just check for
 * the existence of the file. Set optionalSuffix to NULL if it's not
 * needed.
 */
boolean R_FindResource(resourceclass_t resClass, const char *name, 
					   const char *optionalSuffix, char *fileName)
{
	resclass_t *info = &classInfo[resClass];
	const char *gameMode;
	char path[256];
	char fn[256];
	int i;
	
	// The tries:
	// 1. override + game
	// 2. override
	// 3. game
	// 4. default
	for(i = 0; i < 4; i++)
	{
		// First try the overriding path, if it's set.
		if(i < 2)
		{
			if(info->overridePath[0])
				strcpy(path, info->overridePath);
			else
				continue;
		}
		else
		{
			strcpy(path, info->path);
		}

		// Should the game mode subdir be included?
		if(i == 0 || i == 2)
		{
			// A string that identifies the game mode (e.g. doom2-plut).
			gameMode = gx.Get(DD_GAME_MODE);
			if(!gameMode || !gameMode[0]) continue; // Not set?!
			strcat(path, gameMode);
			strcat(path, "\\");
		}

		// First try with the optional suffix.
		if(optionalSuffix)
		{
			strcpy(fn, path);
			strcat(fn, name);
			strcat(fn, optionalSuffix);
			if(R_TryResourceFile(resClass, fn, fileName))
				return true;
		}

		// Try without a suffix.
		strcpy(fn, path);
		strcat(fn, name);
		if(R_TryResourceFile(resClass, fn, fileName))
			return true;
	}

	// Couldn't find anything.
	return false;
}