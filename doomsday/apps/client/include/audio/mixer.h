/** @file mixer.h  Audio channel mixer.
 * @ingroup audio
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_AUDIO_MIXER_H
#define CLIENT_AUDIO_MIXER_H

#include "audio/channel.h"
#include <de/Error>
#include <de/Observers>
#include <functional>

namespace audio {

/**
 * High-level logical audio mixer.
 *
 * A Mixer manages a set of Tracks mapped to playback Channels.
 *
 * @ingroup audio
 */
class Mixer
{
public:
    /**
     * Logical model of a currently playing audio track.
     *
     * Each may be mapped to one or more playback Channels.
     */
    class Track
    {
    public:
        /// Audience notified following a Channel mapping change.
        DENG2_DEFINE_AUDIENCE2(ChannelsRemapped, void trackChannelsRemapped(Track &));

        /**
         * Returns the owning Mixer instance.
         */
        Mixer       &mixer();
        Mixer const &mixer() const;

        /**
         * Returns the symbolic, unique identifier of the track.
         */
        de::String id() const;

        /**
         * Returns a human-friendly, textual title for the track, suitable for use in user
         * facing contexts such as the GUI and Log.
         *
         * @see setTitle()
         */
        de::String title() const;

        /**
         * Change the human-friendly title for the track to @a newTitle.
         *
         * @see title()
         */
        void setTitle(de::String const &newTitle);

        /**
         * Returns the total number of Channels mapped to the track.
         */
        de::dint channelCount() const;

        /**
         * Returns @c true if one or more playback Channel is currently mapped to the track.
         *
         * @see channel(), setChannel()
         */
        inline bool hasChannel() const { return channelCount() > 0; }

        /**
         * Add @a channel to the mapping for the track.
         *
         * @param channel  Channel to add to the mapping (ownership is unaffected). If the
         * channel is already mapped - nothing happens. The ChannelsRemapped audience is
         * notified whenever this mapping changes.
         *
         * @see channel()
         */
        Track &addChannel(Channel *channel);

        /**
         * Remove @a channel from the mapping for the track.
         *
         * @param channel  Channel to remove from the mapping (ownership is unaffected).
         * If the channel is not currently mapped - nothing happens. The ChannelsRemapped
         * audience is notified whenever this mapping changes.
         */
        Track &removeChannel(Channel *channel);

        /**
         * Iterate through the mapped Channels, executing @a callback for each.
         */
        de::LoopResult forAllChannels(std::function<de::LoopResult (Channel &)> callback) const;

    private:
        Track(Mixer &mixer, de::String const &trackId);
        friend class Mixer;
        DENG2_PRIVATE(d)
    };

public:
    /// Referenced track could not be located. @ingroup errors
    DENG2_ERROR(MissingTrackError);

    /**
     * Construct a new (default) mixer with no defined Tracks.
     */
    Mixer();
    virtual ~Mixer();

    /**
     * Returns @c true if the mixer has no configured Tracks.
     */
    inline bool isEmpty() { return trackCount() == 0; }

    /**
     * Locate the Track attributed with the given @a trackId.
     * @see findTrack()
     */
    inline Track &operator [] (de::String const &trackId) const {
        return findTrack(trackId);
    }

    /**
     * Clear all the defined Tracks, returning the mixer to the default (empty) state.
     */
    void clearTracks();

    /**
     * Returns @c true if the mixer contains a Track attributed with the given @a trackId.
     *
     * @see findTrack(), tryFindTrack()
     */
    bool hasTrack(de::String const &trackId) const;

    /**
     * Locate the Track attributed with the given @a trackId.
     *
     * @see hasTrack(), operator [], forAllTracks()
     */
    Track &findTrack(de::String const &trackId) const;

    /**
     * Returns a pointer to the Track attributed with the given @a trackId (if found);
     * otherwise @c nullptr.
     *
     * @see hasTrack(), findTrack(), forAllTracks()
     */
    Track *tryFindTrack(de::String const &trackId) const;

    /**
     * Iterate through the Tracks, executing @a callback for each.
     *
     * @see tryFindTrack()
     */
    de::LoopResult forAllTracks(std::function<de::LoopResult (Track &)> callback) const;

    /**
     * Returns the total number of Tracks.
     */
    de::dint trackCount() const;

    /**
     * Prepare a Track and configure it to use the given @a channel.
     *
     * @param trackId  Unique identifier for the track. If an existing Track with this
     * identifier is found, nothing happens and it is returned; otherwise a new Track is
     * created and configured with a mapping to the given @a channel.
     *
     * @return  The (perhaps newly configured) Track.
     */
    Track &makeTrack(de::String const &trackId, Channel *channel = nullptr);

private:
    DENG2_PRIVATE(d)
};

}  // namespace audio

// Debug visual: ------------------------------------------------------------------------

extern int showMixerInfo;
//extern byte refMonitor;

/**
 * Draws debug information on-screen.
 */
void UI_AudioMixerDrawer();

#endif  // CLIENT_AUDIO_MIXER_H
