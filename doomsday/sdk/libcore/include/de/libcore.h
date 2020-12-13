/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_H
#define LIBCORE_H

/**
 * @file libcore.h  Common definitions for Doomsday 2.
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
 * @mainpage Doomsday 2 SDK
 *
 * <p>This documentation covers all the functions and data that Doomsday 2 makes
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
#  define DENG2_EXTERN_C extern "C"
#else
#  define DENG2_EXTERN_C extern
#endif

#if defined(__cplusplus) && !defined(DENG2_C_API_ONLY)
#  define DENG2_USE_QT
#  include <cstddef> // size_t
#  include <cstring> // memset
#  include <functional>
#  include <memory>  // unique_ptr, shared_ptr
#  include <typeinfo>
#  include <stdexcept>
#  include <string>
#endif

#if defined(__x86_64__) || defined(__x86_64) || defined(_LP64) || defined(DENG_64BIT_HOST)
#  define DENG2_64BIT
#endif

#ifdef DENG2_USE_QT
#  include <QtCore/qglobal.h>
#  include <QScopedPointer>
#  include <QDebug>
#  include <QString>

// Qt versioning helper. Qt 4.8 is the oldest we support.
#  if (QT_VERSION < QT_VERSION_CHECK(4, 8, 0))
#    error "Unsupported version of Qt"
#  endif
#  define DENG2_QT_4_6_OR_NEWER
#  define DENG2_QT_4_7_OR_NEWER
#  define DENG2_QT_4_8_OR_NEWER
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#    define DENG2_QT_5_0_OR_NEWER
#  endif
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 1, 0))
#    define DENG2_QT_5_1_OR_NEWER
#  endif
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
#    define DENG2_QT_5_2_OR_NEWER
#  endif
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 3, 0))
#    define DENG2_QT_5_3_OR_NEWER
#  endif
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
#    define DENG2_QT_5_4_OR_NEWER
#  endif
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
#    define DENG2_QT_5_5_OR_NEWER
#  endif
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
#    define DENG2_QT_5_6_OR_NEWER
#  endif
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 7, 0))
#    define DENG2_QT_5_7_OR_NEWER
#  endif
#endif

#ifndef _MSC_VER
#  include <stdint.h> // Not MSVC so use C99 standard header
#endif

#include <assert.h>

/*
 * When using the C API, the Qt string functions are not available, so we
 * must use the platform-specific functions.
 */
#if defined(UNIX) && defined(DENG2_C_API_ONLY)
#  include <strings.h> // strcasecmp etc.
#endif

/**
 * @def DENG2_PUBLIC
 *
 * Used for declaring exported symbols. It must be applied in all exported
 * classes and functions. DEF files are not used for exporting symbols out
 * of libcore.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#  ifdef __DENG2__
// This is defined when compiling the library.
#    define DENG2_PUBLIC __declspec(dllexport)
#  else
#    define DENG2_PUBLIC __declspec(dllimport)
#  endif
#  define DENG2_NORETURN __declspec(noreturn)
#else
// No need to use any special declarators.
#  define DENG2_PUBLIC
#  define DENG2_NORETURN __attribute__((__noreturn__))
#endif

#if defined (DENG_IOS)
#  define DENG2_VISIBLE_SYMBOL __attribute__((visibility("default")))
#else
#  define DENG2_VISIBLE_SYMBOL
#endif

#ifndef NDEBUG
#  define DENG2_DEBUG
   DENG2_EXTERN_C DENG2_PUBLIC void LogBuffer_Flush(void);
#  ifdef DENG2_USE_QT
#    define DENG2_ASSERT(x) {if (!(x)) {LogBuffer_Flush(); Q_ASSERT(x);}}
#  else
#    define DENG2_ASSERT(x) {if (!(x)) {LogBuffer_Flush(); assert(x);}}
#  endif
#  define DENG2_DEBUG_ONLY(x) x
#else
#  define DENG2_NO_DEBUG
#  define DENG2_ASSERT(x)
#  define DENG2_DEBUG_ONLY(x)
#endif

#define DE_ASSERT(x) DENG2_ASSERT(x) // work/omega

#ifdef DENG2_USE_QT
#  ifdef UNIX
#    include <execinfo.h>
/**
 * @macro DENG2_PRINT_BACKTRACE
 * Debug utility for dumping the current backtrace using qDebug.
 */
#    define DENG2_PRINT_BACKTRACE() { \
        void *callstack[128]; \
        int i, frames = backtrace(callstack, 128); \
        char** strs = backtrace_symbols(callstack, frames); \
        for (i = 0; i < frames; ++i) qDebug("%s", strs[i]); \
        free(strs); }
#    define DENG2_BACKTRACE(n, out) { \
        void *callstack[n]; \
        int i, frames = backtrace(callstack, n); \
        char** strs = backtrace_symbols(callstack, frames); \
        out = ""; \
        for (i = 0; i < frames; ++i) { out.append(strs[i]); out.append('\n'); } \
        free(strs); }
#  else
#    define DENG2_PRINT_BACKTRACE() 
#  endif
#endif

/**
 * Macro for determining the name of a type (using RTTI).
 */
#define DENG2_TYPE_NAME(t)  (typeid(t).name())

/**
 * Macro for hiding the warning about an unused parameter.
 */
#define DENG2_UNUSED(a)         (void)a

/**
 * Macro for hiding the warning about two unused parameters.
 */
#define DENG2_UNUSED2(a, b)     (void)a, (void)b

/**
 * Macro for hiding the warning about three unused parameters.
 */
#define DENG2_UNUSED3(a, b, c)  (void)a, (void)b, (void)c

/**
 * Macro for hiding the warning about four unused parameters.
 */
#define DENG2_UNUSED4(a, b, c, d) (void)a, (void)b, (void)c, (void)d

#define DENG2_PLURAL_S(Count) ((Count) != 1? "s" : "")

#define DENG2_BOOL_YESNO(Yes) ((Yes)? "yes" : "no" )

/**
 * Forms an escape sequence string literal. Escape sequences begin
 * with an ASCII Escape character.
 */
#define DENG2_ESC(StringLiteral) "\x1b" StringLiteral
#define _E(Code) DENG2_ESC(#Code)

#define DENG2_OFFSET_PTR(type, member) \
    reinterpret_cast<const void *>(offsetof(type, member))

/**
 * Macro for defining an opaque type in the C wrapper API.
 */
#define DENG2_OPAQUE(Name) \
    struct Name ## _s; \
    typedef struct Name ## _s Name;

/**
 * Macro for converting an opaque wrapper type to a de::type.
 * Asserts that the object really exists (not null).
 */
#define DENG2_SELF(Type, Var) \
    DENG2_ASSERT(Var != 0); \
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
#define DENG2_FOR_EACH(IterClass, Iter, ContainerRef) \
    for (IterClass::iterator Iter = (ContainerRef).begin(); Iter != (ContainerRef).end(); ++Iter)

/// @copydoc DENG2_FOR_EACH
#define DENG2_FOR_EACH_CONST(IterClass, Iter, ContainerRef) \
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
#define DENG2_FOR_EACH_REVERSE(IterClass, Var, ContainerRef) \
    for (IterClass::reverse_iterator Var = (ContainerRef).rbegin(); Var != (ContainerRef).rend(); ++Var)

/// @copydoc DENG2_FOR_EACH_REVERSE
#define DENG2_FOR_EACH_CONST_REVERSE(IterClass, Var, ContainerRef) \
    for (IterClass::const_reverse_iterator Var = (ContainerRef).rbegin(); Var != (ContainerRef).rend(); ++Var)

#define DENG2_NO_ASSIGN(ClassName) \
    ClassName &operator = (ClassName const &) = delete;

#define DENG2_NO_COPY(ClassName) \
    ClassName(ClassName const &) = delete;

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
#define DENG2_CAST_METHODS() \
    template <typename T_> \
    T_ &as() { \
        DENG2_ASSERT(de::is<T_>(this)); \
        return *static_cast<T_ *>(this); \
    } \
    template <typename T_> \
    T_ *asPtr() { \
        DENG2_ASSERT(de::is<T_>(this)); \
        return static_cast<T_ *>(this); \
    } \
    template <typename T_> \
    T_ const &as() const { \
        DENG2_ASSERT(de::is<T_>(this)); \
        return *static_cast<T_ const *>(this); \
    } \
    template <typename T_> \
    T_ const *asPtr() const { \
        DENG2_ASSERT(de::is<T_>(this)); \
        return static_cast<T_ const *>(this); \
    }

/**
 * Macro for starting the definition of a private implementation struct. The
 * struct holds a reference to the public instance, which must be specified in
 * the call to the base class constructor. @see de::Private
 *
 * Example:
 * <pre>
 *    DENG2_PIMPL(MyClass)
 *    {
 *        Impl(Public &inst) : Base(inst) {
 *            // constructor
 *        }
 *        // private data and methods
 *    };
 * </pre>
 */
#define DENG2_PIMPL(ClassName) \
    struct ClassName::Impl : public de::Private<ClassName>

/**
 * Macro for starting the definition of a private implementation struct without
 * a reference to the public instance. This is useful for simpler classes where
 * the private implementation mostly holds member variables.
 */
#define DENG2_PIMPL_NOREF(ClassName) \
    struct ClassName::Impl : public de::IPrivate

/**
 * Macro for publicly declaring a pointer to the private implementation.
 * de::PrivateAutoPtr owns the private instance and will automatically delete
 * it when the PrivateAutoPtr is destroyed.
 */
#define DENG2_PRIVATE(Var) \
    struct Impl; \
    de::PrivateAutoPtr<Impl> Var;

#if defined(__cplusplus) && !defined(DENG2_C_API_ONLY)
namespace de {

/**
 * @defgroup errors Exceptions
 *
 * These are exceptions thrown by libcore when a fatal error occurs.
 */

/**
 * Base class for error exceptions thrown by libcore. @ingroup errors
 */
class DENG2_PUBLIC Error : public std::runtime_error
{
public:
    Error(QString const &where, QString const &message);

    QString name() const;
    virtual QString asText() const;

protected:
    void setName(QString const &name);

private:
    std::string _name;
};

/**
 * Macro for defining an exception class that belongs to a parent group of
 * exceptions.  This should be used so that whoever uses the class
 * that throws an exception is able to choose the level of generality
 * of the caught errors.
 */
#define DENG2_SUB_ERROR(Parent, Name) \
    class Name : public Parent { \
    public: \
        Name(QString const &message) \
            : Parent("-", message) { Parent::setName(#Name); } \
        Name(QString const &where, QString const &message) \
            : Parent(where, message) { Parent::setName(#Name); } \
        virtual void raise() const { throw *this; } \
    } /**< @note One must put a semicolon after the macro invocation. */

/**
 * Define a top-level exception class.
 * @note One must put a semicolon after the macro invocation.
 */
#define DENG2_ERROR(Name) DENG2_SUB_ERROR(de::Error, Name)

/// Thrown from the expectedAs() method if a cast cannot be made as expected.
DENG2_ERROR(CastError);

/*
 * Convenience wrappers for dynamic_cast.
 */

template <typename X_, typename T_>
inline bool is(T_ *ptr) {
    return dynamic_cast<X_ *>(ptr) != nullptr;
}

template <typename X_, typename T_>
inline bool is(T_ const *ptr) {
    return dynamic_cast<X_ const *>(ptr) != nullptr;
}

template <typename X_, typename T_>
inline bool is(T_ &obj) {
    return dynamic_cast<X_ *>(&obj) != nullptr;
}

template <typename X_, typename T_>
inline bool is(T_ const &obj) {
    return dynamic_cast<X_ const *>(&obj) != nullptr;
}

template <typename X_, typename T_>
inline X_ *maybeAs(T_ *ptr) {
    return dynamic_cast<X_ *>(ptr);
}

template <typename X_, typename T_>
inline X_ const *maybeAs(T_ const *ptr) {
    return dynamic_cast<X_ const *>(ptr);
}

template <typename X_, typename T_>
inline X_ *maybeAs(T_ &obj) {
    return dynamic_cast<X_ *>(&obj);
}

template <typename X_, typename T_>
inline X_ const *maybeAs(T_ const &obj) {
    return dynamic_cast<X_ const *>(&obj);
}

template <typename X_, typename T_>
inline X_ &expectedAs(T_ *ptr) {
    if (auto *t = maybeAs<X_>(ptr)) return *t;
    DENG2_ASSERT(false);
    throw CastError(QString("Cannot cast %1 to %2").arg(DENG2_TYPE_NAME(T_)).arg(DENG2_TYPE_NAME(X_)));
}

template <typename X_, typename T_>
inline X_ const &expectedAs(T_ const *ptr) {
    if (auto const *t = maybeAs<X_>(ptr)) return *t;
    DENG2_ASSERT(false);
    throw CastError(QString("Cannot cast %1 to %2").arg(DENG2_TYPE_NAME(T_)).arg(DENG2_TYPE_NAME(X_)));
}

/**
 * Interface for all private instance implementation structs.
 * In a debug build, also contains a verification code that can be used
 * to assert whether the pointed object really is derived from IPrivate.
 */
struct IPrivate {
    virtual ~IPrivate() {}
#ifdef DENG2_DEBUG
#  define DENG2_IPRIVATE_VERIFICATION 0xdeadbeef
    unsigned int _privateImplVerification = DENG2_IPRIVATE_VERIFICATION;
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
    DENG2_NO_COPY  (PrivateAutoPtr)
    DENG2_NO_ASSIGN(PrivateAutoPtr)

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
            DENG2_ASSERT(ip->privateImplVerification() == DENG2_IPRIVATE_VERIFICATION);
            delete ip;
        }
        ptr = p;
    }
    inline ImplType *get() const {
        return ptr;
    }
    inline ImplType const *getConst() const {
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
#ifdef DENG2_DEBUG
    bool isValid() const {
        return ptr && reinterpret_cast<IPrivate *>(ptr)->privateImplVerification() == DENG2_IPRIVATE_VERIFICATION;
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
    DENG2_ASSERT(sizeof(void *) == sizeof(ToType));

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

template <typename ContainerType>
inline ContainerType mapInPlace(ContainerType &c,
                                std::function<typename ContainerType::value_type (
                                    typename ContainerType::value_type const &)> func) {
    for (auto i = c.begin(); i != c.end(); ++i) {
        *i = func(*i);
    }
}

template <typename ContainerType>
inline ContainerType map(ContainerType const &c,
                         std::function<typename ContainerType::value_type (
                             typename ContainerType::value_type const &)> func) {
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
inline ContainerType filter(ContainerType const &c,
                            std::function<bool (typename ContainerType::value_type const &)> func) {
    ContainerType out;
    for (auto i = c.begin(); i != c.end(); ++i) {
        if (func(*i)) out.push_back(*i);
    }
    return out;
}

} // namespace de
#endif // __cplusplus

#if defined(__cplusplus) && !defined(DENG2_C_API_ONLY)
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
void applyFlagOperation(FlagsType &flags, FlagsCompatibleType const &newFlags, FlagOpArg operation) {
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
    Anticlockwise = 0,
    Clockwise     = 1
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
    DENG2_PROTOCOL_1_9_10 = 0,

    DENG2_PROTOCOL_1_10_0 = 0,

    DENG2_PROTOCOL_1_11_0_Time_high_performance = 1,
    DENG2_PROTOCOL_1_11_0 = 1,

    DENG2_PROTOCOL_1_12_0 = 1,

    DENG2_PROTOCOL_1_13_0 = 1,

    DENG2_PROTOCOL_1_14_0_LogEntry_metadata = 2,
    DENG2_PROTOCOL_1_14_0 = 2,

    DENG2_PROTOCOL_1_15_0_NameExpression_with_scope_identifier = 3,
    DENG2_PROTOCOL_1_15_0 = 3,

    DENG2_PROTOCOL_2_0_0  = 3,

    DENG2_PROTOCOL_2_1_0  = 3,

    DENG2_PROTOCOL_2_2_0_NameExpression_identifier_sequence = 4,
    DENG2_PROTOCOL_2_2_0  = 4,

    DENG2_PROTOCOL_LATEST = DENG2_PROTOCOL_2_2_0
};

//@{
/// @ingroup types
typedef qint8   dchar;      ///< 8-bit signed integer.
typedef quint8  dbyte;      ///< 8-bit unsigned integer.
typedef quint8  duchar;     ///< 8-bit unsigned integer.
typedef dchar   dint8;      ///< 8-bit signed integer.
typedef dbyte   duint8;     ///< 8-bit unsigned integer.
typedef qint16  dint16;     ///< 16-bit signed integer.
typedef quint16 duint16;    ///< 16-bit unsigned integer.
typedef dint16  dshort;     ///< 16-bit signed integer.
typedef duint16 dushort;    ///< 16-bit unsigned integer.
typedef qint32  dint32;     ///< 32-bit signed integer.
typedef quint32 duint32;    ///< 32-bit unsigned integer.
typedef dint32  dint;       ///< 32-bit signed integer.
typedef duint32 duint;      ///< 32-bit unsigned integer.
typedef qint64  dint64;     ///< 64-bit signed integer.
typedef quint64 duint64;    ///< 64-bit unsigned integer.
typedef float   dfloat;     ///< 32-bit floating point number.
typedef double  ddouble;    ///< 64-bit floating point number.
typedef size_t  dsize;      // Likely unsigned long.
typedef long    dlong;

// Pointer-integer conversion (used for legacy code).
#ifdef DENG2_64BIT
typedef duint64 dintptr;
#else
typedef duint32 dintptr;
#endif
//@}

} // namespace de

#include "Error"

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
typedef size_t          dsize;
typedef long            dlong;

#endif // !__cplusplus

#endif // LIBCORE_H
