#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include <de/system.h>
#include <de/waveform.h>
#include <de/sound.h>
#include <de/path.h>
#include "../libgloom.h"

namespace gloom {

class ICamera;

class LIBGLOOM_PUBLIC AudioSystem : public de::System
{
public:
    /**
     * Native audio interface could not be accessed. @ingroup errors
     */
    DE_ERROR(NativeError);

public:
    AudioSystem();

    static AudioSystem &get();
    static bool isAvailable();

    /**
     * Prepares an audio waveform for playback. The returned ISound instance is in
     * paused state, ready for configuration and playing.
     *
     * This can either be called every time a new sound needs to be played, or one
     * can retain the ISound object and keep using it several times.
     *
     * @param waveform  Waveform data. AudioSystem does @em NOT take ownership or
     *                  copies of this data (as it may be large) but instead retains
     *                  the provided reference. The caller must ensure that the
     *                  Waveform remains in existence as long as it is used in the
     *                  AudioSystem.
     *
     * @return Sound instance for controlling and querying the playing sound. AudioSystem
     * retains ownership of all Sounds.
     */
    de::Sound &newSound(const de::Waveform &waveform);

    de::Sound &newSound(const de::DotPath &appWaveform);

    void timeChanged(const de::Clock &);

    void setListener(const ICamera *camera);

    const ICamera *listener() const;

public:
    DE_DEFINE_AUDIENCE(NewSound, void newSoundCreated(de::Sound &, const de::DotPath &name))

private:
    DE_PRIVATE(d)
};

} // namespace gloom

#endif // AUDIOSYSTEM_H
