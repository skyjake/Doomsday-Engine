/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/File"
#include "de/App"
#include "de/Date"
#include "de/DirectoryFeed"
#include "de/FS"
#include "de/Feed"
#include "de/Folder"
#include "de/Guard"
#include "de/LinkFile"
#include "de/NativeFile"
#include "de/NativePointerValue"
#include "de/NumberValue"
#include "de/RecordValue"
#include "de/ScriptSystem"

namespace de {

DE_PIMPL_NOREF(File)
{
    /// The source file (NULL for non-interpreted files).
    File *source;

    /// Feed that generated the file. This feed is called upon when the file needs
    /// to be pruned. May also be NULL.
    Feed *originFeed;

    /// Status of the file.
    Status status;

    /// Mode flags.
    Flags mode;

    /// File information.
    Record info;

    Impl() : source(0), originFeed(0) {}

    DE_PIMPL_AUDIENCE(Deletion)
};

DE_AUDIENCE_METHOD(File, Deletion)

File::File(String const &fileName) : Node(fileName), d(new Impl)
{
    d->source = this;

    // Pointer back to the File for script bindings.
    d->info.add(Record::VAR_NATIVE_SELF).set(new NativePointerValue(this)).setReadOnly();

    // Core.File provides the member functions for files.
    d->info.addSuperRecord(ScriptSystem::builtInClass(QStringLiteral("File")));
}

File::~File()
{
    DE_GUARD(this);

    DE_FOR_AUDIENCE2(Deletion, i) i->fileBeingDeleted(*this);

    flush();
    if (d->source != this)
    {
        // If we own a source, get rid of it.
        delete d->source;
        d->source = 0;
    }
    if (Folder *parentFolder = parent())
    {
        // Remove from parent folder.
        parentFolder->remove(this);
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

FileSystem &File::fileSystem()
{
    return DE_APP->fileSystem();
}

Folder *File::parent() const
{
    return maybeAs<Folder>(Node::parent());
}

String File::description(int verbosity) const
{
    DE_GUARD(this);

    // describe() gives the actual description of this file.
    String desc = describe();

    // Check for additional contextual information that may be relevant. First
    // determine if this is being called for a log entry.
    if (verbosity < 0)
    {
        Log &log = Log::threadLog();
        if (log.isStaging())
        {
            verbosity = 0;
            if (log.currentEntryMetadata() & LogEntry::Dev)
            {
                // For dev entries, use a full description.
                verbosity = 2;
            }
            else if ((log.currentEntryMetadata() & LogEntry::LevelMask) <= LogEntry::Verbose)
            {
                // Verbose entries can contain some additional information.
                verbosity = 1;
            }
        }
    }

    if (verbosity > 1)
    {
        if (!mode().testFlag(Write))
        {
            desc = "read-only " + desc;
        }
        if (parent())
        {
            desc += " (path \"" + path() + "\")";
        }
    }

    // In case of DirectoryFeed, the native file desciption itself already contains
    // information about the full native path, so we don't have to describe the
    // feed itself (would be redundant).
    if (originFeed() && (verbosity >= 2 || !is<DirectoryFeed>(originFeed())))
    {
        String const feedDesc = originFeed()->description();
        if (!desc.contains(feedDesc)) // A bit of a kludge; don't repeat the feed description.
        {
            desc += " from " + feedDesc;
        }
    }

#ifdef DE_DEBUG
    // Describing the source file is usually redundant (or even misleading),
    // so only do that in debug builds so that developers can see that a
    // file interpretation is being applied.
    if (source() != this)
    {
        desc += _E(s)_E(D) " {\"" + path() + "\": data sourced from " +
                source()->description(verbosity) + "}" _E(.)_E(.);
    }
#endif

    return desc;
}

String File::describe() const
{
    return "abstract File";
}

void File::setOriginFeed(Feed *feed)
{
    DE_GUARD(this);

    d->originFeed = feed;
}

Feed *File::originFeed() const
{
    DE_GUARD(this);

    return d->originFeed;
}

void File::setSource(File *source)
{
    DE_GUARD(this);

    if (d->source != this)
    {
        // Delete the old source.
        delete d->source;
    }
    d->source = source;
}

File const *File::source() const
{
    DE_GUARD(this);

    if (&target() != this)
    {
        return target().source();
    }
    if (d->source != this)
    {
        return d->source->source();
    }
    return d->source;
}

File *File::source()
{
    DE_GUARD(this);

    if (&target() != this)
    {
        return target().source();
    }
    if (d->source != this)
    {
        return d->source->source();
    }
    return d->source;
}

File &File::target()
{
    return *this;
}

File const &File::target() const
{
    return *this;
}

void File::setStatus(Status const &status)
{
    DE_GUARD(this);

    // The source file status is the official one.
    if (this != d->source)
    {
        d->source->setStatus(status);
    }
    else
    {
        d->status = status;
    }
}

File::Status const &File::status() const
{
    DE_GUARD(this);

    if (this != d->source)
    {
        return d->source->status();
    }
    return d->status;
}

void File::setMode(Flags const &newMode)
{
    DE_GUARD(this);

    // Implicitly flush the file before switching away from write mode.
    if (d->mode.testFlag(Write) && !newMode.testFlag(Write))
    {
        flush();
    }

    if (this != d->source)
    {
        d->source->setMode(newMode);
    }
    else
    {
        d->mode = newMode;
    }
}

void File::setMode(Flags flags, FlagOpArg op)
{
    applyFlagOperation(d->mode, flags, op);
}

Record &File::objectNamespace()
{
    return d->info;
}

Record const &File::objectNamespace() const
{
    return d->info;
}

File::Flags const &File::mode() const
{
    DE_GUARD(this);

    if (this != d->source)
    {
        return d->source->mode();
    }
    return d->mode;
}

void File::verifyWriteAccess()
{
    if (!mode().testFlag(Write))
    {
        /// @throw ReadOnlyError  File is in read-only mode.
        throw ReadOnlyError("File::verifyWriteAccess", path() + " is in read-only mode");
    }
}

File *File::reinterpret()
{
    Folder *folder  = parent();
    File *original  = source();
    bool deleteThis = false;

    if (original != this)
    {
        // Already interpreted. The current interpretation will be replaced.
        DE_ASSERT(!original->parent());
        d->source = 0; // source is owned, so take it away
        deleteThis = true;
    }
    if (folder)
    {
        folder->remove(*this);
        deindex();
    }

    original->flush();
    File *result = fileSystem().interpret(original);

    // The interpreter should use whatever origin feed the file was previously using.
    result->setOriginFeed(d->originFeed);

    if (deleteThis)
    {
        DE_ASSERT(result != this);
        delete this;
    }
    if (folder)
    {
        folder->add(result);
        fileSystem().index(*result);
    }
    return result;
}

NativePath File::correspondingNativePath() const
{
    if (NativeFile const *native = maybeAs<NativeFile>(source()))
    {
        return native->nativePath();
    }
    else if (Folder const *folder = maybeAs<Folder>(target()))
    {
        if (auto *feed = folder->primaryFeedMaybeAs<DirectoryFeed>())
        {
            return feed->nativePath();
        }
    }
    return NativePath();
}

IOStream &File::operator << (IByteArray const &bytes)
{
    DE_UNUSED(bytes);
    throw OutputError("File::operator <<", description() + " does not accept a byte stream");
}

IIStream &File::operator >> (IByteArray &bytes)
{
    DE_UNUSED(bytes);
    throw InputError("File::operator >>", description() + " does not produce a byte stream");
}

IIStream const &File::operator >> (IByteArray &bytes) const
{
    DE_UNUSED(bytes);
    throw InputError("File::operator >>", description() + " does not offer an immutable byte stream");
}

static bool sortByNameAsc(File const *a, File const *b)
{
    return a->name().compareWithoutCase(b->name()) < 0;
}

String File::fileListAsText(QList<File const *> files)
{
    qSort(files.begin(), files.end(), sortByNameAsc);

    String txt;
    foreach (File const *f, files)
    {
        // One line per file.
        if (!txt.isEmpty()) txt += "\n";

        // Folder / Access flags / source flag / has origin feed.
        String flags = QString("%1%2%3%4%5")
                .arg(is<Folder>(f)?                'd' : '-')
                .arg(f->mode().testFlag(Write)?    'w' : 'r')
                .arg(f->mode().testFlag(Truncate)? 't' : '-')
                .arg(f->source() != f?             'i' : '-')
                .arg(f->originFeed()?              'f' : '-');

        txt += flags + QString("%1 %2 %3")
                .arg(f->size(), 9)
                .arg(f->status().modifiedAt.asText(), 23)
                .arg(f->name());

        // Link target.
        if (LinkFile const *link = maybeAs<LinkFile>(f))
        {
            if (!link->isBroken())
            {
                txt += QString(" -> %1").arg(link->target().path());
            }
            else
            {
                txt += " (broken link)";
            }
        }
    }
    return txt;
}

dsize File::size() const
{
    return status().size;
}

Block File::metaId() const
{
    auto const &st = target().status();
    return md5Hash(path(), duint64(st.size), st.modifiedAt);
}

} // namespace de
