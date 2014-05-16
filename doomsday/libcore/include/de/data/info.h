/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_INFO_H
#define LIBDENG2_INFO_H

#include "../String"
#include "../NativePath"
#include <QStringList>
#include <QHash>

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
class DENG2_PUBLIC Info
{
public:
    class BlockElement;

    /**
     * Base class for all elements.
     */
    class DENG2_PUBLIC Element {
    public:
        enum Type {
            None,
            Key,
            List,
            Block
        };

        /// Value of a key/list element.
        struct Value {
            enum Flag {
                Script = 0x1,       ///< Assigned with $= (to be parsed as script).
                DefaultFlags = 0
            };
            Q_DECLARE_FLAGS(Flags, Flag)

            String text;
            Flags flags;

            Value(String const &txt = "", Flags const &f = DefaultFlags)
                : text(txt), flags(f) {}

            operator String const & () const { return text; }
        };
        typedef QList<Value> ValueList;

        /**
         * @param type  Type of the element.
         * @param name  Case-independent name of the element.
         */
        Element(Type type = None, String const &name = "");
        virtual ~Element();

        void setParent(BlockElement *parent);
        BlockElement *parent() const;

        void setSourceLocation(String const &sourcePath, int line);
        String sourcePath() const;
        int lineNumber() const;
        String sourceLocation() const;

        Type type() const;
        bool isKey() const { return type() == Key; }
        bool isList() const { return type() == List; }
        bool isBlock() const { return type() == Block; }
        String const &name() const;

        void setName(String const &name);

        /// Convenience for case-insensitively checking if the name matches @a name.
        inline bool isName(String const &str) const {
            return !name().compareWithoutCase(str);
        }

        virtual ValueList values() const = 0;

        DENG2_AS_IS_METHODS()

    private:
        DENG2_PRIVATE(d)
    };

    /**
     * Element that contains a single string value.
     */
    class DENG2_PUBLIC KeyElement : public Element {
    public:
        enum Flag {
            Attribute = 0x1,
            DefaultFlags = 0
        };
        Q_DECLARE_FLAGS(Flags, Flag)

    public:
        KeyElement(String const &name, Value const &value, Flags const &f = DefaultFlags)
            : Element(Key, name), _value(value), _flags(f) {}

        void setValue(Value const &v) { _value = v; }
        Value const &value() const { return _value; }

        Flags flags() const { return _flags; }

        ValueList values() const {
            return ValueList() << _value;
        }

    private:
        Value _value;
        Flags _flags;
    };

    /**
     * Element that contains a list of string values.
     */
    class DENG2_PUBLIC ListElement : public Element {
    public:
        ListElement(String const &name) : Element(List, name) {}
        void add(Value const &v) { _values << v; }
        ValueList values() const { return _values; }

    private:
        ValueList _values;
    };

    /**
     * Contains other Elements, including other block elements. In addition to a name,
     * each block may have a "block type", which is a lower case identifier (always
     * forced to lower case).
     */
    class DENG2_PUBLIC BlockElement : public Element {
    public:
        DENG2_ERROR(ValuesError);

        typedef QHash<String, Element *> Contents;
        typedef QList<Element *> ContentsInOrder;

    public:
        BlockElement(String const &bType, String const &name, Info &document)
            : Element(Block, name), _info(document) {
            setBlockType(bType);
        }
        ~BlockElement();

        /**
         * The root block is the only one that does not have a block type.
         */
        bool isRootBlock() const { return _blockType.isEmpty(); }

        Info &info() const { return _info; }

        String const &blockType() const { return _blockType; }

        ContentsInOrder const &contentsInOrder() const { return _contentsInOrder; }

        Contents const &contents() const { return _contents; }

        ValueList values() const {
            throw ValuesError("Info::BlockElement::values",
                              "Block elements do not contain text values (only other elements)");
        }

        int size() const { return _contents.size(); }

        bool contains(String const &name) { return _contents.contains(name.toLower()); }

        void setBlockType(String const &bType) { _blockType = bType.toLower(); }

        void clear();

        void add(Element *elem);

        Element *find(String const &name) const;

        template <typename T>
        T *findAs(String const &name) const {
            return dynamic_cast<T *>(find(name));
        }

        /**
         * Finds the value of a key inside the block. If the element is not a
         * key element, returns an empty string.
         *
         * @param name  Name of a key element in the block.
         */
        Value keyValue(String const &name) const;

        /**
         * Looks for an element based on a path where a colon ':' is used to
         * separate element names. Whitespace before and after a separator is
         * ignored.
         *
         * @param path  Name path.
         *
         * @return  The located element, or @c NULL.
         */
        Element *findByPath(String const &path) const;

        /**
         * Moves all elements in this block to the destination block. This block
         * will be empty afterwards.
         *
         * @param destination  Block.
         */
        void moveContents(BlockElement &destination);

    private:
        Info &_info;
        String _blockType;
        Contents _contents; // indexed in lower case
        ContentsInOrder _contentsInOrder;
    };

    /**
     * Interface for objects that provide included document content. @ingroup data
     */
    class DENG2_PUBLIC IIncludeFinder
    {
    public:
        virtual ~IIncludeFinder() {}

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
        virtual String findIncludedInfoSource(String const &includeName, Info const &from,
                                              String *sourcePath) const = 0;

        /// The included document could not be found. @ingroup errors
        DENG2_ERROR(NotFoundError);
    };

public:
    /// The parser encountered a syntax error in the source file. @ingroup errors
    DENG2_ERROR(SyntaxError);

public:
    Info();

    /**
     * Sets the finder for included documents. By default, attempts to locate Info files
     * by treating the name of the included file as an absolute path.
     *
     * @param finder  Include finder object. Info does not take ownership.
     */
    void setFinder(IIncludeFinder const &finder);

    void useDefaultFinder();

    /**
     * Parses a string of text as Info source.
     *
     * @param source  Info source text.
     */
    Info(String const &source);

    /**
     * Parses a file containing Info source.
     *
     * @param file  Info source text.
     */
    Info(File const &file);

    Info(String const &source, IIncludeFinder const &finder);

    /**
     * Sets all the block types whose content is parsed using a script parser.
     * The blocks will contain a single KeyElement named "script".
     *
     * @param blocksToParseAsScript  List of block types.
     */
    void setScriptBlocks(QStringList const &blocksToParseAsScript);

    void setAllowDuplicateBlocksOfType(QStringList const &duplicatesAllowed);

    /**
     * Parses the Info contents from a text string.
     *
     * @param infoSource  Info text.
     */
    void parse(String const &infoSource);

    /**
     * Parses the Info source read from a file.
     *
     * @param file  File containing an Info document.
     */
    void parse(File const &file);

    /**
     * Parses the Info contents from a native text file.
     *
     * @param nativePath  Path of a native file containing the Info source.
     */
    void parseNativeFile(NativePath const &nativePath);

    void clear();

    void setSourcePath(String const &path);

    /**
     * Path of the source, if it has been read from a file.
     *
     * @return Source path in the file system.
     */
    String sourcePath() const;

    BlockElement const &root() const;

    /**
     * Finds an element by its path. Info paths use a colon ':' as separator.
     *
     * @param path  Path of element (case insensitive).
     *
     * @return Element, or @c NULL.
     */
    Element const *findByPath(String const &path) const;

    /**
     * Finds the value of a key.
     *
     * @param key    Key to find (case insensitive).
     * @param value  The value is returned here.
     *
     * @return @c true, if the key was found and @a value is valid. If @c
     * false, the key was not found and @a value is not changed.
     */
    bool findValueForKey(String const &key, String &value) const;

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Info::Element::Value::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Info::KeyElement::Flags)

} // namespace de

#endif // LIBDENG2_INFO_H
