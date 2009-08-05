/*
 * The Doomsday Engine Project
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

#include <dengmain.h>
#include <de/data.h>
#include <de/types.h>
#include <de/fs.h>
#include "../testapp.h"

using namespace de;

int deng_Main(int argc, char** argv)
{
    using std::cout;
    using std::endl;
    
    try
    {
        CommandLine args(argc, argv);
        TestApp app(args);

        Block b;
        Writer(b, littleEndianByteOrder) << duint32(0x11223344);
        duint32 v;
        Reader(b, littleEndianByteOrder) >> v;
        cout << std::hex << v << std::dec << "\n";

        Folder& zip = app.fileSystem().find<Folder>("test.zip");
        
        cout << "Here's test.zip's info:\n";
        cout << zip.info() << "\n";
        
        cout << "Root's info:\n";
        cout << app.fileRoot().info() << "\n";
        
        const File& hello = zip.locate<File>("hello.txt");
        File::Status stats = hello.status();
        cout << "hello.txt size: " << stats.size << " bytes, modified at " << Date(stats.modifiedAt) << endl;
        
        String content(hello);
        cout << "The contents: \"" << content << "\"" << endl;

        try
        {
            // Make a second entry.
            File& worldTxt = zip.newFile("world.txt");
            Writer(worldTxt) << FixedByteArray(content);
        }
        catch(const File::IOError&)
        {
            cout << "Cannot change files in read-only mode.\n";
        }

        // This won't appear in the file system unless FS::refresh() is called.
        // newFile() doesn't interpret anything, just makes a plain file.
        File& zip2 = app.homeFolder().newFile("test2.zip");
        zip2.setMode(File::WRITE | File::TRUNCATE);
        Archive arch;
        arch.add("world.txt", content);
        Writer(zip2) << arch;
        cout << "Wrote " << zip2.path() << endl;
        cout << zip2.info() << "\n";
    }
    catch(const Error& err)
    {
        std::cout << err.what() << "\n";
    }

    std::cout << "Exiting deng_Main()...\n";
    return 0;        
}
