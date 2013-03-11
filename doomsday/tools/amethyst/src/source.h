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

#ifndef __AMETHYST_SOURCE_H__
#define __AMETHYST_SOURCE_H__

#include "linkable.h"
#include "string.h"

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QStringList>

/**
 * Source file parser. Extracts tokens out of the source character stream.
 */
class Source : public Linkable
{
public:
    Source();
    Source(QTextStream& inputStream);
    Source(String fileName);
    ~Source();

    Source *next() { return (Source*) Linkable::next(); }
    Source *prev() { return (Source*) Linkable::prev(); }

    bool isOpen();
    const String& fileName() { return _fileName; }
    int lineNumber() { return _lineNumber; }

    void setFileName(const String& name) { _fileName = name; }

    QChar peek();
    void ignore();
    QChar get();

    void skipToMatching();
    bool getToken(String &token);
    bool getTokenOrBlank(String &token);
    void mustGetToken(String &token);
    void pushToken(const String& token);

protected:
    void nextChar() ;

private:
    QFile _file;
    QTextStream* _is;
    bool _mustDeleteStream;
    String _fileName;
    int _lineNumber;
    QChar _peekedChar;
    QStringList _pushedTokens;
};

#endif
