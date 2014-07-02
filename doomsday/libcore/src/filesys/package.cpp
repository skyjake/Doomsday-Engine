/** @file package.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Package"
#include "de/PackageLoader"
#include "de/Process"
#include "de/Script"
#include "de/App"

namespace de {

DENG2_PIMPL(Package)
, DENG2_OBSERVES(File, Deletion)
{
    File const *file;

    Instance(Public *i, File const *f)
        : Base(i)
        , file(f)
    {
        if(file) file->audienceForDeletion() += this;
    }

    ~Instance()
    {
        if(file) file->audienceForDeletion() -= this;
    }

    void fileBeingDeleted(File const &)
    {
        file = 0;
    }

    void verifyFile() const
    {
        if(!file)
        {
            throw SourceError("Package::verify", "Package's source file missing");
        }
    }
};

Package::Package(File const &file) : d(new Instance(this, &file))
{}

Package::~Package()
{}

File const &Package::file() const
{
    d->verifyFile();
    return *d->file;
}

void Package::validateMetadata(Record const &packageInfo)
{
    if(!packageInfo.has("name"))
    {
        throw IncompleteMetadataError("Package::validateMetadata",
                                      "Package does not have a name");
    }

    if(!packageInfo.has("version"))
    {
        throw IncompleteMetadataError("Package::validateMetadata",
                                      "Package '" + packageInfo.gets("name") +
                                      "' does not have a version");
    }
}

Folder const *Package::root() const
{
    return d->file->maybeAs<Folder>();
}

Record &Package::info()
{
    d->verifyFile();
    return const_cast<File *>(d->file)->info();
}

Record const &Package::info() const
{
    return const_cast<Package *>(this)->info();
}

String Package::identifier() const
{
    d->verifyFile();
    return identifierForFile(*d->file);
}

bool Package::executeFunction(String const &name)
{
    if(info().has(name))
    {
        Script script(name + "()");
        // The global namespace for this function is the package's info namespace.
        Process proc(&info());
        proc.run(script);
        proc.execute();
        return true;
    }
    return false;
}

void Package::didLoad()
{
    executeFunction(L"onLoad");
}

void Package::aboutToUnload()
{
    executeFunction(L"onUnload");
}

String Package::identifierForFile(File const &file)
{
    return file.name().fileNameWithoutExtension();
}

} // namespace de
