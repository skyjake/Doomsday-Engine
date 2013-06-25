/** @file rend_bias.h Shadow Bias lighting model.
 *
 * Calculating macro-scale lighting on the fly.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_SHADOWBIAS
#define DENG_RENDER_SHADOWBIAS

#include <de/Vector>

#define MAX_BIAS_SOURCES    (8 * 32) // Hard limit due to change tracking.

// Vertex illumination flags.
#define VIF_LERP            0x1 ///< Interpolation is in progress.
#define VIF_STILL_UNSEEN    0x2 ///< The color of the vertex is still unknown.

/**
 * POD structure used with Shadow Bias to store per-vertex illumination data.
 */
struct VertexIllum
{
    /// Maximum number of sources which can contribute light to the vertex.
    static int const MAX_AFFECTED = 6;

    de::Vector3f color; ///< Current light intensity at the vertex.
    de::Vector3f dest;  ///< Destination light intensity at the vertex (interpolated to).
    uint updateTime;    ///< When the value was calculated.
    short flags;

    /**
     * Light contribution from affecting sources.
     */
    struct Contribution
    {
        short source;       ///< Index of the source.
        de::Vector3f color; ///< The contributed light intensity.
    } casted[MAX_AFFECTED];

    VertexIllum()
        : updateTime(0),
          flags(VIF_STILL_UNSEEN)
    {
        for(int i = 0; i < MAX_AFFECTED; ++i)
        {
            casted[i].source = -1;
        }
    }
};

/**
 * Interpolate between current and destination.
 */
void lerpIllumination(VertexIllum &illum, uint currentTime, int lightSpeed,
                      de::Vector3f &result);

class BiasTracker
{
public:
    static int const MAX_TRACKED = (MAX_BIAS_SOURCES / 8);

public:
    BiasTracker() { de::zap(_changes); }

    /**
     * Sets/clears a bit in the tracker for the given index.
     */
    void mark(uint index);

    /**
     * Checks if the given index bit is set in the tracker.
     */
    int check(uint index) const;

    /**
     * Copies changes from src.
     */
    void apply(BiasTracker const &src);

    /**
     * Clears changes of src.
     */
    void clear(BiasTracker const &src);

private:
    uint _changes[MAX_TRACKED];
};

#endif // DENG_RENDER_SHADOWBIAS
