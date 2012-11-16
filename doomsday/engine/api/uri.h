/**
 * @file uri.h
 *
 * Universal Resource Identifier.
 *
 * @ingroup base
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2010-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_API_URI_H
#define LIBDENG_API_URI_H

#include "dd_types.h"
#include <de/str.h>
#include <de/reader.h>
#include <de/writer.h>

/// Schemes must be at least this many characters.
#define URI_MINSCHEMELENGTH 2

/**
 * @defgroup uriComponentFlags  Uri Component Flags
 * @ingroup base
 *
 * Flags which identify the logical components of a uri. Used with Uri_Write() to
 * indicate which components of the Uri should be serialized.
 */
///@{
#define UCF_SCHEME           0x1 ///< Scheme.
#define UCF_USER             0x2 ///< User. (Reserved) Not presently implemented.
#define UCF_PASSWORD         0x4 ///< Password. (Reserved) Not presently implemented.
#define UCF_HOST             0x8 ///< Host. (Reserved) Not presently implemented.
#define UCF_PORT            0x10 ///< Port. (Reserved) Not presently implemented.
#define UCF_PATH            0x20 ///< Path.
#define UCF_FRAGMENT        0x40 ///< Fragment. (Reserved) Not presently implemented.
#define UCF_QUERY           0x80 ///< Query. (Reserved) Not presently implemented.
///@}

/**
 * @defgroup printUriFlags  Print Uri Flags
 * @ingroup base
 */
///@{
#define UPF_OUTPUT_RESOLVED           0x1 ///< Include the resolved path in the output.
#define UPF_TRANSFORM_PATH_MAKEPRETTY 0x2 ///< Transform paths making them "pretty".

#define DEFAULT_PRINTURIFLAGS (UPF_OUTPUT_RESOLVED|UPF_TRANSFORM_PATH_MAKEPRETTY)
///@}

#ifdef __cplusplus
#ifndef DENG2_C_API_ONLY

#include <de/Error>
#include <de/Log>
#include <de/NativePath>
#include <de/String>

namespace de
{
    /**
     * Convenient interface class designed to assist working with URIs to engine
     * managed resources. This class is based on the semantics defined for the
     * QUrl class, a component of the Qt GUI Toolkit.
     *
     * @todo Derive from Qt::QUrl?
     *
     * @ingroup base
     */
    class Uri : public LogEntry::Arg::Base
    {
        struct Instance; // needs to be friended by PathNode

    public:
        /// A nonexistent path node was referenced. @ingroup errors
        DENG2_ERROR(NotPathNodeError);

        /// Base class for resolve-related errors. @ingroup errors
        DENG2_ERROR(ResolveError);

        /// An unknown symbol was encountered in the embedded expression. @ingroup errors
        DENG2_SUB_ERROR(ResolveError, UnknownSymbolError);

        /// An unresolveable symbol was encountered in the embedded expression. @ingroup errors
        DENG2_SUB_ERROR(ResolveError, ResolveSymbolError);

        /// Type used to represent a path name hash key.
        typedef ushort hash_type;

        /// Range of a path name hash key; [0..hash_range)
        static hash_type const hash_range;

        /**
         * Path Node represents a name in the URI path hierarchy.
         */
        struct PathNode
        {
            /**
             * This is a hash function. It generates from the node's name a
             * somewhat-random number in the range [0..Uri::hash_range)
             *
             * @return  The generated hash key.
             */
            hash_type hash();

            /// @return  Length of this node's name in characters.
            int length() const;

            /// @return Parent of this node else @c NULL.
            PathNode* parent() const;

            String toString() const;

            friend class Uri;
            friend struct Uri::Instance;

        private:
            bool haveHashKey;
            hash_type hashKey;
            char const* from, *to;
            struct PathNode* parent_;
        };

    public:
        /**
         * Construct a default (empty) Uri instance.
         */
        Uri();

        /**
         * Construct a Uri instance from @a path.
         *
         * @param path  Path to be parsed. Assumed to be in percent-encoded representation.
         *
         * @param defaultResourceClass  If no scheme is defined in @a path and this
         *      is not @c RC_NULL, ask the resource locator whether it knows of an
         *      appropriate default scheme for this class of resource.
         */
        Uri(String path, resourceclassid_t defaultResourceClass = RC_UNKNOWN, QChar delimiter = '/');

        /**
         * Construct a Uri instance by duplicating @a other.
         */
        Uri(Uri const& other);

        ~Uri();

        Uri& operator = (Uri other);

        bool operator == (Uri const& other) const;

        bool operator != (Uri const& other) const;

        /**
         * Constructs a Uri instance from a NativePath that refers to a file in
         * the native file system. All path directives such as '~' are
         * expanded. The resultant Uri will have an empty/zero-length scheme
         * (because file paths do not include one).
         *
         * @param path  Native path to a file in the native file system.
         */
        static Uri fromNativePath(NativePath const& path);

        /**
         * Constructs a Uri instance from a NativePath that refers to a native
         * directory. All path directives such as '~' are expanded. The
         * resultant Uri will have an empty/zero-length scheme (because file
         * paths do not include one).
         *
         * @param nativeDirPath  Native path to a directory in the native
         *                       file system.
         * @param defaultResourceClass  Default resource class.
         */
        static Uri fromNativeDirPath(NativePath const& nativeDirPath, resourceclass_t defaultResourceClass = RC_NULL);

        /**
         * Constructs a Uri instance by reading it from @a reader.
         */
        //static Uri fromReader(reader_s& reader);

        /**
         * Convert this URI to a text string.
         *
         * @see asText()
         */
        operator String () const {
            return asText();
        }

        /**
         * Returns true if the path component of the URI is empty; otherwise false.
         */
        bool isEmpty() const;

        /**
         * Clear the URI returning it to an empty state.
         */
        Uri& clear();

        /**
         * Attempt to resolve this URI. Substitutes known symbolics in the possibly
         * templated path. Resulting path is a well-formed, filesys compatible path
         * (perhaps base-relative).
         *
         * @return  The resolved path.
         */
        String const& resolved() const;

        /**
         * @return  Plain-text string representation of the current scheme.
         */
        ddstring_t const* scheme() const;

        /**
         * @return  Plain-text string representation of the current path.
         */
        ddstring_t const* path() const;

        /**
         * Change the scheme of the URI to @a newScheme.
         */
        Uri& setScheme(String newScheme);

        /**
         * Change the path of the URI to @a newPath.
         */
        Uri& setPath(String newPath, QChar delimiter = '/');

        /**
         * Update this URI by parsing new values from the specified arguments.
         *
         * @param newUri  URI to be parsed. Assumed to be in percent-encoded representation.
         *
         * @param defaultResourceClass  If no scheme is defined in @a newUri and
         *      this is not @c RC_NULL, ask the resource locator whether it knows
         *      of an appropriate default scheme for this class of resource.
         */
        Uri& setUri(String newUri, resourceclassid_t defaultResourceClass = RC_UNKNOWN,
                    QChar delimiter = '/');

        /**
         * Compose from this URI a plain-text representation. Any internal encoding
         * method or symbolic identifiers will be left unchanged in the resultant
         * string (not decoded, not resolved).
         *
         * @return  Plain-text String representation.
         */
        String compose(QChar delimiter = '/') const;

        /**
         * Retrieve the path node with index @a index. Note that nodes are indexed
         * in reverse order (right to left) and NOT the autological left to right
         * order.
         *
         * For example, if the path is "c:/mystuff/myaddon.addon" the corresponding
         * path node map is arranged as follows:
         * <pre>
         *   [0:{myaddon.addon}, 1:{mystuff}, 2:{c:}].
         * </pre>
         *
         * @note The zero-length name in relative paths is also treated as a node.
         *       For example, the path "/Users/username" has three nodes.
         *
         * @param index  Reverse-index of the path node to lookup. All paths have
         *               at least one node (even empty ones) thus index @c 0 will
         *               always be valid.
         *
         * @return  Referenced path node.
         */
        PathNode& pathNode(int index) const;

        /**
         * @return  Total number of nodes in the URI path name map.
         * @see pathNode()
         */
        int pathNodeCount() const;

        /**
         * @return  First node in the URI path name map.
         * @see pathNode()
         */
        inline PathNode& firstPathNode() const {
            return pathNode(0);
        }

        /**
         * @return  Last node in the URI path name map.
         * @see pathNode()
         */
        inline PathNode& lastPathNode() const {
            return pathNode(pathNodeCount() - 1);
        }

        /**
         * Transform the URI into a human-friendly representation. Percent decoding done.
         *
         * @return  Human-friendly String representation.
         */
        String asText() const;

        /**
         * Swaps URI @a second with URI @a first.
         */
        friend void swap(Uri& first, Uri& second); // nothrow

        // Implements LogEntry::Arg::Base.
        LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::STRING; }

        /**
         * Serialize the URI using @a writer.
         *
         * @note Scheme should only be omitted when it can be unambiguously deduced
         *       from context.
         *
         * @param writer            Writer instance.
         * @param omitComponents    @ref uriComponentFlags
         */
        void write(writer_s& writer, int omitComponents = 0) const;

        /**
         * Deserializes the URI using @a reader. If the deserialized URI is missing
         * a scheme then @a defaultScheme will be used instead.
         *
         * @param reader            Reader instance.
         * @param defaultScheme     Default scheme.
         */
        Uri& read(reader_s& reader, String defaultScheme = "");

        /**
         * Print debug ouput for the URI.
         *
         * @param indent            Number of characters to indent the print output.
         * @param flags             @ref printUriFlags
         * @param unresolvedText    Text string to be printed if not resolvable.
         */
        void debugPrint(int indent, int flags = DEFAULT_PRINTURIFLAGS,
                        String unresolvedText = "") const;

    private:
        Instance* d;
    };

} // namespace de
#endif // DENG2_C_API_ONLY
#endif //__cplusplus

#ifdef __cplusplus
extern "C" {
#endif

/*
 * C wrapper API:
 */
struct uri_s; // The uri instance (opaque).

/**
 * Uri instance. Created with Uri_New() or one of the other constructors.
 */
typedef struct uri_s Uri;

/**
 * Constructs a default (empty) Uri instance. The uri should be destroyed with
 * Uri_Delete() once it is no longer needed.
 */
Uri* Uri_New(void);

/**
 * Constructs a Uri instance from @a path. The uri should be destroyed with
 * Uri_Delete() once it is no longer needed.
 *
 * @param path  Path to be parsed. Assumed to be in percent-encoded representation.
 * @param defaultResourceClass  If no scheme is defined in @a path and this is not @c RC_NULL,
 *      look for an appropriate default scheme for this class of resource.
 */
Uri* Uri_NewWithPath2(char const* path, resourceclassid_t defaultResourceClass);
Uri* Uri_NewWithPath(char const* path);

/**
 * Constructs a Uri instance by duplicating @a other. The uri should be destroyed
 * with Uri_Delete() once it is no longer needed.
 */
Uri* Uri_Dup(Uri const* other);

/**
 * Constructs a Uri instance by reading it from @a reader.  The uri should be
 * destroyed with Uri_Delete() once it is no longer needed.
 */
Uri* Uri_FromReader(Reader* reader);

/**
 * Destroys the uri.
 */
void Uri_Delete(Uri* uri);

/**
 * Returns true if the path component of the URI is empty; otherwise false.
 * @param uri  Uri instance.
 */
boolean Uri_IsEmpty(Uri const* uri);

/**
 * Clears the uri, returning it to an empty state.
 * @param uri  Uri instance.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_Clear(Uri* uri);

/**
 * Copy the contents of @a other into this uri.
 *
 * @param uri  Uri instance.
 * @param other  Uri to be copied.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_Copy(Uri* uri, Uri const* other);

/**
 * Attempt to compose a resolved copy of this Uri. Substitutes known symbolics
 * in the possibly templated path. Resulting path is a well-formed, filesys
 * compatible path (perhaps base-relative).
 *
 * @param uri  Uri instance.
 *
 * @return  Resolved path else @c NULL if non-resolvable.
 */
AutoStr* Uri_Resolved(Uri const* uri);

/**
 * @param uri  Uri instance.
 * @return  Plain-text String representation of the current scheme.
 */
ddstring_t const* Uri_Scheme(Uri const* uri);

/**
 * @param uri  Uri instance.
 * @return  Plain-text String representation of the current path.
 */
ddstring_t const* Uri_Path(Uri const* uri);

/**
 * @param uri     Uri instance.
 * @param scheme  New scheme to be parsed.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_SetScheme(Uri* uri, char const* scheme);

/**
 * @param uri   Uri instance.
 * @param path  New path to be parsed.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_SetPath(Uri* uri, char const* path);

/**
 * Update uri by parsing new values from the specified arguments.
 *
 * @param uri   Uri instance.
 * @param path  Path to be parsed. Assumed to be in percent-encoded representation.
 * @param defaultResourceClass  If no scheme is defined in @a path and this is not
 *              @c RC_NULL, look for an appropriate default scheme for this class
 *              of resource.
 *
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_SetUri2(Uri* uri, char const* path, resourceclassid_t defaultResourceClass);
Uri* Uri_SetUri(Uri* uri, char const* path/* defaultResourceClass = RC_UNKNOWN*/);

Uri* Uri_SetUriStr(Uri* uri, ddstring_t const* path);

/**
 * Transform the uri into a plain-text representation. Any internal encoding method
 * or symbolic identifiers will be included in their original, unresolved form in
 * the resultant string.
 *
 * @param uri  Uri instance.
 * @return  Plain-text String representation.
 */
AutoStr* Uri_Compose(Uri const* uri);

/**
 * Transform the uri into a human-friendly representation (percent decoding is done).
 *
 * @param uri  Uri instance.
 *
 * @return  Human-friendly String representation.
 */
AutoStr* Uri_ToString(Uri const* uri);

/**
 * Are these two uri instances considered equal once resolved?
 *
 * @todo Return a delta of lexicographical difference.
 *
 * @param uri  Uri instance.
 * @param other  Other uri instance.
 */
boolean Uri_Equality(Uri const* uri, Uri const* other);

/**
 * Serialize @a uri using @a writer.
 *
 * @note Scheme should only be omitted when it can be unambiguously deduced from context.
 *
 * @param uri               Uri instance.
 * @param writer            Writer instance.
 * @param omitComponents    @ref uriComponentFlags
 */
void Uri_Write2(Uri const* uri, Writer* writer, int omitComponents);
void Uri_Write(Uri const* uri, Writer* writer/*, omitComponents = 0 (include everything)*/);

/**
 * Deserializes @a uri using @a reader.
 *
 * @param uri  Uri instance.
 * @param reader  Reader instance.
 * @return  Same as @a uri, for caller convenience.
 */
Uri* Uri_Read(Uri* uri, Reader* reader);

/**
 * Deserializes @a uri using @a reader. If the deserialized Uri lacks a scheme,
 * @a defaultScheme will be used instead.
 *
 * @param uri               Uri instance.
 * @param reader            Reader instance.
 * @param defaultScheme     Default scheme.
 */
void Uri_ReadWithDefaultScheme(Uri* uri, Reader* reader, char const* defaultScheme);

/**
 * Print debug output for @a uri.
 *
 * @param uri               Uri instance.
 * @param indent            Number of characters to indent the print output.
 * @param flags             @ref printUriFlags
 * @param unresolvedText    Text string to be printed if @a uri is not resolvable.
 */
void Uri_DebugPrint3(Uri const* uri, int indent, int flags, char const* unresolvedText);
void Uri_DebugPrint2(Uri const* uri, int indent, int flags/*, use the default unresolved text */);
void Uri_DebugPrint(Uri const* uri, int indent/*, flags = DEFAULT_PRINTURIFLAGS */);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_API_URI_H */
