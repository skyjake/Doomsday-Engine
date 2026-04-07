/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/textapp.h>
#include <de/ziparchive.h>
#include <de/block.h>
#include <de/date.h>
#include <de/fixedbytearray.h>
#include <de/reader.h>
#include <de/writer.h>
#include <de/filesystem.h>

using namespace de;

int main(int argc, char **argv)
{
    init_Foundation();
    try
    {
        TextApp app(makeList(argc, argv));
        app.initSubsystems(App::DisablePersistentData);

        Block b;
        Writer(b, littleEndianByteOrder) << duint32(0x11223344);
        duint32 v;
        Reader(b, littleEndianByteOrder) >> v;
        LOG_MSG("%x") << v;

        Folder &zip = app.fileSystem().find<Folder>("test.zip");

        LOG_MSG("Here's test.zip's info:\n") << zip.objectNamespace();
        LOG_MSG("Root folder's info:\n") << app.rootFolder().objectNamespace();

        LOG_MSG    ("General description: %s")   << zip.description();
        LOG_VERBOSE("Verbose description: %s")   << zip.description();
        LOGDEV_MSG ("Developer description: %s") << zip.description();

        const File &hello = zip.locate<File>("hello.txt");
        File::Status stats = hello.status();
        LOG_MSG("hello.txt size: %i bytes, modified at %s") << stats.size << Date(stats.modifiedAt);

        String content = String::fromUtf8(Block(hello));
        LOG_MSG("The contents: \"%s\"") << content;

        try
        {
            // Make a second entry.
            File &worldTxt = zip.createFile("world.txt");
            Writer(worldTxt) << FixedByteArray(content.toUtf8());
        }
        catch (const File::OutputError &er)
        {
            LOG_WARNING("Cannot change files in read-only mode:\n") << er.asText();
        }

        // test2.zip won't appear in the file system as a folder unless
        // FS::refresh() is called. createFile() doesn't interpret anything, just
        // makes a plain file.
        File &zip2 = app.homeFolder().replaceFile("test2.zip");
        zip2.setMode(File::Write);
        zip2.clear();
        ZipArchive arch;
        arch.add(Path("world.txt"), content.toUtf8());
        Writer(zip2) << arch;
        LOG_MSG("Wrote ") << zip2.path();
        LOG_MSG("") << zip2.objectNamespace();

        LOG_MSG    ("General description: %s")   << zip2.description();
        LOG_VERBOSE("Verbose description: %s")   << zip2.description();
        LOGDEV_MSG ("Developer description: %s") << zip2.description();

        // Manual reinterpretation can be requested.
        DE_ASSERT(zip2.parent() != nullptr);
        Folder &updated = zip2.reinterpret()->as<Folder>();
        DE_ASSERT(!zip2.parent()); // became a source

        // This should now be a package folder so let's fill it with the archive
        // contents.
        updated.populate();

        LOG_MSG("After reinterpretation: %s with path %s") << updated.description() << updated.path();

        LOG_MSG("Trying to get folder ") << updated.path() / "subtest";
        Folder &subFolder = App::fileSystem().makeFolder(updated.path() / "subtest");

        Writer(subFolder.replaceFile("world2.txt")) << content.toUtf8();
        Writer(subFolder.replaceFile("world3.txt")) << content.toUtf8();
        Writer(subFolder.replaceFile("world2.txt")) << content.toUtf8();
        Writer(subFolder.replaceFile("world2.txt")) << content.toUtf8();
        Writer(subFolder.replaceFile("world3.txt")) << content.toUtf8();

        try
        {
            File &denied = subFolder.locate<File>("world3.txt");
            denied.setMode(File::ReadOnly);
            Writer(denied) << content.toUtf8();
        }
        catch (const Error &er)
        {
            LOG_MSG("Correctly denied access to read-only file within archive: %s")
                    << er.asText();
        }

        LOG_MSG("Contents of subtest folder:\n") << updated.locate<Folder const>("subtest").contentsAsText();

        LOG_MSG("Before flushing:\n") << app.homeFolder().contentsAsText();

        (500_ms).sleep(); // make the time difference clearer

        // Changes were made to the archive via files. The archive won't be
        // written back to its source file until the ArchiveFolder instance
        // is deleted or when a flush is done.
        updated.flush();

        LOG_MSG("After flushing:\n") << app.homeFolder().contentsAsText();

        FS::copySerialized(updated.path(), "home/copied.zip", FS::PlainFileCopy);
        LOG_MSG("Plain copy: ") << App::rootFolder().locate<File const>("home/copied.zip").description();

        FS::copySerialized(updated.path(), "home/copied.zip");
        LOG_MSG("Normal copy: ") << App::rootFolder().locate<File const>("home/copied.zip").description();
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
