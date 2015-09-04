/** @file sdlmixerdriver.cpp  Audio driver for playback using SDL_mixer.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <yagisan@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_DISABLE_SDLMIXER
#ifndef CLIENT_AUDIO_SDLMIXERDRIVER_H
#define CLIENT_AUDIO_SDLMIXERDRIVER_H

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include <de/liblegacy.h>
#include "api_audiod.h"
#include "api_audiod_sfx.h"

#include "audio/system.h"
#include <de/String>

namespace audio {

/**
 * Provides an audio driver for playback usnig SDL_mixer.
 */
class SdlMixerDriver : public audio::System::IDriver
{
public:
    SdlMixerDriver();

    /**
     * If the driver is still initialized it will be automatically deinitialized
     * when this is called.
     */
    virtual ~SdlMixerDriver();

public:  // Implements audio::System::IDriver: -----------------------------------

    void initialize();
    void deinitialize();

    Status status() const;

    de::String description() const;
    de::String identityKey() const;
    de::String title() const;

    bool hasCd() const;
    bool hasMusic() const;
    bool hasSfx() const;

    audiointerface_cd_t /*const*/ &iCd() const;
    audiointerface_music_t /*const*/ &iMusic() const;
    audiointerface_sfx_t /*const*/ &iSfx() const;
    de::String interfaceName(void *playbackInterface) const;

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

#endif  // CLIENT_AUDIO_SDLMIXERDRIVER_H
#endif  // !DENG_DISABLE_SDLMIXER
