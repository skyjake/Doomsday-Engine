/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2011-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

/**
 * @file libcore.h  Common definitions for Doomsday Engine Core Library
 */

/**
 * @defgroup core  Core
 *
 * @defgroup data  Data Types and Structures
 * Classes related to accessing, storing, and processing of data. @ingroup core
 *
 * @defgroup net  Network
 * Classes responsible for network communications. @ingroup core
 *
 * @defgroup math  Math Utilities
 * @ingroup core
 *
 * @defgroup types  Basic Types
 * Basic data types. @ingroup core
 */

/**
 * @mainpage Doomsday SDK
 *
 * <p>This documentation covers all the functions and data that Doomsday makes
 * available for games and other plugins.</p>
 *
 * @section Overview
 *
 * <p>The SDK is composed of a set of shared libraries. The Core library is the
 * only required one as it provides the foundation classes. GUI-dependent
 * functionality (libgui and libappfw) are kept separate so that server-only
 * builds can use the non-GUI ones.</p>
 *
 * <p>The documentation has been organized into <a href="modules.html">modules</a>.
 * The primary ones are listed below:</p>
 *
 * - @ref core
 * - @ref data
 * - @ref script
 * - @ref fs
 * - @ref net
 * - @ref appfw
 */

#ifdef __cplusplus
#  define DE_EXTERN_C extern "C"
#else
#  define DE_EXTERN_C extern
#  define nullptr NULL
#endif

#if defined(__cplusplus) && !defined(DE_C_API_ONLY)
#  include <cstddef> // size_t
#  include <cstring> // memset
#  include <functional>
#  include <memory>  // unique_ptr, shared_ptr
#  include <typeinfo>
#  include <stdexcept>
#  include <string>
#  include <limits>
#endif

#if defined(__x86_64__) || defined(__x86_64) || defined(_LP64) || defined(DE_64BIT_HOST)
#  define DE_64BIT
#endif

#ifndef _MSC_VER
#  include <stdint.h> // Not MSVC so use C99 standard header
#endif

#include <assert.h>

/*
 * When using the C API, the Qt string functions are not available, so we
 * must use the platform-specific functions.
 */
#if defined (UNIX) && defined (DE_C_API_ONLY)
#  include <strings.h> // strcasecmp etc.
#endif

/**
 * @def DE_PUBLIC
 *
 * Used for declaring exported symbols. It must be applied in all exported
 * classes and functions. DEF files are not used for exporting symbols out
 * of libcore.
 */
#if defined (_WIN32) && defined (_MSC_VER)
#  ifdef __LIBCORE__
// This is defined when compiling the library.
#    define DE_PUBLIC   __declspec(dllexport)
#  else
#    define DE_PUBLIC   __declspec(dllimport)
#  endif
#  define DE_HIDDEN
#  define DE_NORETURN   __declspec(noreturn)
#elif defined (__CYGWIN__)
#  define DE_PUBLIC     __attribute__((visibility("default")))
#  define DE_HIDDEN     
#  define DE_NORETURN   __attribute__((__noreturn__))
#elif defined (MACOSX) 
#  define DE_PUBLIC     __attribute__((visibility("default")))
#  define DE_HIDDEN     __attribute__((visibility("hidden")))
#  define DE_NORETURN   __attribute__((__noreturn__))
#else
#  define DE_PUBLIC     __attribute__((visibility("default")))
#  define DE_HIDDEN 
#  define DE_NORETURN   __attribute__((__noreturn__))
#endif

#if defined (DE_ASSERT)
#  undef DE_ASSERT
#endif
#if defined (DE_ASSERT_FAIL)
#  undef DE_ASSERT_FAIL
#endif
#if defined (DE_UNUSED)
#  undef DE_UNUSED
#endif

#if defined (__clang__)
#  define DE_FALLTHROUGH    [[clang::fallthrough]]
#else
#  define DE_FALLTHROUGH
#endif

#ifndef NDEBUG
#  define DE_DEBUG
   DE_EXTERN_C DE_PUBLIC void LogBuffer_Flush(void);
#  define DE_ASSERT(x) {if (!(x)) {LogBuffer_Flush(); assert(x);}}
#  define DE_DEBUG_ONLY(x) x
#else
#  define DE_NO_DEBUG
#  define DE_ASSERT(x)
#  define DE_DEBUG_ONLY(x)
#endif

#define DE_ASSERT_FAIL(msgCStr)  DE_ASSERT(msgCStr == nullptr)

#if defined (UNIX) && !defined (__CYGWIN__)
#  include <execinfo.h>
/**
 * @macro DE_PRINT_BACKTRACE
 * Debug utility for dumping the current backtrace using qDebug.
 */
#  define DE_PRINT_BACKTRACE() { \
        void *callstack[128]; \
        int i, frames = backtrace(callstack, 128); \
        char** strs = backtrace_symbols(callstack, frames); \
        for (i = 0; i < frames; ++i) fprintf(stderr, "%s", strs[i]); \
        free(strs); }
#  define DE_BACKTRACE(n, outStr) { \
        void *callstack[n]; \
        int i, frames = backtrace(callstack, n); \
        char** strs = backtrace_symbols(callstack, frames); \
        out = ""; \
        for (i = 0; i < frames; ++i) { outStr.append(strs[i]); outStr.append('\n'); } \
        free(strs); }
#else
#  define DE_PRINT_BACKTRACE()
#endif

/**
 * Macro for determining the name of a type (using RTTI).
 */
#define DE_TYPE_NAME(t)  (typeid(t).name())

#define DE_CONCAT(x, y)         x##y
#define DE_GLUE(x, y)           x y

/**
 * @macro DE_UNUSED(...)
 * Macro for marking parameters and variables as intentionally unused, so the compiler
 * will not complain about them.
 */
#define DE_UNUSED_MANY(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, ...) \
    ((void)(_0), (void)(_1), (void)(_2), (void)(_3), (void)(_4), \
     (void)(_5), (void)(_6), (void)(_7), (void)(_8), (void)(_9))
#define DE_NUM_ARG_RSEQ_N()     8, 7, 6, 5, 4, 3, 2, 1, 0
#define DE_NUM_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N

#if !defined (_MSC_VER)
#  define DE_UNUSED(...)        DE_UNUSED_MANY(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
// Macro trick for determining number of arguments (up to 8).
#  define DE_NUM_ARG(...)       DE_NUM_ARG_(__VA_ARGS__, DE_NUM_ARG_RSEQ_N())
#  define DE_NUM_ARG_(...)      DE_NUM_ARG_N(__VA_ARGS__)
#else // _MSC_VER
#  define DE_UNUSED_MANY_(args) DE_UNUSED_MANY args
#  define DE_UNUSED(...)        DE_UNUSED_MANY_((__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
#  define DE_NUM_ARG_(args)     DE_NUM_ARG_N args
#  define DE_NUM_ARG(...)       DE_NUM_ARG_((__VA_ARGS__, DE_NUM_ARG_RSEQ_N()))
#endif

#define DE_PLURAL_S(Count) ((Count) != 1? "s" : "")

#define DE_BOOL_YESNO(Yes) ((Yes)? "yes" : "no")

/**
 * Forms an escape sequence string literal. Escape sequences begin
 * with an ASCII Escape character.
 */
#define DE_ESC(StringLiteral)       "\x1b" StringLiteral
#define DE_ESCL(WideStringLiteral)  L"\x1b" WideStringLiteral
#define _E(Code) DE_ESC(#Code)

#define DENG2_OFFSET_PTR(type, member) \
    reinterpret_cast<const void *>(offsetof(type, member))

/**
 * Returns a String literal that uses statically allocated memory
 * for the string.
 */
#define DE_STR(cStringLiteral) \
    ([]() -> de::String { \
        const size_t len = std::strlen(cStringLiteral); /* likely will be constexpr */ \
        static iBlockData blockData = { \
           2, const_cast<char *>(cStringLiteral), len, len + 1}; \
        static iBlock block = { &blockData }; \
        return de::String(&block); \
    }())

/**
 * Macro for defining an opaque type in the C wrapper API.
 */
#define DE_OPAQUE(Name) \
    struct Name ## _s; \
    typedef struct Name ## _s Name;

/**
 * Macro for converting an opaque wrapper type to a de::type.
 * Asserts that the object really exists (not null).
 */
#define DE_SELF(Type, Var) \
    DE_ASSERT(Var != nullptr); \
    de::Type *self = reinterpret_cast<de::Type *>(Var);

/**
 * Macro for iterating through an STL container. Note that @a ContainerRef should be a
 * variable and not a temporary value returned from some function call -- otherwise the
 * begin() and end() calls may not refer to the same container's beginning and end.
 *
 * @param IterClass     Class being iterated.
 * @param Iter          Name/declaration of the iterator variable.
 * @param ContainerRef  Container.
 */
#define DE_FOR_EACH(IterClass, Iter, ContainerRef) \
    for (IterClass::iterator Iter = (ContainerRef).begin(); Iter != (ContainerRef).end(); ++Iter)

/// @copydoc DE_FOR_EACH
#define DE_FOR_EACH_CONST(IterClass, Iter, ContainerRef) \
    for (IterClass::const_iterator Iter = (ContainerRef).begin(); Iter != (ContainerRef).end(); ++Iter)

/**
 * Macro for iterating through an STL container in reverse. Note that @a ContainerRef
 * should be a variable and not a temporary value returned from some function call --
 * otherwise the begin() and end() calls may not refer to the same container's beginning
 * and end.
 *
 * @param IterClass     Class being iterated.
 * @param Var           Name/declaration of the iterator variable.
 * @param ContainerRef  Container.
 */
#define DE_FOR_EACH_REVERSE(IterClass, Var, ContainerRef) \
    for (IterClass::reverse_iterator Var = (ContainerRef).rbegin(); Var != (ContainerRef).rend(); ++Var)

/// @copydoc DE_FOR_EACH_REVERSE
#define DE_FOR_EACH_CONST_REVERSE(IterClass, Var, ContainerRef) \
    for (IterClass::const_reverse_iterator Var = (ContainerRef).rbegin(); Var != (ContainerRef).rend(); ++Var)

#define DE_NO_ASSIGN(ClassName) \
    ClassName &operator = (const ClassName &) = delete;

#define DE_NO_COPY(ClassName) \
    ClassName(const ClassName &) = delete;

/**
 * Macro for declaring methods for convenient casting:
 *
 * - `is` checks if the object can be casted.
 * - `as` performs the cast as efficiently as possible (just a `static_cast`).
 *   Use this when the cast is always known to be legal.
 * - `maybeAs` does a `dynamic_cast` (which has slower performance). Failure
 *   to cast simply results in nullptr.
 * - `expectedAs` does a `dynamic_cast` and throws an exception if the cast fails.
 *   Slowest performance, but is the safest.
 */
#define DE_CAST_METHODS() \
    template <typename T_> \
    T_ &as() { \
        DE_ASSERT(de::is<T_>(this)); \
        return *static_cast<T_ *>(this); \
    } \
    template <typename T_> \
    T_ *asPtr() { \
        DE_ASSERT(de::is<T_>(this)); \
        return static_cast<T_ *>(this); \
    } \
    template <typename T_> \
    const T_ &as() const { \
        DE_ASSERT(de::is<T_>(this)); \
        return *static_cast<const T_ *>(this); \
    } \
    template <typename T_> \
    const T_ *asPtr() const { \
        DE_ASSERT(de::is<T_>(this)); \
        return static_cast<const T_ *>(this); \
    }

/**
 * Macro for starting the definition of a private implementation struct. The
 * struct holds a reference to the public instance, which must be specified in
 * the call to the base class constructor. @see de::Private
 *
 * Example:
 * <pre>
 *    DE_PIMPL(MyClass)
 *    {
 *        Impl(Public &inst) : Base(inst) {
 *            // constructor
 *        }
 *        // private data and methods
 *    };
 * </pre>
 */
#define DE_PIMPL(ClassName) \
    struct DE_HIDDEN ClassName::Impl : public de::Private<ClassName>

/**
 * Macro for starting the definition of a private implementation struct without
 * a reference to the public instance. This is useful for simpler classes where
 * the private implementation mostly holds member variables.
 */
#define DE_PIMPL_NOREF(ClassName) \
    struct DE_HIDDEN ClassName::Impl : public de::IPrivate

/**
 * Macro for publicly declaring a pointer to the private implementation.
 * de::PrivateAutoPtr owns the private instance and will automatically delete
 * it when the PrivateAutoPtr is destroyed.
 */
#define DE_PRIVATE(Var) \
    struct DE_HIDDEN Impl; \
    DE_HIDDEN de::PrivateAutoPtr<Impl> Var;

#if defined(__cplusplus) && !defined(DE_C_API_ONLY)
namespace de {

DE_PUBLIC void debug    (const char *, ...);
DE_PUBLIC void warning  (const char *, ...);

/**
 * Formats a string using the standard C printf() syntax.
 *
 * @param format  Format string.
 *
 * @return Formatted string.
 */
DE_PUBLIC std::string stringf(const char *format, ...);

/**
 * @defgroup errors Exceptions
 *
 * These are exceptions thrown by libcore when a fatal error occurs.
 */

/**
 * Base class for error exceptions thrown by libcore. @ingroup errors
 */
class DE_PUBLIC Error : public std::runtime_error
{
public:
    Error(const std::string &where, const std::string &message);
    Error(const Error &) = default;
    virtual ~Error() = default;

    std::string name() const;
    virtual std::string asText() const;

    /**
     * Prints the error as plain text using de::warning. EscapeParser is used
     * to remove any escape sequences.
     */
    void warnPlainText() const;

    std::string asPlainText() const;

protected:
    void setName(const std::string &name);

private:
    std::string _name;
};

/**
 * Macro for defining an exception class that belongs to a parent group of
 * exceptions.  This should be used so that whoever uses the class
 * that throws an exception is able to choose the level of generality
 * of the caught errors.
 */
#define DE_SUB_ERROR(Parent, Name) \
    class Name : public Parent { \
    public: \
        Name(const std::string &message) \
            : Parent("-", message) { Parent::setName(#Name); } \
        Name(const std::string &where, const std::string &message) \
            : Parent(where, message) { Parent::setName(#Name); } \
        virtual void raise() const { throw *this; } \
    } /**< @note One must put a semicolon after the macro invocation. */

/**
 * Define a top-level exception class.
 * @note One must put a semicolon after the macro invocation.
 */
#define DE_ERROR(Name) DE_SUB_ERROR(de::Error, Name)

/// Thrown from the expectedAs() method if a cast cannot be made as expected.
DE_ERROR(CastError);

template <typename Iterator>
const typename Iterator::key_type &ckey(const Iterator &i) {
    return i->first;
}
template <typename Iterator>
const typename Iterator::value_type cvalue(const Iterator &i) {
    return i->second;
}

/*
 * Convenience wrappers for dynamic_cast.
 */

template <typename X_, typename T_>
inline bool is(T_ *ptr) {
    return dynamic_cast<X_ *>(ptr) != nullptr;
}

template <typename X_, typename T_>
inline bool is(const T_ *ptr) {
    return dynamic_cast<const X_ *>(ptr) != nullptr;
}

template <typename X_, typename T_>
inline bool is(T_ &obj) {
    return dynamic_cast<X_ *>(&obj) != nullptr;
}

template <typename X_, typename T_>
inline bool is(const T_ &obj) {
    return dynamic_cast<const X_ *>(&obj) != nullptr;
}

template <typename X_, typename T_>
inline X_ *maybeAs(T_ *ptr) {
    return dynamic_cast<X_ *>(ptr);
}

template <typename X_, typename T_>
inline const X_ *maybeAs(const T_ *ptr) {
    return dynamic_cast<const X_ *>(ptr);
}

template <typename X_, typename T_>
inline X_ *maybeAs(T_ &obj) {
    return dynamic_cast<X_ *>(&obj);
}

template <typename X_, typename T_>
inline const X_ *maybeAs(const T_ &obj) {
    return dynamic_cast<const X_ *>(&obj);
}

template <typename X_, typename T_>
inline X_ &expectedAs(T_ *ptr) {
    if (auto *t = maybeAs<X_>(ptr)) return *t;
    DE_ASSERT(false);
    throw CastError(stringf("Cannot cast %s to %s", DE_TYPE_NAME(T_), DE_TYPE_NAME(X_)));
}

template <typename X_, typename T_>
inline const X_ &expectedAs(const T_ *ptr) {
    if (const auto *t = maybeAs<X_>(ptr)) return *t;
    DE_ASSERT(false);
    throw CastError(stringf("Cannot cast %s to %s", DE_TYPE_NAME(T_), DE_TYPE_NAME(X_)));
}

/**
 * Interface for all private instance implementation structs.
 * In a debug build, also contains a verification code that can be used
 * to assert whether the pointed object really is derived from IPrivate.
 */
struct IPrivate {
    virtual ~IPrivate() = default;
#ifdef DE_DEBUG
#  define DE_IPRIVATE_VERIFICATION 0xdeadbeef
    unsigned int _privateImplVerification = DE_IPRIVATE_VERIFICATION;
    unsigned int privateImplVerification() const { return _privateImplVerification; }
#endif
};

/**
 * Pointer to the private implementation. Behaves like std::unique_ptr, but with
 * the additional requirement that the pointed/owned instance must be derived
 * from de::IPrivate.
 */
template <typename ImplType>
class PrivateAutoPtr
{
    DE_NO_COPY  (PrivateAutoPtr)
    DE_NO_ASSIGN(PrivateAutoPtr)

public:
    PrivateAutoPtr(ImplType *p) : ptr(p) {}
    PrivateAutoPtr(PrivateAutoPtr &&moved) : ptr(moved.ptr) {
        moved.ptr = nullptr;
    }
    ~PrivateAutoPtr() { reset(); }

    PrivateAutoPtr &operator = (PrivateAutoPtr &&moved) {
        reset();
        ptr = moved.ptr;
        moved.ptr = nullptr;
        return *this;
    }
    inline ImplType &operator * () const { return *ptr; }
    inline ImplType *operator -> () const { return ptr; }
    void reset(ImplType *p = 0) {
        IPrivate *ip = reinterpret_cast<IPrivate *>(ptr);
        if (ip)
        {
            DE_ASSERT(ip->privateImplVerification() == DE_IPRIVATE_VERIFICATION);
            delete ip;
        }
        ptr = p;
    }
    inline ImplType *get() const {
        return ptr;
    }
    inline const ImplType *getConst() const {
        return ptr;
    }
    inline operator ImplType *() const {
        return ptr;
    }
    ImplType *release() {
        ImplType *p = ptr;
        ptr = 0;
        return p;
    }
    void swap(PrivateAutoPtr &other) {
        std::swap(ptr, other.ptr);
    }
    inline bool isNull() const {
        return !ptr;
    }
#ifdef DE_DEBUG
    bool isValid() const {
        return ptr && reinterpret_cast<IPrivate *>(ptr)->privateImplVerification() == DE_IPRIVATE_VERIFICATION;
    }
#endif

private:
    ImplType *ptr;
};

/**
 * Utility template for defining private implementation data (pimpl idiom). Use
 * this in source files, not in headers.
 */
template <typename PublicType>
struct Private : public IPrivate {
    using Public = PublicType;
    typedef Private<PublicType> Base;

    Public *thisPublic;
    inline Public &self() const { return *thisPublic; }

    Private(PublicType &i) : thisPublic(&i) {}
    Private(PublicType *i) : thisPublic(i) {}
};

template <typename ToType, typename FromType>
inline ToType function_cast(FromType ptr)
{
    /**
     * @note Casting to a pointer-to-function type: see
     * http://www.trilithium.com/johan/2004/12/problem-with-dlsym/
     */
    // This is not 100% portable to all possible memory architectures; thus:
    DE_ASSERT(sizeof(void *) == sizeof(ToType));

    union { FromType original; ToType target; } forcedCast;
    forcedCast.original = ptr;
    return forcedCast.target;
}

template <typename ToType, typename FromType>
inline ToType functionAssign(ToType &dest, FromType src)
{
    return dest = de::function_cast<ToType>(src);
}

/**
 * Clears a region of memory. Size of the region is the size of Type.
 * @param t  Reference to the memory.
 */
template <typename Type>
inline void zap(Type &t) {
    std::memset(&t, 0, sizeof(Type));
}

/**
 * Clears a region of memory. Size of the region is the size of Type.
 * @param t  Pointer to the start of the region of memory.
 *
 * @note An overloaded zap(Type *) would not work as the size of array
 * types could not be correctly determined at compile time; thus this
 * function is not an overload.
 */
template <typename Type>
inline void zapPtr(Type *t) {
    std::memset(t, 0, sizeof(Type));
}

template <typename Container>
inline void deleteAll(Container &c) {
    for (auto *i : c) {
        delete i;
    }
}

template <typename ContainerType>
inline ContainerType mapInPlace(ContainerType &c,
                                const std::function<typename ContainerType::value_type (
                                    const typename ContainerType::value_type &)> &func) {
    for (auto i = c.begin(); i != c.end(); ++i) {
        *i = func(*i);
    }
}

template <typename ContainerType>
inline ContainerType map(const ContainerType &c,
                         const std::function<typename ContainerType::value_type (
                             const typename ContainerType::value_type &)> &func) {
    ContainerType out;
    for (auto i = c.begin(); i != c.end(); ++i) {
        out.push_back(func(*i));
    }
    return out;
}

template <typename OutContainer,
          typename InContainer,
          typename Func,
          typename Inserter = std::back_insert_iterator<OutContainer>>
inline OutContainer map(const InContainer &input, Func func) {
    OutContainer out;
    Inserter ins(out);
    for (const auto &i : input) {
        *ins++ = func(i);
    }
    return out;
}

template <typename ContainerType>
inline ContainerType filter(const ContainerType &c,
                            const std::function<bool (const typename ContainerType::value_type &)> &func) {
    ContainerType out;
    for (auto i = c.begin(); i != c.end(); ++i) {
        if (func(*i)) out.push_back(*i);
    }
    return out;
}

template <typename Container, typename Iterator>
inline Container compose(Iterator start, Iterator end) {
    Container c;
    for (Iterator i = start; i != end; ++i) {
        c.push_back(*i);
    }
    return c;
}

} // namespace de
#endif // __cplusplus

#if defined(__cplusplus) && !defined(DE_C_API_ONLY)
namespace de {

/**
 * Operation performed on a flag set (e.g., QFlags).
 */
enum FlagOp {
    UnsetFlags   = 0,   ///< Specified flags are unset, leaving others unmodified.
    SetFlags     = 1,   ///< Specified flags are set, leaving others unmodified.
    ReplaceFlags = 2    ///< Specified flags become the new set of flags, replacing all previous flags.
};

struct FlagOpArg {
    FlagOp op;
    inline FlagOpArg(FlagOp op) : op(op) {}
    inline FlagOpArg(bool set)  : op(set? SetFlags : UnsetFlags) {}
    inline operator FlagOp () const { return op; }
};

template <typename FlagsType, typename FlagsCompatibleType>
void applyFlagOperation(FlagsType &flags, const FlagsCompatibleType &newFlags, FlagOpArg operation) {
    switch (operation.op) {
    case SetFlags:     flags |=  newFlags; break;
    case UnsetFlags:   flags &= ~newFlags; break;
    case ReplaceFlags: flags  =  newFlags; break;
    }
}

/**
 * Clock-wise direction identifiers.
 */
enum ClockDirection {
    CounterClockwise = 0,
    Clockwise        = 1
};

/**
 * Status to return from abortable iteration loops that use callbacks per iteration.
 *
 * The "for" pattern:
 * <code>
 * LoopResult forExampleObjects(std::function<LoopResult (ExampleObject &)> func);
 *
 * example.forExampleObjects([] (ExampleObject &ex) {
 *     // do stuff...
 *     return LoopContinue;
 * });
 * </code>
 */
enum GenericLoopResult {
    LoopContinue = 0,
    LoopAbort    = 1
};

/// Use as return type of iteration loop callbacks (a "for*" method).
struct LoopResult
{
    int value;

    LoopResult(int val = LoopContinue) : value(val) {}
    operator bool () const { return value != LoopContinue; }
    operator int () const { return value; }
    operator GenericLoopResult () const { return GenericLoopResult(value); }
};

/**
 * All serialization in all contexts use a common protocol version number.
 * Whenever anything changes in serialization, the protocol version needs to be
 * incremented. Therefore, deserialization routines shouldn't check for
 * specific versions, but instead always use < and >.
 *
 * Do not reserve version numbers in advance; any build may need to increment
 * the version, necessitating changing the subsequent numbers.
 */
enum ProtocolVersion {
    DE_PROTOCOL_1_9_10 = 0,

    DE_PROTOCOL_1_10_0 = 0,

    DE_PROTOCOL_1_11_0_Time_high_performance = 1,
    DE_PROTOCOL_1_11_0 = 1,

    DE_PROTOCOL_1_12_0 = 1,

    DE_PROTOCOL_1_13_0 = 1,

    DE_PROTOCOL_1_14_0_LogEntry_metadata = 2,
    DE_PROTOCOL_1_14_0 = 2,

    DE_PROTOCOL_1_15_0_NameExpression_with_scope_identifier = 3,
    DE_PROTOCOL_1_15_0 = 3,

    DE_PROTOCOL_2_0_0  = 3,
    DE_PROTOCOL_2_1_0  = 3,

    DE_PROTOCOL_2_2_0_NameExpression_identifier_sequence = 4,
    DE_PROTOCOL_2_2_0  = 4,

    DE_PROTOCOL_LATEST = DE_PROTOCOL_2_2_0
};

//@{
/// @ingroup types
typedef int8_t   dchar;      ///< 8-bit signed integer.
typedef uint8_t  dbyte;      ///< 8-bit unsigned integer.
typedef uint8_t  duchar;     ///< 8-bit unsigned integer.
typedef dchar    dint8;      ///< 8-bit signed integer.
typedef dbyte    duint8;     ///< 8-bit unsigned integer.
typedef int16_t  dint16;     ///< 16-bit signed integer.
typedef uint16_t duint16;    ///< 16-bit unsigned integer.
typedef dint16   dshort;     ///< 16-bit signed integer.
typedef duint16  dushort;    ///< 16-bit unsigned integer.
typedef int32_t  dint32;     ///< 32-bit signed integer.
typedef uint32_t duint32;    ///< 32-bit unsigned integer.
typedef dint32   dint;       ///< 32-bit signed integer.
typedef duint32  duint;      ///< 32-bit unsigned integer.
typedef int64_t  dint64;     ///< 64-bit signed integer.
typedef uint64_t duint64;    ///< 64-bit unsigned integer.
typedef float    dfloat;     ///< 32-bit floating point number.
typedef double   ddouble;    ///< 64-bit floating point number.
typedef uint64_t dsize;
typedef int64_t  dsigsize;

typedef unsigned int uint; // for convenience

class DE_PUBLIC Char
{
public:
    inline constexpr Char() : _ch(0) {}
    inline constexpr Char(char ch) : _ch(uint32_t(ch)) {}
    inline constexpr Char(uint32_t uc32) : _ch(uc32) {}
    inline Char(const Char &other) = default;
    inline Char(Char &&moved)      = default;

    inline explicit operator bool() const { return _ch != 0; }
    inline operator uint32_t() const { return _ch; }
    inline uint32_t unicode() const { return _ch; }
    inline int delta(Char from) const { return int(_ch) - int(from._ch); }

    Char &operator=(const Char &) = default;
    Char &operator=(Char &&) = default;

    Char upper() const;
    Char lower() const;

    bool isSpace() const;
    bool isAlpha() const;
    bool isNumeric() const;
    bool isAlphaNumeric() const;

private:
    uint32_t _ch;
};

class DE_PUBLIC Flags
{
public:
    inline Flags(uint32_t flags = 0) : _flg(flags) {}
    inline Flags(const Flags &other) = default;
    inline Flags(Flags &&moved)      = default;

    inline operator uint32_t() const { return _flg; }

    inline bool testFlag(uint32_t f) const { return (_flg & f) == f; }

    inline Flags &operator=(uint32_t flags) { _flg = flags; return *this; }
    Flags &operator=(const Flags &) = default;
    Flags &operator=(Flags &&) = default;
    inline uint32_t &operator|=(uint32_t flags) { return _flg |= flags; }
    inline uint32_t &operator&=(uint32_t flags) { return _flg &= flags; }
    inline uint32_t &operator^=(uint32_t flags) { return _flg ^= flags; }

private:
    uint32_t _flg;
};

// Pointer-integer conversion (used for legacy code).
#ifdef DE_64BIT
typedef duint64 dintptr;
#else
typedef duint32 dintptr;
#endif
//@}

} // namespace de

#else // !__cplusplus

/*
 * Data types for C APIs.
 */
#if _MSC_VER < 1900
typedef short           dint16;
typedef unsigned short  duint16;
typedef int             dint32;
typedef unsigned int    duint32;
typedef long long       dint64;
typedef unsigned long long duint64;
#else
#include <stdint.h>
typedef int16_t         dint16;
typedef uint16_t        duint16;
typedef int32_t         dint32;
typedef uint32_t        duint32;
typedef int64_t         dint64;
typedef uint64_t        duint64;
#endif
typedef unsigned char   dbyte;
typedef unsigned int    duint;  // 32-bit
typedef float           dfloat;
typedef double          ddouble;
typedef uint64_t        dsize;
typedef int64_t         dsigsize;

#endif // !__cplusplus
