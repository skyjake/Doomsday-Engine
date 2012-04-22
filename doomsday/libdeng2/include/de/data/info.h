/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_INFO_H
#define LIBDENG2_INFO_H

#include "../String"
#include <QStringList>
#include <QHash>

namespace de {

/**
 * Key/value tree. Read from the "Snowberry" Info file format.
 *
 * This implementation has been ported to C++ based on cfparser.py from
 * Snowberry.
 *
 * @todo Document Info syntax in wiki, with example.
 */
class Info
{
public:
    /**
     * Base class for all elements.
     */
    class Element {
    public:
        enum Type {
            None,
            Key,
            List,
            Block
        };

        /**
         * @param name   Case-independent name of the element.
         */
        Element(Type t = None, const String& n = "") : _type(t) { setName(n); }
        virtual ~Element() {}

        Type type() const { return _type; }
        bool isKey() const { return _type == Key; }
        bool isList() const { return _type == List; }
        bool isBlock() const { return _type == Block; }
        const String& name() const { return _name; }

        void setName(const String& name) { _name = name.toLower(); }

        virtual QStringList values() const = 0;

    private:
        Type _type;
        String _name;
    };

    /**
     * Element that contains a single string value.
     */
    class KeyElement : public Element {
    public:
        KeyElement(const String& name, const String& value) : Element(Key, name), _value(value) {}

        void setValue(const String& v) { _value = v; }
        const String& value() const { return _value; }

        QStringList values() const {
            QStringList list;
            list << _value;
            return list;
        }

    private:
        String _value;
    };

    /**
     * Element that contains a list of string values.
     */
    class ListElement : public Element {
    public:
        ListElement(const String& name) : Element(List, name) {}

        void add(const String& v) { _values << v; }

        QStringList values() const { return _values; }

    private:
        QStringList _values;
    };

    /**
     * Contains other Elements, including other block elements. In addition to
     * a name, each block may have a "block type", which is a case insensitive
     * identifier.
     */
    class BlockElement : public Element {
    public:
        DENG2_ERROR(ValuesError)

        typedef QHash<String, Element*> Contents;
        typedef QList<Element*> ContentsInOrder;

        BlockElement(const String& bType, const String& name) : Element(Block, name) {
            setBlockType(bType);
        }
        ~BlockElement();

        const String& blockType() const { return _blockType; }
        const ContentsInOrder& contentsInOrder() const { return _contentsInOrder; }
        const Contents& contents() const { return _contents; }

        QStringList values() const {
            throw ValuesError("Info::BlockElement::values",
                              "Block elements do not contain text values (only other elements)");
        }

        int size() const { return _contents.size(); }
        bool contains(const String& name) { return _contents.contains(name); }

        void setBlockType(const String& bType) { _blockType = bType.toLower(); }
        void clear();
        void add(Element* elem) {
            DENG2_ASSERT(elem != 0);
            _contentsInOrder.append(elem);
            _contents.insert(elem->name(), elem);
        }

        Element* find(const String& name) const {
            Contents::const_iterator found = _contents.find(name);
            if(found == _contents.end()) return 0;
            return found.value();
        }

        /**
         * Finds the value of a key inside the block. If the element is not a
         * key element, returns an empty string.
         *
         * @param name  Name of a key element in the block.
         */
        String keyValue(const String& name) const {
            Element* e = find(name);
            if(!e || !e->isKey()) return "";
            return static_cast<KeyElement*>(e)->value();
        }

    private:
        String _blockType;
        Contents _contents;
        ContentsInOrder _contentsInOrder;
    };

public:
    /// The parser encountered a syntax error in the source file. @ingroup errors
    DENG2_ERROR(SyntaxError)

public:
    Info();

    /**
     * Deserializes the key/value data from text.
     *
     * @param infoSource  Info text.
     */
    Info(const String& infoSource);

    ~Info();

    const BlockElement& root() const;

private:
    struct Instance;
    Instance* d;
};

} // namespace de

#endif // LIBDENG2_INFO_H
