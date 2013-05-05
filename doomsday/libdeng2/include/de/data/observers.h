/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_OBSERVERS_H
#define LIBDENG2_OBSERVERS_H

#include "../libdeng2.h"
#include "../ReadWriteLockable"
#include "../Guard"

#include <QSet>

/**
 * Macro that forms the name of an observer interface.
 */
#define DENG2_AUDIENCE_INTERFACE(Name) \
    I##Name##Observer

/**
 * Macro for declaring an observer interface containing one method.
 *
 * @param Name    Name of the audience. E.g., "Deletion" produces @c DeletionAudience.
 * @param Method  The pure virtual method that the observer has to implement.
 *                The @c virtual keyword and <code>=0</code> are automatically included.
 */
#define DENG2_DECLARE_AUDIENCE(Name, Method) \
    class DENG2_AUDIENCE_INTERFACE(Name) { \
    public: \
        virtual ~DENG2_AUDIENCE_INTERFACE(Name)() {} \
        virtual Method = 0; \
    };

/**
 * Defines an audience. Typically used inside a class to define the observers
 * as a public member variable (used by DENG_DEFINE_AUDIENCE). Produces a
 * member variable called "audienceFor{Name}".
 *
 * @param Name  Name of the audience.
 */
#define DENG2_AUDIENCE(Name) \
    typedef de::Observers<DENG2_AUDIENCE_INTERFACE(Name)> Name##Audience; \
    Name##Audience audienceFor##Name;

#define DENG2_EXTERN_AUDIENCE(Name) \
    typedef de::Observers<DENG2_AUDIENCE_INTERFACE(Name)> Name##Audience; \
    extern Name##Audience audienceFor##Name;

/**
 * Macro for defining an observer interface containing a single method.
 *
 * @param Name    Name of the audience. E.g., "Deletion" produces @c DeletionAudience 
 *                and @c audienceForDeletion.
 * @param Method  The pure virtual method that the observer has to implement. 
 *                The @c virtual keyword and <code>=0</code> are automatically included.
 */
#define DENG2_DEFINE_AUDIENCE(Name, Method) \
    DENG2_DECLARE_AUDIENCE(Name, Method) \
    DENG2_AUDIENCE(Name)

/**
 * Macro that can be used in class declarations to specify which audiences the class
 * can belong to.
 *
 * @param Type      Name of the type where the audience is defined.
 * @param Audience  Audience name.
 */
#define DENG2_OBSERVES(Type, Audience) public Type::I##Audience##Observer

/**
 * Macro for looping through all observers. @note The @a Audience type needs to be defined
 * in the scope.
 *
 * @param SetName  Name of the observer set class.
 * @param Var      Variable used in the loop.
 * @param Name     Name of the observer set.
 */
#define DENG2_FOR_EACH_OBSERVER(SetName, Var, Name) for(SetName::Loop Var(Name); !Var.done(); ++Var)

/**
 * Macro for looping through the audience members.
 *
 * @param Name  Name of the audience.
 * @param Var   Variable used in the loop.
 */
#define DENG2_FOR_AUDIENCE(Name, Var) \
    DENG2_FOR_EACH_OBSERVER(Name##Audience, Var, audienceFor##Name)

/**
 * Macro for looping through the public audience members from inside a private
 * implementation.
 *
 * @param Name  Name of the audience.
 * @param Var   Variable used in the loop.
 */
#define DENG2_FOR_PUBLIC_AUDIENCE(Name, Var) \
    DENG2_FOR_EACH_OBSERVER(Name##Audience, Var, self.audienceFor##Name)

namespace de {

/**
 * Template for observer sets. The template type should be an interface
 * implemented by all the observers. @ingroup data
 *
 * @par Thread-safety
 *
 * Observers and Observers::Loop lock the observer set separately for reading
 * and writing as appropriate.
 */
template <typename Type>
class Observers : public ReadWriteLockable
{
public:
    typedef QSet<Type *> Members;
    typedef typename Members::iterator iterator;
    typedef typename Members::const_iterator const_iterator;
    typedef typename Members::size_type size_type;

    /**
     * Iteration utility for observers. This (or @c foreach) should be used when
     * notifying observers, because it is safe against the observer removing
     * itself from the observer set, or the set itself being destroyed.
     */
    class Loop {
    public:
        Loop(Observers &observers) {
            DENG2_GUARD_READ(observers);
            _observers = observers._members;
            _next = _observers.begin();
            next();
        }
        bool done() {
            return _current == _observers.end();
        }
        void next() {
            _current = _next;
            if(_next != _observers.end()) {
                ++_next;
            }
        }
        iterator &get() {
            return _current;
        }
        Type *operator -> () {
            return *get();
        }
        Loop &operator ++ () {
            next();
            return *this;
        }
    private:
        Members _observers;
        iterator _current;
        iterator _next;
    };

    friend class Loop;

public:
    Observers() {}

    Observers(Observers<Type> const &other) {
        *this = other;
    }

    virtual ~Observers() {
        clear();
    }

    void clear() {
        DENG2_GUARD_WRITE(this);
        _members.clear();
    }

    Observers<Type> &operator = (Observers<Type> const &other) {
        if(this == &other) return *this;
        DENG2_GUARD_READ(other);
        DENG2_GUARD_WRITE(this);
        _members = other._members;
        return *this;
    }

    /// Add an observer into the set. The set does not receive
    /// ownership of the observer instance.
    void add(Type *observer) {
        DENG2_GUARD_WRITE(this);
        _members.insert(observer);
    }

    Observers<Type> &operator += (Type *observer) {
        add(observer);
        return *this;
    }

    Observers<Type> &operator += (Type &observer) {
        add(&observer);
        return *this;
    }

    Observers<Type> const &operator += (Type *observer) const {
        const_cast<Observers<Type> *>(this)->add(observer);
        return *this;
    }

    void remove(Type *observer) {
        DENG2_GUARD_WRITE(this);
        _members.remove(observer);
    }

    Observers<Type> &operator -= (Type *observer) {
        remove(observer);
        return *this;
    }

    Observers<Type> &operator -= (Type &observer) {
        remove(&observer);
        return *this;
    }

    Observers<Type> const &operator -= (Type *observer) const {
        const_cast<Observers<Type> *>(this)->remove(observer);
        return *this;
    }

    size_type size() const {
        DENG2_GUARD_READ(this);
        return _members.size();
    }

    bool isEmpty() const {
        return size() == 0;
    }

    iterator begin() {
        return _members.begin();
    }

    iterator end() {
        return _members.end();
    }

    const_iterator begin() const {
        return _members.constBegin();
    }

    const_iterator end() const {
        return _members.constEnd();
    }

private:
    Members _members;
};

} // namespace de

#endif /* LIBDENG2_OBSERVERS_H */
