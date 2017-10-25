/** @file idgameslink.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/filesys/idgameslink.h"

#include <de/Async>
#include <de/data/gzip.h>

#include <QRegularExpression>

using namespace de;

IdgamesLink::IdgamesLink(String const &address)
    : filesys::WebHostedLink(address, "ls-laR.gz")
{}

void IdgamesLink::parseRepositoryIndex(QByteArray data)
{
    // This may be a long list, so let's do it in a background thread.
    // The link will be marked connected only after the data has been parsed.

    scope() += async([this, data] () -> String
    {
        Block const listing = gDecompress(data);
        QTextStream is(listing, QIODevice::ReadOnly);
        is.setCodec("UTF-8");
        QRegularExpression const reDir("^\\.?(.*):$");
        QRegularExpression const reTotal("^total\\s+\\d+$");
        QRegularExpression const reFile("^(-|d)[-rwxs]+\\s+\\d+\\s+\\w+\\s+\\w+\\s+"
                                        "(\\d+)\\s+(\\w+\\s+\\d+\\s+[0-9:]+)\\s+(.*)$",
                                        QRegularExpression::CaseInsensitiveOption);
        String currentPath;
        bool ignore = false;
        QRegularExpression const reIncludedPaths("^/(levels|music|sounds|themes)");

        std::unique_ptr<FileTree> tree(new FileTree);
        while (!is.atEnd())
        {
            if (String const line = is.readLine().trimmed())
            {
                if (!currentPath)
                {
                    // This should be a directory path.
                    auto match = reDir.match(line);
                    if (match.hasMatch())
                    {
                        currentPath = match.captured(1);
                        qDebug() << "[WebRepositoryLink] Parsing path:" << currentPath;

                        ignore = !reIncludedPaths.match(currentPath).hasMatch();
                    }
                }
                else if (!ignore && reTotal.match(line).hasMatch())
                {
                    // Ignore directory sizes.
                }
                else if (!ignore)
                {
                    auto match = reFile.match(line);
                    if (match.hasMatch())
                    {
                        bool const isFolder = (match.captured(1) == QStringLiteral("d"));
                        if (!isFolder)
                        {
                            String const name = match.captured(4);
                            if (name.startsWith(QChar('.')) || name.contains(" -> "))
                                continue;

                            auto &entry = tree->insert(currentPath / name);
                            entry.size = match.captured(2).toULongLong(nullptr, 10);;
                            entry.modTime = Time::fromText(match.captured(3), Time::UnixLsStyleDateTime);
                        }
                    }
                }
            }
            else
            {
                currentPath.clear();
            }
        }
        qDebug() << "idgames file tree contains" << tree->size() << "entries";
        setFileTree(tree.release());
        return String();
    },
    [this] (String const &errorMessage)
    {
        if (!errorMessage)
        {
            wasConnected();
        }
        else
        {
            handleError("Failed to parse directory listing: " + errorMessage);
            wasDisconnected();
        }
});
}

filesys::Link *IdgamesLink::construct(String const &address)
{
    if ((address.startsWith("http:") || address.startsWith("https:")) &&
        !address.contains("dengine.net"))
    {
        return new IdgamesLink(address);
    }
    return nullptr;
}
