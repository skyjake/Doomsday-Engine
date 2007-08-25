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

// CODE --------------------------------------------------------------------

void DS_Error(void)
{
}

int DS_Init(void)
{
}

void DS_Shutdown(void)
{
}

sfxbuffer_t *DS_CreateBuffer(int flags, int bits, int rate)
{
}

void DS_DestroyBuffer(sfxbuffer_t *buf)
{
}

void DS_Load(sfxbuffer_t *buf, struct sfxsample_s *sample)
{
}

void DS_Reset(sfxbuffer_t *buf)
{
}

void DS_Play(sfxbuffer_t *buf)
{
}

void DS_Stop(sfxbuffer_t *buf)
{
}

void DS_Refresh(sfxbuffer_t *buf)
{
}

void DS_Event(int type)
{
}

void DS_Set(sfxbuffer_t *buf, int property, float value)
{
}

void DS_Setv(sfxbuffer_t * buf, int property, float *values)
{
}

void DS_Listener(int property, float value)
{
}

void SetEnvironment(float *rev)
{
}

void DS_Listenerv(int property, float *values)
{
}
