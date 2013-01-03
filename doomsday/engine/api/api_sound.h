#ifndef DOOMSDAY_API_SOUND_H
#define DOOMSDAY_API_SOUND_H

#include "apis.h"
#include <de/types.h>

struct mobj_s;

DENG_API_TYPEDEF(S)
{
    de_api_t api;

    void (*MapChange)(void);

    int (*LocalSoundAtVolumeFrom)(int sound_id, struct mobj_s* origin, coord_t* pos, float volume);
    int (*LocalSoundAtVolume)(int soundID, struct mobj_s* origin, float volume);
    int (*LocalSound)(int soundID, struct mobj_s* origin);
    int (*LocalSoundFrom)(int soundID, coord_t* fixedpos);
    int (*StartSound)(int soundId, struct mobj_s* origin);
    int (*StartSoundEx)(int soundId, struct mobj_s* origin);
    int (*StartSoundAtVolume)(int soundID, struct mobj_s* origin, float volume);
    int (*ConsoleSound)(int soundID, struct mobj_s* origin, int targetConsole);

    /**
     * Stop playing sound(s), either by their unique identifier or by their
     * emitter(s).
     *
     * @param soundId       @c 0: stops all sounds emitted from the targeted origin(s).
     * @param origin        @c NULL: stops all sounds with the ID.
     *                      Otherwise both ID and origin must match.
     * @param flags         @ref soundStopFlags
     */
    void (*StopSound2)(int soundID, struct mobj_s* origin, int flags);

    void (*StopSound)(int soundID, struct mobj_s* origin/*,flags=0*/);

    int (*IsPlaying)(int soundID, struct mobj_s* origin);
    int (*StartMusic)(const char* musicID, boolean looped);
    int (*StartMusicNum)(int id, boolean looped);
    void (*StopMusic)(void);
    void (*PauseMusic)(boolean doPause);
}
DENG_API_T(S);

#ifndef DENG_NO_API_MACROS_SOUND
#define S_MapChange                 _api_S.MapChange
#define S_LocalSoundAtVolumeFrom    _api_S.LocalSoundAtVolumeFrom
#define S_LocalSoundAtVolume        _api_S.LocalSoundAtVolume
#define S_LocalSound                _api_S.LocalSound
#define S_LocalSoundFrom            _api_S.LocalSoundFrom
#define S_StartSound                _api_S.StartSound
#define S_StartSoundEx              _api_S.StartSoundEx
#define S_StartSoundAtVolume        _api_S.StartSoundAtVolume
#define S_ConsoleSound              _api_S.ConsoleSound
#define S_StopSound2                _api_S.StopSound2
#define S_StopSound                 _api_S.StopSound
#define S_IsPlaying                 _api_S.IsPlaying
#define S_StartMusic                _api_S.StartMusic
#define S_StartMusicNum             _api_S.StartMusicNum
#define S_StopMusic                 _api_S.StopMusic
#define S_PauseMusic                _api_S.PauseMusic
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(S);
#endif

#endif // DOOMSDAY_API_SOUND_H
