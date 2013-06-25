/** @file grabbable.h Grabbable.
 *
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_WORLD_GRABBABLE_H
#define DENG_WORLD_GRABBABLE_H

#include <de/Error>
#include <de/Observers>
#include <de/Vector>

/**
 * Abstract base for any class whose instances can be manipulated and/or moved
 * by proxy once "grabbed". Conceptually a grabbable is similar to a reference
 * counter combined with an API which imposes additional restrictions to the
 * behavior/semantics of interactions with the derived instance(s).
 *
 * All grabbables have a map coordinate space origin. Each instance may be put
 * into a "locked" state where this origin is considered immutable. It should
 * be noted that this lock is a @em logical concept which is only enforced by
 * this interface (it may still be moved by some other means provided either
 * the OriginChange audience is notified, or the old origin is respected).
 */
class Grabbable
{
    DENG2_NO_COPY  (Grabbable)
    DENG2_NO_ASSIGN(Grabbable)

public:
    /// Base class for all grab errors. @ingroup errors
    DENG2_ERROR(GrabError);

    /// Base class for all ungrab errors. @ingroup errors
    DENG2_ERROR(UngrabError);

    /// Base class for all lock errors. @ingroup errors
    DENG2_ERROR(LockError);

    /// Base class for all unlock errors. @ingroup errors
    DENG2_ERROR(UnlockError);

    /*
     * Notified when the grabbable is about to be deleted.
     */
    DENG2_DEFINE_AUDIENCE(Deletion, void grabbableBeingDeleted(Grabbable &grabbable))

    /*
     * Notified when the lock state of the grabbable changes.
     */
    DENG2_DEFINE_AUDIENCE(LockChange, void grabbableLockChanged(Grabbable &grabbable))

    /*
     * Notified when the origin of the grabbable changes.
     */
    DENG2_DEFINE_AUDIENCE(OriginChange, void grabbableOriginChanged(Grabbable &grabbable))

public:
    /**
     * Construct a new grabbable (initial state is ungrabbed and unlocked).
     */
    Grabbable();

    virtual ~Grabbable();

    template <typename Type>
    Type &as() {
        DENG2_ASSERT(dynamic_cast<Type *>(this) != 0);
        return *static_cast<Type *>(this);
    }

    template <typename Type>
    Type const &as() const {
        DENG2_ASSERT(dynamic_cast<Type const *>(this) != 0);
        return *static_cast<Type const *>(this);
    }

    /**
     * Returns @c true iff the grabbable is currently grabbed.
     */
    bool isGrabbed() const;

    /**
     * Attempt to grab the grabbable (ownership is unaffected). The default
     * implementation assumes no preconditions therefore the grab succeeds.
     *
     * Derived classes may overide this for specialized grab behavior. If the
     * grab succeeds the implementor should call @ref addGrab() otherwise;
     * throw a new GrabError.
     */
    virtual void grab();

    /**
     * Attempt to ungrab the grabbable (ownership is unaffected). The default
     * implementation assumes no preconditions therefore the ungrab succeeds.
     *
     * Derived classes may overide this for specialized ungrab behavior. If the
     * ungrab succeeds the implementor should call @ref decGrab() otherwise;
     * throw a new UngrabError.
     */
    virtual void ungrab();

    /**
     * Returns @c true iff the grabbable is currently locked. The LockChange
     * audience is notified whenever the lock state changes.
     *
     * @see setLock()
     */
    bool isLocked() const;

    /**
     * Lock the grabbable if unlocked (preventing it from being moved). The
     * default implementation assumes no further preconditions and therefore
     * the lock succeeds.
     *
     * Derived classes may overide this for specialized lock behavior. If the
     * lock succeeds the implementor should use @code setLock(true) @endcode to
     *  enable the lock otherwise; throw a new LockError.
     *
     * @see isLocked(), unlock()
     */
    virtual void lock() { setLock(true); }

    /**
     * Unlock the grabbable if locked (allowing it to be moved). The default
     * implementation assumes no further preconditions and therefore the unlock
     * succeeds.
     *
     * Derived classes may overide this for specialized unlock behavior. If the
     * unlock succeeds the implementor should use @code setLock(false) @endcode
     * to disable the lock otherwise; throw a new UnlockError.
     *
     * @see lock(), unlock()
     */
    virtual void unlock() { setLock(false); }

    /**
     * Attempt to move the grabbable. Note that the move will be denied if the
     * grabbable is currently locked (nothing will happen).
     */
    void move(de::Vector3d const &newOrigin);

    /**
     * Returns the origin of the grabbable in the map coordinate space. The
     * OriginChange audience must be notified whenever the origin changes.
     *
     * @see setOrigin()
     */
    virtual de::Vector3d const &origin() const = 0;

    /**
     * Change the origin of the grabbable in the map coordinate space. The
     * OriginChange audience must be notified whenever the origin changes.
     * The default implementation assumes the grabbable cannot be moved and
     * does nothing.
     *
     * @param newOrigin  New origin coordinates to apply.
     *
     * @see origin()
     */
    virtual void setOrigin(de::Vector3d const &newOrigin) {}

protected:
    /**
     * Increment the grab count. Derived classes must call this when a grab
     * attempt is deemed to succeed.
     */
    void addGrab();

    /**
     * Decrement the grab count. Derived classes must call this when an ungrab
     * attempt is deemed to succeed.
     */
    void decGrab();

    /**
     * Change the lock state of the grabbable. Derived classes use this to
     * manipulate the lock state. Repeat attempts to enable/disable the lock
     * are ignored.
     */
    void setLock(bool enable = true);

private:
    int _grabs;
    bool _locked;
};

#endif // DENG_WORLD_GRABBABLE_H
