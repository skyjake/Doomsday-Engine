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
    
    // Create the default set of info variables common to all files.
    info_.add(new Variable("name", new Accessor(*this, Accessor::NAME), Accessor::VARIABLE_MODE));
    info_.add(new Variable("path", new Accessor(*this, Accessor::PATH), Accessor::VARIABLE_MODE));
    info_.add(new Variable("type", new Accessor(*this, Accessor::TYPE), Accessor::VARIABLE_MODE));
    info_.add(new Variable("size", new Accessor(*this, Accessor::SIZE), Accessor::VARIABLE_MODE));
    info_.add(new Variable("modifiedAt", new Accessor(*this, Accessor::MODIFIED_AT),
        Accessor::VARIABLE_MODE));
}

File::~File()
{
    flush();   
    if(source_ != this) 
    {
        // If we own a source, get rid of it.
        delete source_;
        source_ = 0;
    }
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

void File::clear()
{
    verifyWriteAccess();
}

FS& File::fileSystem()
{
    return App::fileSystem();
}

void File::setOriginFeed(Feed* feed)
{
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
    if(source_ != this)
    {
        // Delete the old source.
        delete source_;
    }
    source_ = source;
}        
        
const File* File::source() const
{
    if(source_ != this)
    {
        return source_->source();
    }
    return source_;
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
        source_->setStatus(status);
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

void File::setMode(const Mode& newMode)
{
    if(this != source_)
    {
        source_->setMode(newMode);
    }
    else
    {
        mode_ = newMode;
    }
}

const File::Mode& File::mode() const 
{
    if(this != source_)
    {
        return source_->mode();
    }
    return mode_;
}

void File::verifyWriteAccess()
{
    if(!mode()[WRITE_BIT])
    {
        /// @throw ReadOnlyError  File is in read-only mode.
        throw ReadOnlyError("File::verifyWriteAccess", path() + " is in read-only mode");
    }
}

File::Size File::size() const
{
    return status().size;
}

void File::get(Offset at, Byte* values, Size count) const
{
    if(at >= size() || at + count > size())
    {
        throw OffsetError("File::get", "Out of range");
    }
}

void File::set(Offset at, const Byte* values, Size count)
{
    verifyWriteAccess();
}

File::Accessor::Accessor(File& owner, Property prop) : owner_(owner), prop_(prop)
{
    update();
}

void File::Accessor::update() const
{
    // We need to alter the value content.
    Accessor* nonConst = const_cast<Accessor*>(this);
    
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

Value* File::Accessor::duplicateContent() const
{
    if(prop_ == SIZE)
    {
        return new NumberValue(asNumber());
    }
    return new TextValue(*this);
}
