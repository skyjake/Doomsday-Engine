/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jamie Jones <yagisan@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

/**
* Desgin and Implmentation notes, for other hackers.
* MIDI, and External Audio is handled by SDL_sound. CD Audio is to be handled
* by native SDL, and ideally directed into SDL_sound for further processing.
*
* SDL_sound has no concept of channels
*/


// HEADER FILES ------------------------------------------------------------

#include "driver_sdlsound.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int     DS_Init(void);
void    DS_Shutdown(void);
sfxbuffer_t *DS_CreateBuffer(int flags, int bits, int rate);
void    DS_DestroyBuffer(sfxbuffer_t *buf);
void    DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample);
void    DS_Reset(sfxbuffer_t *buf);
void    DS_Play(sfxbuffer_t *buf);
void    DS_Stop(sfxbuffer_t *buf);
void    DS_Refresh(sfxbuffer_t *buf);
void    DS_Event(int type);
void    DS_Set(sfxbuffer_t *buf, int property, float value);
void    DS_Setv(sfxbuffer_t *buf, int property, float *values);
void    DS_Listener(int property, float value);
void    DS_Listenerv(int property, float *values);

/** The Ext music interface. */
int     DM_Ext_Init(void);
void    DM_Ext_Update(void);
void    DM_Ext_Set(int property, float value);
int     DM_Ext_Get(int property, void *value);
void    DM_Ext_Pause(int pause);
void    DM_Ext_Stop(void);
void   *DM_Ext_SongBuffer(int length);
int     DM_Ext_PlayFile(const char *filename, int looped);
int     DM_Ext_PlayBuffer(int looped);

/** The MUS music interface. */
int     DM_Mus_Init(void);
void    DM_Mus_Update(void);
void    DM_Mus_Set(int property, float value);
int     DM_Mus_Get(int property, void *value);
void    DM_Mus_Pause(int pause);
void    DM_Mus_Stop(void);
void   *DM_Mus_SongBuffer(int length);
int     DM_Mus_Play(int looped);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------
boolean sdl_sound_init = false;
Sound_Version compiled;
Sound_Version linked;
Sound_DecoderInfo **decoder_info;
static int verbose;

// CODE --------------------------------------------------------------------

static void message(const char *message)
{
    Con_Message("SDL_Sound: %s\n", message);
}

void DS_Error(void)
{
    char        buf[500];

    sprintf(buf, "ERROR: %s", Sound_GetError());
    message(buf);
}

int DS_Init(void)
{
    if(sdl_sound_init)
        return true;
    Con_Message("Initialising SDL_sound\n");
    if(SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        message(SDL_GetError());
        return false;
    }

    if(!Sound_Init())
    {
        message(Sound_GetError());
        return false;
    }
    SOUND_VERSION(&compiled);
    Sound_GetLinkedVersion(&linked);
    Con_Message("SDL_Sound: Compiled with SDL_sound version %d.%d.%d\n",compiled.major, compiled.minor, compiled.patch);
    Con_Message("SDL_Sound: Linked with SDL_sound version %d.%d.%d\n", linked.major, linked.minor, linked.patch);
    if ((compiled.major < linked.major) || \
       ((compiled.minor < linked.minor) && (compiled.major == linked.major)) || \
       ((compiled.patch < linked.patch) && (compiled.minor == linked.minor) && (compiled.major == linked.major)))
    {
       Con_Message("ERROR: SDL_Sound: Linked SDL_sound library out of date\n");
        return false;
    }

/* === */

    const Sound_DecoderInfo **decoder_info = Sound_AvailableDecoders();
    const Sound_DecoderInfo **i;
    const char **ext;

    Con_Message("SDL_Sound: Supported sound decoders:\n");
    if (decoder_info == NULL)
    {
        Con_Message("ERROR: SDL_Sound: No supported sound decoders\n");
        return false;
    }
    else
    {
        for (i = decoder_info; *i != NULL; i++)
        {
            Con_Message("SDL_Sound: %s decoder\n", (*i)->description);
            if((verbose = ArgExists("-verbose")))
               {
               Con_Message("           supports: ");
                   for (ext = (*i)->extensions; *ext != NULL; ext++)
                   Con_Message("%s, ", *ext);
                   Con_Message("\n");
               }
        } /* for */
    } /* else */


/* === */
    sdl_sound_init = true;
    return true;
}

void DS_Shutdown(void)
{
    if(!sdl_sound_init)
        return;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    Sound_Quit();
    sdl_sound_init = false;
}

sfxbuffer_t *DS_CreateBuffer(int flags, int bits, int rate)
{
    Con_Message("Stub: SDL_Sound: DS_CreateBuffer\n");
}

void DS_DestroyBuffer(sfxbuffer_t *buf)
{
    Con_Message("Stub: SDL_Sound: DS_DestroyBuffer\n");
}

void DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample)
{
    Con_Message("Stub: SDL_Sound: DS_Load\n");
}

void DS_Reset(sfxbuffer_t *buf)
{
    Con_Message("Stub: SDL_Sound: DS_Reset\n");
}

void DS_Play(sfxbuffer_t *buf)
{
    Con_Message("Stub: SDL_Sound: DS_Play\n");
}

void DS_Stop(sfxbuffer_t *buf)
{
    Con_Message("Stub: SDL_Sound: DS_Stop\n");
}

void DS_Refresh(sfxbuffer_t *buf)
{
    Con_Message("Stub: SDL_Sound: DS_Refresh\n");
}

void DS_Event(int type)
{
    Con_Message("Stub: SDL_Sound: DS_Event\n");
}

void DS_Set(sfxbuffer_t *buf, int property, float value)
{
    Con_Message("Stub: SDL_Sound: DS_Set\n");
}

void DS_Setv(sfxbuffer_t * buf, int property, float *values)
{
    Con_Message("Stub: SDL_Sound: DS_Setv\n");
}

void DS_Listener(int property, float value)
{
    Con_Message("Stub: SDL_Sound: DS_Listener\n");
}

void SetEnvironment(float *rev)
{
    Con_Message("Stub: SDL_Sound: SetEnvironment\n");
}

void DS_Listenerv(int property, float *values)
{
    Con_Message("Stub: SDL_Sound: DS_Listenerv\n");
}
