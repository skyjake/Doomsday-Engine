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
        Intercept() : _wallDivs(0), _distance(0) {}

    public:
        ddouble operator - (Intercept const &other) const
        {
            return distance() - other.distance();
        }

        bool operator < (Intercept const &other) const
        {
            return distance() < other.distance();
        }

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

        friend class WallDivs;

    private:
        WallDivs *_wallDivs;
        ddouble _distance;
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

    void append(ddouble distance)
    {
        Intercept *node = &_intercepts[_interceptCount++];
        node->_wallDivs = this;
        node->_distance = distance;
    }

    Intercept *find(ddouble distance) const
    {
        for(int i = 0; i < _interceptCount; ++i)
        {
            Intercept *icpt = const_cast<Intercept *>(&_intercepts[i]);
            if(icpt->distance() == distance)
                return icpt;
        }
        return 0;
    }

    static int compareIntercepts(void const *e1, void const *e2)
    {
        ddouble const delta = (*reinterpret_cast<Intercept const *>(e1)) - (*reinterpret_cast<Intercept const *>(e2));
        if(delta > 0) return 1;
        if(delta < 0) return -1;
        return 0;
    }

    void sort()
    {
        if(count() < 2) return;

        // Sorting is required. This shouldn't take too long...
        // There seldom are more than two or three intercepts.
        qsort(_intercepts, _interceptCount, sizeof(*_intercepts), compareIntercepts);
        assertSorted();
    }

#ifdef DENG_DEBUG
    void debugPrint() const
    {
        LOG_DEBUG("WallDivs [%p]:") << de::dintptr(this);
        for(int i = 0; i < _interceptCount; ++i)
        {
            Intercept const *node = &_intercepts[i];
            LOG_DEBUG("  %i: %f") << i << node->distance();
        }
    }
#endif

    Intercepts const &intercepts() const
    {
        return _intercepts;
    }

private:
    /**
     * Ensure the intercepts are sorted (in ascending distance order).
     */
    void assertSorted() const
    {
#ifdef DENG_DEBUG
        if(isEmpty()) return;

        WallDivs::Intercept *node = &first();
        ddouble farthest = node->distance();
        forever
        {
            DENG2_ASSERT(node->distance() >= farthest);
            farthest = node->distance();

            if(!node->hasNext()) break;
            node = &node->next();
        }
#endif
    }

    int _interceptCount;
    Intercepts _intercepts;
};

} // namespace de

#endif // DENG_RENDER_WALLDIV_H
