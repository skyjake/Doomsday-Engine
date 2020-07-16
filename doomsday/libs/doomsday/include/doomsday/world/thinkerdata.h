/** @file thinkerdata.h  Base class for thinker private data.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
  */

#ifndef LIBDOOMSDAY_THINKERDATA_H
#define LIBDOOMSDAY_THINKERDATA_H

#include "thinker.h"

#include <de/scripting/process.h>
#include <de/id.h>
#include <de/scripting/iobject.h>
#include <de/iserializable.h>

/**
 * Base class for thinker private data.
 *
 * Contains internal functionality common to all thinkers regardless of their type.
 */
class LIBDOOMSDAY_PUBLIC ThinkerData
        : public Thinker::IData
        , public de::IObject
        , public de::ISerializable
        , public de::Deletable
{
public:
    DE_AUDIENCE(Deletion, void thinkerBeingDeleted(thinker_s &))

public:
    ThinkerData(const de::Id &id = de::Id::none());
    ThinkerData(const ThinkerData &other);

    /**
     * Returns the unique and persistent ID of the thinker.
     *
     * Note that due to historical reasons game-side mobj IDs are separately enumerated
     * 16-bit numbers.
     *
     * @return Thinker ID.
     *
     * @todo Use this for identifying all thinkers everywhere, including mobjs.
     */
    const de::Id &id() const;

    void setId(const de::Id &id);

    void setThinker(thinker_s *thinker) override;
    void think() override;
    IData *duplicate() const override;

    thinker_s &thinker();
    const thinker_s &thinker() const;

    /**
     * Initializes Doomsday Script bindings for the thinker. This is called
     * when the thinker is added to the world, so mobjs have been assigned
     * their IDs.
     */
    virtual void initBindings();

    // Implements IObject.
    de::Record &objectNamespace() override;
    const de::Record &objectNamespace() const override;

    // Implements ISerializable.
    void operator >> (de::Writer &to) const override;
    void operator << (de::Reader &from) override;

public:
    /*
     * Finds a thinker based on its unique identifier. This searches all ThinkerData
     * instances in existence at the moment. If there happens to be multiple ThinkerData
     * instances with the same ID, returns the most recently created instance.
     *
     * @param id  Identifier.
     * @return Thinker or @c nullptr.
     */
    static ThinkerData *find(const de::Id &id);

private:
    DE_PRIVATE(d)

#ifdef DE_DEBUG
public:
    struct DebugCounter {
        de::Id id;
        static de::duint32 total;

        DebugCounter()  { total++; }
        ~DebugCounter() { total--; }
    };
    DebugCounter _debugCounter;

    struct DebugValidator {
        DebugValidator()  { DE_ASSERT(DebugCounter::total == 0); }
        ~DebugValidator() { DE_ASSERT(DebugCounter::total == 0); }
    };
#endif
};

DE_SCRIPT_ARGUMENT_TYPE(ThinkerData *,
    if (!arg) return ScriptLex::NONE;
    return scriptArgumentAsText(arg->objectNamespace());
)

DE_SCRIPT_ARGUMENT_TYPE(const ThinkerData *,
    if (!arg) return ScriptLex::NONE;
    return scriptArgumentAsText(arg->objectNamespace());
)

#endif // LIBDOOMSDAY_THINKERDATA_H
