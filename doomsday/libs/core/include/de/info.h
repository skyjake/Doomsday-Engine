#include <utility>

/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_INFO_H
#define LIBCORE_INFO_H

#include "de/nativepath.h"
#include "de/record.h"
#include "de/sourcelinetable.h"
#include "de/string.h"
#include "de/list.h"
#include "de/hash.h"

namespace de {

class File;

/**
 * Key/value tree. The tree is parsed from the "Snowberry" Info file format.
 *
 * All element names (key identifiers, block names, etc.) are case insensitive, although
 * their case is preserved when parsing the tree.
 *
 * See the Doomsday Wiki for an example of the syntax:
 * http://dengine.net/dew/index.php?title=Info
 *
 * This implementation is based on a C++ port of cfparser.py from Snowberry.
 * @ingroup data
 *
 * @todo Should use de::Lex internally.
 */
class DE_PUBLIC Info
{
public:
    /// The parser encountered a syntax error in the source file. @ingroup errors
    DE_ERROR(SyntaxError);

    class BlockElement;

    /**
     * Base class for all elements.
     */
    class DE_PUBLIC Element
    {
    public:
        enum Type { None, Key, List, Block };

        /// Value of a key/list element.
        struct Value {
            enum Flag {
                Script        = 0x1, ///< Assigned with $= (to be parsed as script).
                StringLiteral = 0x2, ///< Quoted string literal (otherwise a plain token).
                DefaultFlags  = 0
            };
            String text;
            Flags flags;

            Value(String txt = "", Flags f = DefaultFlags)
                : text(std::move(txt)), flags(f) {}

            operator const String & () const { return text; }
        };
        using ValueList = de::List<Value>;

        /**
         * @param type  Type of the element.
         * @param name  Case-independent name of the element.
         */
        Element(Type type = None, const String &name = String());
        virtual ~Element() = default;

        void setParent(BlockElement *parent);
        BlockElement *parent() const;

        void setSourceLocation(const String &sourcePath, int line);
        String sourceLocation() const;
        duint32 sourceLineId() const;

        Type type() const;
        bool isKey() const { return type() == Key; }
        bool isList() const { return type() == List; }
        bool isBlock() const { return type() == Block; }
        const String &name() const;

        void setName(const String &name);

        /// Convenience for case-insensitively checking if the name matches @a name.
        inline bool isName(const String &str) const {
            return !name().compareWithoutCase(str);
        }

        virtual ValueList values() const = 0;

        DE_CAST_METHODS()

    private:
        DE_PRIVATE(d)
    };

    /**
     * Element that contains a single string value.
     */
    class DE_PUBLIC KeyElement : public Element
    {
    public:
        enum Flag { Attribute = 0x1, DefaultFlags = 0 };

    public:
        KeyElement(const String &name, Value value, Flags f = DefaultFlags)
            : Element(Key, name)
            , _value(std::move(value))
            , _flags(f)
        {}

        Flags flags() const { return _flags; }

        void         setValue(const Value &v) { _value = v; }
        const Value &value() const { return _value; }
        ValueList    values() const override { return ValueList() << _value; }

    private:
        Value _value;
        Flags _flags;
    };

    /**
     * Element that contains a list of string values.
     */
    class DE_PUBLIC ListElement : public Element
    {
    public:
        ListElement(const String &name) : Element(List, name) {}
        void add(const Value &v) { _values << v; }
        ValueList values() const override { return _values; }

    private:
        ValueList _values;
    };

    /**
     * Contains other Elements, including other block elements. In addition to a name,
     * each block may have a "block type", which is a lower case identifier (always
     * forced to lower case).
     */
    class DE_PUBLIC BlockElement : public Element
    {
    public:
        DE_ERROR(ValuesError);

        using Contents        = Hash<String, Element *>;
        using ContentsInOrder = de::List<Element *>;

    public:
        BlockElement(const String &bType, const String &name, Info &document)
            : Element(Block, name), _info(document)
        {
            setBlockType(bType);
        }
        ~BlockElement() override;

        /**
         * The root block is the only one that does not have a block type.
         */
        bool isRootBlock() const { return _blockType.isEmpty(); }

        bool isEmpty() const { return _contents.empty(); }

        Info &info() const { return _info; }

        const String &blockType() const { return _blockType; }

        const ContentsInOrder &contentsInOrder() const { return _contentsInOrder; }

        const Contents &contents() const { return _contents; }

        ValueList values() const override {
            throw ValuesError("Info::BlockElement::values",
                              "Block elements do not contain text values (only other elements)");
        }

        dsize size() const { return _contents.size(); }

        bool contains(const String &name) const { return _contents.contains(name.lower()); }

        void setBlockType(const String &bType) { _blockType = bType.lower(); }

        void clear();

        void add(Element *elem);

        Element *find(const String &name) const;

        template <typename T>
        T *findAs(const String &name) const {
            return dynamic_cast<T *>(find(name));
        }

        /**
         * Finds the value of a key inside the block.
         *
         * @param name  Name of a key element in the block. This may also be
         *              a path where a color ':' is used for separating
         *              element names.
         * @param defaultValue  If the key is not found, this value is returned instead.
         *
         * @return Value of the key element. If the located element is not a
         * key element, returns @a defaultValue.
         */
        Value keyValue(const String &name, const String &defaultValue = String()) const;

        String operator[](const String &name) const;

        /**
         * Looks for an element based on a path where a colon ':' is used to
         * separate element names. Whitespace before and after a separator is
         * ignored.
         *
         * @param path  Name path.
         *
         * @return  The located element, or @c NULL.
         */
        Element *findByPath(const String &path) const;

        /**
         * Moves all elements in this block to the destination block. This block
         * will be empty afterwards.
         *
         * @param destination  Block.
         */
        void moveContents(BlockElement &destination);

        /**
         * Converts the contents of the block into a Record. Variables in the record
         * are named using the unmodified element names (i.e., not lowercased).
         *
         * @return Record with block elements.
         */
        Record asRecord() const;

    private:
        Info &          _info;
        String          _blockType;
        Contents        _contents; // indexed in lower case
        ContentsInOrder _contentsInOrder;
    };

    /**
     * Interface for objects that provide included document content. @ingroup data
     */
    class DE_PUBLIC IIncludeFinder
    {
    public:
        virtual ~IIncludeFinder() = default;

        /**
         * Finds an Info document.
         *
         * @param includeName  Name of the Info document as specified in an \@include
         *                     directive.
         * @param from         Info document where the inclusion occurs.
         * @param sourcePath   Optionally, the path of the Info source is returned
         *                     here (if the content was read from a file). This can
         *                     be NULL if the caller doesn't need to know the path.
         *
         * @return Content of the included document.
         */
        virtual String findIncludedInfoSource(const String &includeName, const Info &from,
                                              String *sourcePath) const = 0;

        /// The included document could not be found. @ingroup errors
        DE_ERROR(NotFoundError);
    };

public:
    Info();

    /**
     * Parses a string of text as Info source.
     *
     * @param source  Info source text.
     */
    Info(const String &source);

    /**
     * Parses a file containing Info source.
     *
     * @param file  Info source text.
     */
    Info(const File &file);

    Info(const String &source, const IIncludeFinder &finder);

    /**
     * Sets the finder for included documents. By default, attempts to locate Info files
     * by treating the name of the included file as an absolute path.
     *
     * @param finder  Include finder object. Info does not take ownership.
     */
    void setFinder(const IIncludeFinder &finder);

    void useDefaultFinder();

    /**
     * Sets all the block types whose content is parsed using a script parser.
     * The blocks will contain a single KeyElement named "script".
     *
     * @param blocksToParseAsScript  List of block types.
     */
    void setScriptBlocks(const StringList &blocksToParseAsScript);

    void setAllowDuplicateBlocksOfType(const StringList &duplicatesAllowed);

    /**
     * Sets the block type used for single-token blocks. By default, this is not set,
     * meaning that single-token blocks are treated as anonymous.
     *
     * <pre>group {
     *   key: value
     * }</pre>
     *
     * This could be interpreted as an anonymous block with type "group", or a untyped
     * block named "group". Setting the implicit block type will treat this as a
     * block of type @a implicitBlock named "group".
     *
     * However, this behavior only applies when the block type is not the same as @a
     * implicitBlock. In this example, if @a implicitBlock is set to "group", the
     * resulting block would still be anonymous and have type "group".
     *
     * @param implicitBlock  Block type for anonymous/untyped blocks.
     */
    void setImplicitBlockType(const String &implicitBlock);

    /**
     * Parses the Info contents from a text string.
     *
     * @param infoSource  Info text.
     */
    void parse(const String &infoSource);

    /**
     * Parses the Info source read from a file.
     *
     * @param file  File containing an Info document.
     */
    void parse(const File &file);

    /**
     * Parses the Info contents from a native text file.
     *
     * @param nativePath  Path of a native file containing the Info source.
     */
    void parseNativeFile(const NativePath &nativePath);

    void clear();

    void setSourcePath(const String &path);

    /**
     * Path of the source, if it has been read from a file.
     *
     * @return Source path in the file system.
     */
    String sourcePath() const;

    const BlockElement &root() const;

    /**
     * Finds an element by its path. Info paths use a colon ':' as separator.
     *
     * @param path  Path of element (case insensitive).
     *
     * @return Element, or @c nullptr.
     */
    const Element *findByPath(const String &path) const;

    /**
     * Finds the value of a key.
     *
     * @param key    Key to find (case insensitive).
     * @param value  The value is returned here.
     *
     * @return @c true, if the key was found and @a value is valid. If @c
     * false, the key was not found and @a value is not changed.
     */
    bool findValueForKey(const String &key, String &value) const;

    /**
     * Finds the value of a key.
     *
     * @param keyPath  Path of the key (':' as separator).
     *
     * @return Value text or an empty string if the key is not found.
     */
    String operator[](const String &keyPath) const;

    bool isEmpty() const;

public:
    static String quoteString(const String &text);
    static String sourceLocation(duint32 lineId);
    static const SourceLineTable &sourceLineTable();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_INFO_H
