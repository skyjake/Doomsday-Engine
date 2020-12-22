/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_OBSERVERS_H
#define LIBCORE_OBSERVERS_H

#include "de/libcore.h"
#include "de/lockable.h"
#include "de/guard.h"
#include "de/pointerset.h"

/**
 * Macro that forms the name of an observer interface.
 */
#define DE_AUDIENCE_INTERFACE(Name) \
    I##Name##Observer

/**
 * Macro for declaring an observer interface containing one method.
 *
 * @param Name    Name of the audience. E.g., "Deletion" produces @c DeletionAudience.
 * @param Method  The pure virtual method that the observer has to implement.
 *                The @c virtual keyword and <code>=0</code> are automatically included.
 */
#define DE_DECLARE_AUDIENCE(Name, Method) \
    class DE_AUDIENCE_INTERFACE(Name) : public de::ObserverBase { \
    public: \
        virtual ~DE_AUDIENCE_INTERFACE(Name)() = default; \
        virtual Method = 0; \
    };

/**
 * Defines an audience. Typically used inside a class to define the observers
 * as a public member variable (used by DE_DEFINE_AUDIENCE). Produces a
 * member variable called "audienceFor{Name}".
 *
 * @param Name  Name of the audience.
 */
#define DE_AUDIENCE_VAR(Name) \
    using Name##Audience = de::Observers<DE_AUDIENCE_INTERFACE(Name)>; \
    Name##Audience audienceFor##Name;

#define DE_EXTERN_AUDIENCE(Name) \
    using Name##Audience = de::Observers<DE_AUDIENCE_INTERFACE(Name)>; \
    DE_PUBLIC extern Name##Audience audienceFor##Name;

#define DE_DECLARE_AUDIENCE_METHOD(Name) \
    using Name##Audience = de::Observers<DE_AUDIENCE_INTERFACE(Name)>; \
    Name##Audience &audienceFor##Name(); \
    const Name##Audience &audienceFor##Name() const;

#define DE_AUDIENCE_METHOD(ClassName, Name) \
    ClassName::Name##Audience &ClassName::audienceFor##Name() { return d->audienceFor##Name; } \
    const ClassName::Name##Audience &ClassName::audienceFor##Name() const { return d->audienceFor##Name; }

#define DE_AUDIENCE_METHOD_INLINE(Name) \
    using Name##Audience = de::Observers<DE_AUDIENCE_INTERFACE(Name)>; \
    Name##Audience _audienceFor##Name; \
    Name##Audience &audienceFor##Name() { return _audienceFor##Name; } \
    const Name##Audience &audienceFor##Name() const { return _audienceFor##Name; }

#define DE_PIMPL_AUDIENCE(Name) \
    Name##Audience audienceFor##Name;

/**
 * Macro for defining an observer interface containing a single method.
 *
 * @param Name    Name of the audience. E.g., "Deletion" produces @c DeletionAudience
 *                and @c audienceForDeletion.
 * @param Method  The pure virtual method that the observer has to implement.
 *                The @c virtual keyword and <code>=0</code> are automatically included.
 */
#define DE_DEFINE_AUDIENCE(Name, Method) \
    DE_DECLARE_AUDIENCE(Name, Method) \
    DE_AUDIENCE_VAR(Name)

#define DE_AUDIENCE(Name, Method) \
    DE_DECLARE_AUDIENCE(Name, Method) \
    DE_DECLARE_AUDIENCE_METHOD(Name)

#define DE_AUDIENCE_INLINE(Name, Method) \
    DE_DECLARE_AUDIENCE(Name, Method) \
    DE_AUDIENCE_METHOD_INLINE(Name)

// Variadic conveniences:

#if !defined (_MSC_VER)
#  define DE_PIMPL_AUDIENCES_1(x)      DE_PIMPL_AUDIENCE(x)
#  define DE_PIMPL_AUDIENCES_2(x, ...) DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_1(__VA_ARGS__)
#  define DE_PIMPL_AUDIENCES_3(x, ...) DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_2(__VA_ARGS__)
#  define DE_PIMPL_AUDIENCES_4(x, ...) DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_3(__VA_ARGS__)
#  define DE_PIMPL_AUDIENCES_5(x, ...) DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_4(__VA_ARGS__)
#  define DE_PIMPL_AUDIENCES_6(x, ...) DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_5(__VA_ARGS__)
#  define DE_PIMPL_AUDIENCES_7(x, ...) DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_6(__VA_ARGS__)
#  define DE_PIMPL_AUDIENCES_8(x, ...) DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_7(__VA_ARGS__)

#  define DE_PIMPL_AUDIENCES_(N, ...) DE_CONCAT(DE_PIMPL_AUDIENCES_, N)(__VA_ARGS__)
#  define DE_PIMPL_AUDIENCES(...) \
    DE_PIMPL_AUDIENCES_(DE_NUM_ARG(__VA_ARGS__), __VA_ARGS__)

#  define DE_AUDIENCE_METHODS_1(ClassName, x)      DE_AUDIENCE_METHOD(ClassName, x)
#  define DE_AUDIENCE_METHODS_2(ClassName, x, ...) DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_1(ClassName, __VA_ARGS__)
#  define DE_AUDIENCE_METHODS_3(ClassName, x, ...) DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_2(ClassName, __VA_ARGS__)
#  define DE_AUDIENCE_METHODS_4(ClassName, x, ...) DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_3(ClassName, __VA_ARGS__)
#  define DE_AUDIENCE_METHODS_5(ClassName, x, ...) DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_4(ClassName, __VA_ARGS__)
#  define DE_AUDIENCE_METHODS_6(ClassName, x, ...) DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_5(ClassName, __VA_ARGS__)
#  define DE_AUDIENCE_METHODS_7(ClassName, x, ...) DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_6(ClassName, __VA_ARGS__)
#  define DE_AUDIENCE_METHODS_8(ClassName, x, ...) DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_7(ClassName, __VA_ARGS__)

#  define DE_AUDIENCE_METHODS_(N, ClassName, ...) DE_CONCAT(DE_AUDIENCE_METHODS_, N)(ClassName, __VA_ARGS__)
#  define DE_AUDIENCE_METHODS(ClassName, ...) \
    DE_AUDIENCE_METHODS_(DE_NUM_ARG(__VA_ARGS__), ClassName, __VA_ARGS__)

#else
// Slightly modified version for MSVC because __VA_ARGS__ gets expanded differently.
#  define DE_PIMPL_AUDIENCES_X_(N, args) DE_GLUE(DE_CONCAT(DE_PIMPL_AUDIENCES_, N), args)
#  define DE_PIMPL_AUDIENCES_1(x)       DE_PIMPL_AUDIENCE(x)
#  define DE_PIMPL_AUDIENCES_2(x, ...)  DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_1(__VA_ARGS__)
#  define DE_PIMPL_AUDIENCES_3(x, ...)  DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_X_(2, (__VA_ARGS__))
#  define DE_PIMPL_AUDIENCES_4(x, ...)  DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_X_(3, (__VA_ARGS__))
#  define DE_PIMPL_AUDIENCES_5(x, ...)  DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_X_(4, (__VA_ARGS__))
#  define DE_PIMPL_AUDIENCES_6(x, ...)  DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_X_(5, (__VA_ARGS__))
#  define DE_PIMPL_AUDIENCES_7(x, ...)  DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_X_(6, (__VA_ARGS__))
#  define DE_PIMPL_AUDIENCES_8(x, ...)  DE_PIMPL_AUDIENCE(x) DE_PIMPL_AUDIENCES_X_(7, (__VA_ARGS__))

#  define DE_PIMPL_AUDIENCES_N(N)       DE_CONCAT(DE_PIMPL_AUDIENCES_, N)
#  define DE_PIMPL_AUDIENCES_(N, args)  DE_GLUE(DE_PIMPL_AUDIENCES_N(N), args)
#  define DE_PIMPL_AUDIENCES(...) \
    DE_PIMPL_AUDIENCES_(DE_NUM_ARG(__VA_ARGS__), (__VA_ARGS__))

#  define DE_AUDIENCE_METHODS_X_(N, args) DE_GLUE(DE_CONCAT(DE_AUDIENCE_METHODS_, N), args)
#  define DE_AUDIENCE_METHODS_1(ClassName, x)       DE_AUDIENCE_METHOD(ClassName, x)
#  define DE_AUDIENCE_METHODS_2(ClassName, x, ...)  DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_1(ClassName, __VA_ARGS__)
#  define DE_AUDIENCE_METHODS_3(ClassName, x, ...)  DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_X_(2, (ClassName, __VA_ARGS__))
#  define DE_AUDIENCE_METHODS_4(ClassName, x, ...)  DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_X_(3, (ClassName, __VA_ARGS__))
#  define DE_AUDIENCE_METHODS_5(ClassName, x, ...)  DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_X_(4, (ClassName, __VA_ARGS__))
#  define DE_AUDIENCE_METHODS_6(ClassName, x, ...)  DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_X_(5, (ClassName, __VA_ARGS__))
#  define DE_AUDIENCE_METHODS_7(ClassName, x, ...)  DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_X_(6, (ClassName, __VA_ARGS__))
#  define DE_AUDIENCE_METHODS_8(ClassName, x, ...)  DE_AUDIENCE_METHOD(ClassName, x) DE_AUDIENCE_METHODS_X_(7, (ClassName, __VA_ARGS__))

#  define DE_AUDIENCE_METHODS_N(N)      DE_CONCAT(DE_AUDIENCE_METHODS_, N)
#  define DE_AUDIENCE_METHODS_(N, args) DE_GLUE(DE_AUDIENCE_METHODS_N(N), args)
#  define DE_AUDIENCE_METHODS(ClassName, ...) \
    DE_AUDIENCE_METHODS_(DE_NUM_ARG(__VA_ARGS__), (ClassName, __VA_ARGS__))
#endif

/**
 * Macro that can be used in class declarations to specify which audiences the class
 * can belong to.
 *
 * @param Type      Name of the type where the audience is defined.
 * @param Audience  Audience name.
 */
#define DE_OBSERVES(Type, Audience) public Type::I##Audience##Observer

/**
 * Macro for looping through all observers. @note The @a Audience type needs to be defined
 * in the scope.
 *
 * @param SetName  Name of the observer set class.
 * @param Var      Variable used in the loop.
 * @param Name     Name of the observer set.
 */
#define DE_FOR_OBSERVERS(Var, Name) \
    Name.call(); \
    for (std::remove_reference<decltype(Name)>::type::Loop Var(Name); !Var.done(); ++Var)

/**
 * Macro for looping through the audience members.
 *
 * @param Name  Name of the audience.
 * @param Var   Variable used in the loop.
 */
#define DE_NOTIFY_VAR(Name, Var) \
    DE_FOR_OBSERVERS(Var, audienceFor##Name)

#define DE_NOTIFY(Name, Var) \
    DE_FOR_OBSERVERS(Var, audienceFor##Name())

/**
 * Macro for looping through the public audience members from inside a private
 * implementation.
 *
 * @param Name  Name of the audience.
 * @param Var   Variable used in the loop.
 */
#define DE_NOTIFY_PUBLIC_VAR(Name, Var) \
    DE_FOR_OBSERVERS(Var, self().audienceFor##Name)

#define DE_NOTIFY_PUBLIC(Name, Var) \
    DE_FOR_OBSERVERS(Var, self().audienceFor##Name())

namespace de {

class ObserverBase;

/**
 * Interface for a group of observers.
 */
class DE_PUBLIC IAudience
{
public:
    virtual ~IAudience();
    virtual void addMember   (ObserverBase *member) = 0;
    virtual void removeMember(ObserverBase *member) = 0;
};

class DE_PUBLIC ObserverBase
{
public:
    ObserverBase();
    ObserverBase(const ObserverBase &) = delete; // copying denied
    virtual ~ObserverBase();

    void addMemberOf   (IAudience &observers);
    void removeMemberOf(IAudience &observers);

private:
    LockableT<PointerSetT<IAudience>> _memberOf;
};

/**
 * Template for observer sets. The template type should be an interface
 * implemented by all the observers. The observer type must implement ObserverBase.
 * @ingroup data
 *
 * @par How to use the non-pimpl audience macros
 *
 * These examples explain how to create an audience called "Deletion". In
 * general, audience names should be nouns like this so they can be used in the
 * form "audience for (something)".
 *
 * In a class declaration, define the audience in the @c public section of the
 * class: <pre>DE_DEFINE_AUDIENCE(Deletion, ...interface-function...)</pre>
 *
 * This will generate a public member variable called @c audienceForDeletion
 * that can be directly manipulated by other objects.
 *
 * Note that because the audience is created as a public member variable, this
 * can easily lead to ABI backwards compatibility issues down the road if there
 * is need to make changes to the class.
 *
 * @par How to use the pimpl audience macros
 *
 * Another set of macros is provided for declaring and defining a
 * pimpl-friendly audience. The caveat is that you'll need to separately
 * declare accessor methods and define the audience inside the private
 * implementation of the class.
 *
 * First, define the audience in the @c public section of the class:
 * <pre>DE_AUDIENCE(Deletion, ...interface-function...)</pre>
 *
 * This works like DE_DEFINE_AUDIENCE, but without a public member variable.
 * Instead, accessor methods are declared for accessing the audience.
 *
 * Then, inside the private implementation (@c Instance struct), define the
 * audience: <pre>DE_PIMPL_AUDIENCE(Deletion)</pre>
 *
 * Finally, define the accessor methods (for instance, just before the
 * constructor of the class):
 * <pre>DE_AUDIENCE_METHOD(ClassName, Deletion)</pre>
 *
 * It is recommended to keep the DE_PIMPL_AUDIENCE and DE_AUDIENCE_METHOD
 * macros close together in the source file for easier maintenance. The former
 * could be at the end of the @c Instance struct while the latter is immediately
 * following the struct.
 *
 * @par Thread-safety
 *
 * Observers and Observers::Loop lock the observer set separately for reading
 * and writing as appropriate.
 */
template <typename Type>
class Observers : public Lockable, public IAudience
{
public:
    using Members        = PointerSetT<Type>; // note: ordered, array-based
    using const_iterator = typename Members::const_iterator;
    using size_type      = int;

    /**
     * Iteration utility for observers. This should be used when
     * notifying observers, because it is safe against the observer removing
     * itself from the observer set, or the set itself being destroyed.
     */
    class Loop : public PointerSet::IIterationObserver
    {
    public:
        Loop(const Observers &observers)
            : _audience(&observers)
            , _prevObserver(nullptr)
        {
            DE_GUARD(_audience);
            if (members().flags() & PointerSet::AllowInsertionDuringIteration)
            {
                _prevObserver = members().iterationObserver();
                members().setIterationObserver(this);
            }
            members().setBeingIterated(true);
            _next = members().begin();
            next();
        }
        virtual ~Loop()
        {
            DE_GUARD(_audience);
            members().setBeingIterated(false);
            if (members().flags() & PointerSet::AllowInsertionDuringIteration)
            {
                members().setIterationObserver(_prevObserver);
            }
        }
        bool done() const { return _current >= members().end(); }
        void next()
        {
            _current = _next;
            if (_current < members().begin())
            {
                _current = members().begin();
                if (_next < _current) _next = _current;
            }
            if (_next < members().end())
            {
                ++_next;
            }
        }
        const const_iterator &get() const { return _current; }
        Type *operator->() const { return *get(); }
        Loop &operator++()
        {
            next();
            return *this;
        }
        void pointerSetIteratorsWereInvalidated(const PointerSet::Pointer *oldBase,
                                                const PointerSet::Pointer *newBase) override
        {
            if (_prevObserver)
            {
                _prevObserver->pointerSetIteratorsWereInvalidated(oldBase, newBase);
            }
            _current = reinterpret_cast<const_iterator>(newBase) +
                       (_current - reinterpret_cast<const_iterator>(oldBase));
            _next = reinterpret_cast<const_iterator>(newBase) +
                    (_next - reinterpret_cast<const_iterator>(oldBase));
        }

    private:
        inline const Members &members() const { return _audience->_members; }

        const Observers *               _audience;
        PointerSet::IIterationObserver *_prevObserver;
        const_iterator                  _current;
        const_iterator                  _next;
    };

    friend class Loop;

public:
    Observers() {}

    Observers(Observers<Type> const &other) { *this = other; }

    virtual ~Observers()
    {
        _disassociateAllMembers();
    }

    void clear()
    {
        _disassociateAllMembers();
    }

    Observers<Type> &operator=(Observers<Type> const &other)
    {
        if (this == &other) return *this;
        DE_GUARD(other);
        DE_GUARD(this);
        _members = other._members;
        for (Type *observer : _members)
        {
            observer->addMemberOf(*this);
        }
        return *this;
    }

    /// Add an observer into the set. The set does not receive
    /// ownership of the observer instance.
    void add(Type *observer)
    {
        _add(observer);
        observer->addMemberOf(*this);
    }

    Observers<Type> &operator+=(Type *observer)
    {
        add(observer);
        return *this;
    }

    Observers<Type> &operator+=(Type &observer)
    {
        add(&observer);
        return *this;
    }

    Observers<Type> const &operator+=(const Type *observer) const
    {
        const_cast<Observers<Type> *>(this)->add(const_cast<Type *>(observer));
        return *this;
    }

    Observers<Type> const &operator+=(const Type &observer) const
    {
        const_cast<Observers<Type> *>(this)->add(const_cast<Type *>(&observer));
        return *this;
    }

    void remove(Type *observer)
    {
        _remove(observer);
        observer->removeMemberOf(*this);
    }

    Observers<Type> &operator-=(Type *observer)
    {
        remove(observer);
        return *this;
    }

    Observers<Type> &operator-=(Type &observer)
    {
        remove(&observer);
        return *this;
    }

    Observers<Type> const &operator-=(Type *observer) const
    {
        const_cast<Observers<Type> *>(this)->remove(observer);
        return *this;
    }

    Observers<Type> const &operator-=(Type &observer) const
    {
        const_cast<Observers<Type> *>(this)->remove(&observer);
        return *this;
    }

    Observers<Type> &operator+=(const std::function<void()> &callback)
    {
        DE_GUARD(this);
        _callbacks << callback;
        return *this;
    }

    void call() const
    {
        lock();
        const auto cbs = _callbacks;
        unlock();

        for (const auto &cb : cbs)
        {
            cb();
        }
    }

    size_type size() const
    {
        DE_GUARD(this);
        return _members.size();
    }

    inline bool isEmpty() const { return size() == 0; }

    bool contains(const Type *observer) const
    {
        DE_GUARD(this);
        return _members.contains(const_cast<Type *>(observer));
    }

    bool contains(const Type &observer) const
    {
        DE_GUARD(this);
        return _members.contains(const_cast<Type *>(&observer));
    }

    /**
     * Allows or denies addition of audience members while the audience is being
     * iterated. By default, addition is not allowed. If additions are allowed, only one
     * Loop can be iterating the audience at a time.
     *
     * @param yes  @c true to allow additions, @c false to deny.
     */
    void setAdditionAllowedDuringIteration(bool yes)
    {
        DE_GUARD(this);
        _members.setFlags(Members::AllowInsertionDuringIteration, yes);
    }

    // Implements IAudience.
    void addMember   (ObserverBase *member) { _add   (static_cast<Type *>(member)); }
    void removeMember(ObserverBase *member) { _remove(static_cast<Type *>(member)); }

private:
    void _disassociateAllMembers()
    {
        for (;;)
        {
            Type *observer;
            {
                DE_GUARD(this);
                if (_members.isEmpty()) break;
                observer = _members.take();
            }
            observer->removeMemberOf(*this);
        }
    }

    void _add(Type *observer)
    {
        DE_GUARD(this);
        DE_ASSERT(observer != 0);
        _members.insert(observer);
    }

    void _remove(Type *observer)
    {
        DE_GUARD(this);
        _members.remove(observer);
    }

    Members _members;
    List<std::function<void()>> _callbacks;
};

} // namespace de

#endif /* LIBCORE_OBSERVERS_H */
