//===========================================================================
// S_WAV.H
//===========================================================================
#ifndef __DOOMSDAY_SOUND_WAVE_FILE_H__
#define __DOOMSDAY_SOUND_WAVE_FILE_H__

int		WAV_CheckFormat(char *data);
void *	WAV_Load(const char *filename, int *bits, int *rate, int *samples);
void *	WAV_MemoryLoad(byte *data, int datalength, int *bits, int *rate, int *samples);

#endif 