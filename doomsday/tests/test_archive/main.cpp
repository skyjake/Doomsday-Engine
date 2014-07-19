/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/TextApp>
#include <de/ZipArchive>
#include <de/Block>
#include <de/Date>
#include <de/FixedByteArray>
#include <de/Reader>
#include <de/Writer>
#include <de/FS>

#include <QDebug>

using namespace de;

int main(int argc, char **argv)
{
    try
    {
        TextApp app(argc, argv);
        app.initSubsystems(App::DisablePlugins);

        Block b;
        Writer(b, littleEndianByteOrder) << duint32(0x11223344);
        duint32 v;
        Reader(b, littleEndianByteOrder) >> v;
        LOG_MSG("%x") << v;

        Folder &zip = app.fileSystem().find<Folder>("test.zip");

        LOG_MSG("Here's test.zip's info:\n") << zip.info();
        LOG_MSG("Root folder's info:\n") << app.rootFolder().info();
        
        LOG_MSG    ("General description: %s")   << zip.description();
        LOG_VERBOSE("Verbose description: %s")   << zip.description();
        LOGDEV_MSG ("Developer description: %s") << zip.description();

        File const &hello = zip.locate<File>("hello.txt");
        File::Status stats = hello.status();
        LOG_MSG("hello.txt size: %i bytes, modified at %s") << stats.size << Date(stats.modifiedAt);
        
        String content = String::fromUtf8(Block(hello));
        LOG_MSG("The contents: \"%s\"") << content;

        try
        {
            // Make a second entry.
            File &worldTxt = zip.newFile("world.txt");
            Writer(worldTxt) << FixedByteArray(content.toUtf8());
        }
        catch(File::OutputError const &er)
        {
            LOG_WARNING("Cannot change files in read-only mode:\n") << er.asText();
        }

        // test2.zip won't appear in the file system as a folder unless
        // FS::refresh() is called. newFile() doesn't interpret anything, just
        // makes a plain file.
        File &zip2 = app.homeFolder().replaceFile("test2.zip");
        zip2.setMode(File::Write | File::Truncate);
        ZipArchive arch;
        arch.add(Path("world.txt"), content.toUtf8());        
        Writer(zip2) << arch;
        LOG_MSG("Wrote ") << zip2.path();
        LOG_MSG("") << zip2.info();

        LOG_MSG    ("General description: %s")   << zip2.description();
        LOG_VERBOSE("Verbose description: %s")   << zip2.description();
        LOGDEV_MSG ("Developer description: %s") << zip2.description();

        // Manual reinterpretation can be requested.
        DENG2_ASSERT(zip2.parent() != 0);
        Folder &updated = zip2.reinterpret()->as<Folder>();
        DENG2_ASSERT(!zip2.parent()); // became a source

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
        catch(Error const &er)
        {
            LOG_MSG("Correctly denied access to read-only file within archive: %s")
                    << er.asText();
        }

        LOG_MSG("Contents of subtest folder:\n") << updated.locate<Folder const>("subtest").contentsAsText();

        LOG_MSG("Before flushing:\n") << app.homeFolder().contentsAsText();

        TimeDelta(0.5).sleep(); // make the time difference clearer

        // Changes were made to the archive via files. The archive won't be
        // written back to its source file until the ArchiveFolder instance
        // is deleted or when a flush is done.
        updated.flush();

        LOG_MSG("After flushing:\n") << app.homeFolder().contentsAsText();

        App::fileSystem().copySerialized(updated.path(), "home/copied.zip", FS::PlainFileCopy);
        LOG_MSG("Plain copy: ") << App::rootFolder().locate<File const>("home/copied.zip").description();

        App::fileSystem().copySerialized(updated.path(), "home/copied.zip");
        LOG_MSG("Normal copy: ") << App::rootFolder().locate<File const>("home/copied.zip").description();
    }
    catch(Error const &err)
    {
        qWarning() << err.asText();
    }

    qDebug() << "Exiting main()...";
    return 0;        
}
