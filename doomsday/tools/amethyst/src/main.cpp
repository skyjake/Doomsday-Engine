/*
 * Copyright (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "commandline.h"
#include "processor.h"
#include "defs.h"
#include <QtCore/QCoreApplication>
#include <QDebug>

#ifndef _WIN32
void messagePrinter(QtMsgType type, const char* msg)
{
    switch(type)
    {
    case QtDebugMsg:
    case QtWarningMsg:
    case QtCriticalMsg:
        fwprintf(stderr, L"%s\n", msg);
        break;

    case QtFatalMsg:
        fwprintf(stderr, L"Fatal Error: %s\n", msg);
        abort();
    }
}
#endif

void printBanner(void)
{
#ifdef _WIN32
    qDebug() << "### Amethyst (c) 2002-2011 Jaakko Keranen <jaakko.keranen@iki.fi>";
#else
    fwprintf(stderr, L"%ls\n", L"### Amethyst (c) 2002-2011 Jaakko Keränen <jaakko.keranen@iki.fi>");
#endif
    qDebug() << "### "BUILD_STR;
}

void printUsage(void)
{
    // Print to cout so it can be redirected easily.
    qDebug() << "Usage: amethyst [opts] source source ...";
    qDebug() << "Input will be read from stdin if no source files specified.";
    qDebug() << "By default, output goes to stdout.";
    qDebug() << "Options:";
    qDebug() << "-dNAME          Define an empty macro called NAME.";
    qDebug() << "-eEXT           Replace the extension of the output file with \".EXT\".";
    qDebug() << "-iDIR           Define an additional include directory.";
    qDebug() << "-oFILE          Output to FILE.";
    qDebug() << "-s              Print a dump of the Shards.";
    qDebug() << "-g              Print a dump of the Gems.";
    qDebug() << "-c              Print a dump of the Schedule.";
    qDebug() << "--help, -h, -?  Show usage information.";
}

int main(int argc, char **argv)
{
#ifndef _WIN32
    qInstallMsgHandler(messagePrinter);
#endif
    QCoreApplication a(argc, argv); // event loop never started

    CommandLine args(argc, argv);
    Processor amethyst;
    bool filesFound = false;
    QTextStream stdOutput(stdout, QIODevice::WriteOnly | QIODevice::Text);
    QTextStream stdInput(stdin, QIODevice::ReadOnly | QIODevice::Text);
    QScopedPointer<QTextStream> fileOutput;
    QFile outFile;
    String outExt;

    printBanner();

    if(args.exists("-h") || args.exists("-?") || args.exists("--help"))
    {
        printUsage();
        return 0;
    }

    // By default, use stdout.
    QTextStream* output = &stdOutput;

    if(args.exists("-s")) amethyst.setMode(PMF_DUMP_SHARDS);
    if(args.exists("-g")) amethyst.setMode(PMF_DUMP_GEMS);
    if(args.exists("-c")) amethyst.setMode(PMF_DUMP_SCHEDULE);

    // Process command line arguments.
    for(int i = 1; i < argc; i++)
    {
        if(args.beginsWith(i, "-o"))
        {
            // Open the given file for output.
            String name = args.at(i).mid(2);
            if(!outExt.isEmpty() && name.contains("."))
            {
                // Replace the current extension with outExt.
                name = name.left(name.indexOf(".")) + "." + outExt;
            }
            // Open the new output file.
            outFile.close();
            outFile.setFileName(name);
            if(!outFile.open(QFile::WriteOnly | QFile::Truncate))
            {
                qWarning() << (name + ": " + outFile.errorString()).toLatin1().data();
                exit(1);
            }
            fileOutput.reset(new QTextStream(&outFile));
            output = fileOutput.data();
        }
        else if(args.beginsWith(i, "-e"))
        {
            outExt = args.at(i).mid(2);
        }
        else if(args.beginsWith(i, "-d"))
        {
            amethyst.define(args.at(i).mid(2));
        }
        else if(args.beginsWith(i, "-i"))
        {
            amethyst.addIncludePath(args.at(i).mid(2));
        }
        else if(args.at(i)[0] != '-')
        {
            // This must be a source file.
            filesFound = true;
            QFile inFile(args.at(i));
            if(!inFile.open(QFile::ReadOnly | QFile::Text))
            {
                qWarning() << (inFile.fileName() + ": " + inFile.errorString()).toLatin1().data();
                exit(1);
            }
            QTextStream source(&inFile);
            qDebug() << inFile.fileName().toUtf8().data();

            amethyst.setSourceName(args.at(i));
            amethyst.compile(source, *output);
        }
    }

    // If no source files were specified, read from stdin.
    if(!filesFound) amethyst.compile(stdInput, *output);

    return 0;
}
