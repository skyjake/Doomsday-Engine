/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/File"
#include "de/App"
#include "de/FS"
#include "de/Folder"
#include "de/Date"
#include "de/NumberValue"

#include <sstream>

using namespace de;

File::File(const String& fileName)
    : parent_(0), originFeed_(0), name_(fileName)
{
    source_ = this;
    
    // Create the default set of info variables.
    info_.add(new Variable("name", new AccessorValue(*this, AccessorValue::NAME),
        Variable::TEXT | Variable::READ_ONLY | Variable::NO_SERIALIZE));
    info_.add(new Variable("path", new AccessorValue(*this, AccessorValue::PATH),
        Variable::TEXT | Variable::READ_ONLY | Variable::NO_SERIALIZE));
    info_.add(new Variable("type", new AccessorValue(*this, AccessorValue::TYPE),
        Variable::TEXT | Variable::READ_ONLY | Variable::NO_SERIALIZE));
    info_.add(new Variable("size", new AccessorValue(*this, AccessorValue::SIZE),
        Variable::TEXT | Variable::READ_ONLY | Variable::NO_SERIALIZE));
    info_.add(new Variable("modifiedAt", new AccessorValue(*this, AccessorValue::MODIFIED_AT),
        Variable::TEXT | Variable::READ_ONLY | Variable::NO_SERIALIZE));
}

File::~File()
{
    flush();    
    if(parent_)
    {
        // Remove from parent folder.
        parent_->remove(this);
    }
    deindex();
}

void File::deindex()
{
    fileSystem().deindex(*this);
}

void File::flush()
{}

FS& File::fileSystem()
{
    return App::fileSystem();
}

void File::setOriginFeed(Feed* feed)
{
    // Folders should never have an origin feed.
    assert(!dynamic_cast<Folder*>(this));

    originFeed_ = feed;
}

const String File::path() const
{
    String thePath = name();
    for(Folder* i = parent_; i; i = i->parent_)
    {
        thePath = i->name().concatenatePath(thePath);
    }
    return "/" + thePath;
}
        
void File::setSource(File* source)
{
    source_ = source;
}        
        
const File* File::source() const
{
    if(source_ != this)
    {
        return source_->source();
    }
    return const_cast<const File*>(source_);
}

File* File::source()
{
    if(source_ != this)
    {
        return source_->source();
    }
    return source_;
}

void File::setStatus(const Status& status)
{
    // The source file status is the official one.
    if(this != source_)
    {
        source()->setStatus(status);
    }
    else
    {
        status_ = status;
    }
}

const File::Status& File::status() const 
{
    if(this != source_)
    {
        return source_->status();
    }
    return status_;
}

File::Size File::size() const
{
    return 0;
}

void File::get(Offset at, Byte* values, Size count) const
{
    if(at >= size() || at + count > size())
    {
        /// @throw IByteArray::OffsetError  Attempted to read past bounds of file.
        throw OffsetError("File::get", "Out of range");
    }
}

void File::set(Offset at, const Byte* values, Size count)
{
    /// @throw ReadOnlyError  File is in read-only mode.
    throw ReadOnlyError("File::set", "File can only be read");
}

File::AccessorValue::AccessorValue(File& owner, Property prop) : owner_(owner), prop_(prop)
{
    update();
}

void File::AccessorValue::update() const
{
    // We need to alter the text value.
    AccessorValue* nonConst = const_cast<AccessorValue*>(this);
    
    std::ostringstream os;
    
    switch(prop_)
    {
    case NAME:
        nonConst->setValue(owner_.name());
        break;
        
    case PATH:
        nonConst->setValue(owner_.path());
        break;

    case TYPE:
        nonConst->setValue(owner_.status().type() == File::Status::FILE? "file" : "folder");
        break;

    case SIZE:
        os << owner_.status().size;
        nonConst->setValue(os.str());
        break;

    case MODIFIED_AT:
        os << owner_.status().modifiedAt.asDate();
        nonConst->setValue(os.str());
        break;
    }
}

Value* File::AccessorValue::duplicate() const
{
    if(prop_ == SIZE)
    {
        return new NumberValue(asNumber());
    }
    return new TextValue(asText());
}

Value::Number File::AccessorValue::asNumber() const
{
    update();
    return TextValue::asNumber();
}

Value::Text File::AccessorValue::asText() const
{
    update();
    return TextValue::asText();
}

dsize File::AccessorValue::size() const
{
    update();
    return TextValue::size();
}

bool File::AccessorValue::isTrue() const
{
    update();
    return TextValue::isTrue();
}

dint File::AccessorValue::compare(const Value& value) const
{
    update();
    return TextValue::compare(value);
}

void File::AccessorValue::sum(const Value& value)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("File::AccessorValue::sum", "File accessor values cannot be modified");
}

void File::AccessorValue::multiply(const Value& value)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("File::AccessorValue::multiply", "File accessor values cannot be modified");
}

void File::AccessorValue::divide(const Value& value)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("File::AccessorValue::divide", "File accessor values cannot be modified");
}

void File::AccessorValue::modulo(const Value& divisor)
{
    /// @throw ArithmeticError  Attempted to modify the value of the accessor.
    throw ArithmeticError("File::AccessorValue::modulo", "File accessor values cannot be modified");
}

void File::AccessorValue::operator >> (Writer& to) const
{
    /// @throw CannotSerializeError  Attempted to serialize the accessor.
    throw CannotSerializeError("File::AccessorValue::operator >>", "File accessor cannot be serialized");
}

void File::AccessorValue::operator << (Reader& from)
{
    /// @throw CannotSerializeError  Attempted to deserialize the accessor.
    throw CannotSerializeError("File::AccessorValue::operator <<", "File accessor cannot be deserialized");
}
