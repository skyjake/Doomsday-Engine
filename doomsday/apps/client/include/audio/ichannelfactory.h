/** @file ichannelfactory.h  Interface for (Factory) audio::Channel construction.
 * @ingroup audio
 *
 * @authors Copyright © 2003-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_AUDIO_ICHANNELFACTORY_H
#define CLIENT_AUDIO_ICHANNELFACTORY_H

#ifdef __SERVER__
#  error "audio" is not available in a SERVER build
#endif

#include "audio/channel.h"
#include <de/Record>
#include <QList>

namespace audio {

/**
 * Interface for any component able to produce Channels for use by/with the audio::System.
 *
 * Specialized factories are free to choose the concrete channel type and/or customize it
 * accordingly for the logical Channel::Type requested.
 */
class IChannelFactory
{
public:
    virtual ~IChannelFactory() {}

    /**
     * Returns a set of configuration Records describing the Channel formats that the
     * factory is capable of producing.
     *
     * Each record must contain at least the following required Values:
     *
     * - "identityKey" (Text)   : Unique textual, symbolic identifier (lowercase) for
     * "this" configuration, used in Config.
     *
     * - "channelType" (Number) : Logical Channel::Type identifier.
     *
     * @todo This configuration could also declare which audio formats it is capable of
     * playing (e.g., MIDI only, CD tracks only). -jk
     */
    virtual QList<de::Record> listInterfaces() const = 0;

    /**
     * Called when the audio::System needs a new playback Channel of the given @a type.
     *
     * @note: Ownership is currently retained!
     */
    virtual Channel *makeChannel(Channel::Type type) = 0;
};

}  // namespace audio

#endif  // CLIENT_AUDIO_ICHANNELFACTORY_H
