/**@file mixer.h  Object-oriented model for a logical audio mixer.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef WINMM_MIXER_H
#define WINMM_MIXER_H

#include <de/libcore.h>

/**
 * Models a logical audio mixer with one or more Line(Outs).
 */
class Mixer
{
public:
    /// Mixer/Line is not ready for use. @ingroup errors
    DENG2_ERROR(ReadyError);

    /// An unknown mixer Line was referenced. @ingroup errors
    DENG2_ERROR(MissingLineError);

    /**
     * Models a logical Line(Out).
     */
    class Line
    {
    public:
        enum Type
        {
            Cd,    ///< CD Audio.
            Synth  ///< Synthesizer.
        };

    public:
        /**
         * Construct a new line(out) of the @a type specified and initialize it.
         *
         * @param mixer  Parent Mixer instance.
         * @param type   Logical line out type.
         */
        Line(Mixer &mixer, Type type);
        virtual ~Line();

        /**
         * Returns @c true if the line is initialized and ready for use.
         */
        bool isReady() const;

        /**
         * Returns the parent Mixer instance for the line(out).
         */
        Mixer       &mixer();
        Mixer const &mixer() const;

        /**
         * Change the line out volume to @a newVolume.
         */
        void setVolume(float newVolume);

        /**
         * Returns the current line out volume.
         */
        int volume() const;

    private:
        DENG2_PRIVATE(d)
    };

    // There is one Line for each logical type.
    typedef Line::Type LineId;

public:
    /**
     * Construct a new logical mixer and initialize it, ready for use.
     */
    Mixer();
    virtual ~Mixer();

    /**
     * Returns @c true if the mixer is initialized and ready for use.
     */
    bool isReady() const;

    /**
     * Lookup a mixer Line by it's unique @a lineId.
     */
    Line       &line(LineId lineId);
    Line const &line(LineId lineId) const;

    inline Line       &cdLine   ()       { return line(LineId::Cd);   }
    inline Line const &cdLine   () const { return line(LineId::Cd);   }

    inline Line       &synthLine()       { return line(LineId::Synth); }
    inline Line const &synthLine() const { return line(LineId::Synth); }

    void *getHandle() const;

public:
    /**
     * Query the number of Mixer devices on the host system.
     */
    static de::dint deviceCount();
    
private:
    DENG2_PRIVATE(d)
};

#endif  // WINMM_MIXER_H
