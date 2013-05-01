/** @file walldiv.h Wall geometry divisions.
 * @ingroup render
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_WALLDIV_H
#define DENG_RENDER_WALLDIV_H

#include <de/Error>

/// Maximum number of walldivnode_ts in a walldivs_t dataset.
#define WALLDIVS_MAX_INTERCEPTS          64

namespace de {

class WallDivs
{
public:
    /// Required intercept is missing. @ingroup errors
    DENG2_ERROR(MissingInterceptError);

    class Intercept
    {
    protected:
        Intercept(ddouble distance = 0);

    public:
        bool operator < (Intercept const &other) const {
            return _distance < other._distance;
        }

        /**
         * Determine the distance between "this" and the @a other intercept.
         */
        ddouble operator - (Intercept const &other) const {
            return _distance - other._distance;
        }

        /**
         * Returns distance along the half-plane relative to the origin.
         */
        ddouble distance() const { return _distance; }

        bool hasNext() const
        {
            int idx = this - _wallDivs->_intercepts;
            return (idx + 1 < _wallDivs->_interceptCount);
        }

        bool hasPrev() const
        {
            int idx = this - _wallDivs->_intercepts;
            return (idx > 0);
        }

        Intercept &next() const
        {
            int idx = this - _wallDivs->_intercepts;
            if(idx + 1 < _wallDivs->_interceptCount)
            {
                return _wallDivs->_intercepts[idx+1];
            }
            throw WallDivs::MissingInterceptError("WallDivs::Intercept", "No next neighbor");
        }

        Intercept &prev() const
        {
            int idx = this - _wallDivs->_intercepts;
            if(idx > 0)
            {
                return _wallDivs->_intercepts[idx-1];
            }
            throw WallDivs::MissingInterceptError("WallDivs::Intercept", "No previous neighbor");
        }

#ifdef DENG_DEBUG
        void debugPrint() const;
#endif

        friend class WallDivs;

    private:
        /// Distance along the half-plane relative to the origin.
        ddouble _distance;

        WallDivs *_wallDivs;
    };

    typedef Intercept Intercepts[WALLDIVS_MAX_INTERCEPTS];

public:
    WallDivs() : _interceptCount(0)
    {
        std::memset(_intercepts, 0, sizeof(_intercepts));
    }

    int count() const { return _interceptCount; }

    inline bool isEmpty() const { return count() == 0; }

    Intercept &first() const
    {
        if(_interceptCount > 0)
        {
            return const_cast<Intercept &>(_intercepts[0]);
        }
        throw MissingInterceptError("WallDivs::first", "Intercepts list is empty");
    }

    Intercept &last() const
    {
        if(_interceptCount > 0)
        {
            return const_cast<Intercept &>(_intercepts[_interceptCount-1]);
        }
        throw MissingInterceptError("WallDivs::last", "Intercepts list is empty");
    }

    bool intercept(ddouble distance);

    void sort();

#ifdef DENG_DEBUG
    void printIntercepts() const;
#endif

    /**
     * Returns the list of intercepts for the half-plane for efficient traversal.
     *
     * @note This list may or may not yet be sorted. If a sorted list is desired
     * then sortAndMergeIntercepts() should first be called.
     *
     * @see interceptLineSegment(), intercepts()
     */
    Intercepts const &intercepts() const;

private:
    Intercept *find(ddouble distance) const;
    void assertSorted() const;

    int _interceptCount;
    Intercepts _intercepts;
};

} // namespace de

#endif // DENG_RENDER_WALLDIV_H
