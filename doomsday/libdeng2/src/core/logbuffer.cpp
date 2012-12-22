/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/LogBuffer"
#include "de/Writer"
#include "de/FixedByteArray"
#include "de/Guard"
#include "de/App"

#include <stdio.h>
#include <QTextStream>
#include <QCoreApplication>
#include <QList>
#include <QTimer>
#include <QDebug>

namespace de {

Time::Delta const FLUSH_INTERVAL = .2; // seconds

namespace internal {

/**
 * @internal
 * Interface for output streams for printing log messages.
 */
class IOutputStream
{
public:
    virtual ~IOutputStream() {}
    virtual void flush() {}
    virtual IOutputStream &operator << (QString const &text) = 0;
};

/// @internal Stream that outputs to a de::File.
class FileOutputStream : public IOutputStream
{
public:
    FileOutputStream(File *f) : _file(f) {}
    void flush() {
        if(_file) _file->flush();
    }
    IOutputStream &operator << (QString const &text) {
        if(_file) *_file << Block(text.toUtf8());
        return *this;
    }
private:
    File *_file;
};

/// @internal Stream that outputs to a QTextStream.
class TextOutputStream : public IOutputStream
{
public:
    TextOutputStream(QTextStream *ts) : _ts(ts) {
        if(_ts) _ts->setCodec("UTF-8");
    }
    ~TextOutputStream() {
        delete _ts;
    }
    void flush() {
        if(_ts) _ts->flush();
    }
    IOutputStream &operator << (QString const &text) {
        if(_ts) (*_ts) << text;
        return *this;
    }
private:
    QTextStream *_ts;
};

/// @internal Stream that outputs to QDebug.
class DebugOutputStream : public IOutputStream
{
public:
    DebugOutputStream(QtMsgType msgType) {
        _qs = new QDebug(msgType);
    }
    ~DebugOutputStream() {
        delete _qs;
    }
    IOutputStream &operator << (QString const &text) {
        _qs->nospace();
        (*_qs) << text.toUtf8().constData();
        _qs->nospace();
        return *this;
    }
private:
    QDebug *_qs;
};

} // namespace internal

struct LogBuffer::Instance
{
    typedef QList<LogEntry *> EntryList;

    dint enabledOverLevel;
    dint maxEntryCount;
    bool standardOutput;
    bool flushingEnabled;
    File *outputFile;
    EntryList entries;
    EntryList toBeFlushed;
    Time lastFlushedAt;
    QTimer *autoFlushTimer;

    /// @todo These belong in the formatter.
    String sectionOfPreviousLine;
    int sectionDepthOfPreviousLine;

    Instance(duint maxEntryCount)
        : enabledOverLevel(LogEntry::MESSAGE),
          maxEntryCount(maxEntryCount),
          standardOutput(true),
          flushingEnabled(true),
          outputFile(0),
          autoFlushTimer(0),
          sectionDepthOfPreviousLine(0)
    {}
};

LogBuffer *LogBuffer::_appBuffer = 0;

LogBuffer::LogBuffer(duint maxEntryCount) 
    : d(new Instance(maxEntryCount))
{
    d->autoFlushTimer = new QTimer(this);
    connect(d->autoFlushTimer, SIGNAL(timeout()), this, SLOT(flush()));

#ifdef WIN32
    d->standardOutput = false;
#endif
}

LogBuffer::~LogBuffer()
{
    DENG2_GUARD(this);

    setOutputFile("");
    clear();

    delete d;
}

void LogBuffer::clear()
{
    DENG2_GUARD(this);

    // Flush first, we don't want to miss any messages.
    flush();

    DENG2_FOR_EACH(Instance::EntryList, i, d->entries)
    {
        delete *i;
    }
    d->entries.clear();
}

dsize LogBuffer::size() const
{
    DENG2_GUARD(this);
    return d->entries.size();
}

void LogBuffer::latestEntries(Entries &entries, int count) const
{
    DENG2_GUARD(this);
    entries.clear();
    for(int i = d->entries.size() - 1; i >= 0; --i)
    {
        entries.append(d->entries[i]);
        if(count && entries.size() >= count)
        {
            return;
        }
    }
}

void LogBuffer::setMaxEntryCount(duint maxEntryCount)
{
    d->maxEntryCount = maxEntryCount;
}

void LogBuffer::add(LogEntry *entry)
{       
    DENG2_GUARD(this);

    // We will not flush the new entry as it likely has not yet been given
    // all its arguments.
    if(d->lastFlushedAt.since() > FLUSH_INTERVAL)
    {
        flush();
    }

    d->entries.push_back(entry);
    d->toBeFlushed.push_back(entry);

    // Should we start autoflush?
    if(!d->autoFlushTimer->isActive() && qApp)
    {
        // Every now and then the buffer will be flushed.
        d->autoFlushTimer->start(FLUSH_INTERVAL * 1000);
    }
}

void LogBuffer::enable(LogEntry::Level overLevel)
{
    d->enabledOverLevel = overLevel;
}

bool LogBuffer::isEnabled(LogEntry::Level overLevel) const
{
    return d->enabledOverLevel <= overLevel;
}

void LogBuffer::enableStandardOutput(bool yes)
{
    d->standardOutput = yes;
}

void LogBuffer::enableFlushing(bool yes)
{
    d->flushingEnabled = yes;
}

void LogBuffer::setOutputFile(String const &path)
{
    flush();

    if(d->outputFile)
    {
        d->outputFile->audienceForDeletion -= this;
        d->outputFile = 0;
    }

    if(!path.isEmpty())
    {
        d->outputFile = &App::rootFolder().replaceFile(path);
        d->outputFile->setMode(File::Write);
        d->outputFile->audienceForDeletion += this;
    }
}

void LogBuffer::flush()
{
    using internal::IOutputStream;
    using internal::FileOutputStream;
    using internal::TextOutputStream;
    using internal::DebugOutputStream;

    if(!d->flushingEnabled) return;

    DENG2_GUARD(this);

    if(!d->toBeFlushed.isEmpty())
    {
        FileOutputStream fs(d->outputFile? d->outputFile : 0);
#ifndef WIN32
        TextOutputStream outs(d->standardOutput? new QTextStream(stdout) : 0);
        TextOutputStream errs(d->standardOutput? new QTextStream(stderr) : 0);
#else
        DebugOutputStream outs(QtDebugMsg);
        DebugOutputStream errs(QtWarningMsg);
#endif
        try
        {
            /**
             * @todo This is a hard-coded line formatter with fixed line length
             * and the assumption of fixed-width fonts. In the future there
             * should be multiple formatters that can be plugged into
             * LogBuffer, and this one should be used only for debug output and
             * the log text file (doomsday.out).
             */
#ifdef _DEBUG
            // Debug builds include a timestamp and msg type indicator.
            duint const MAX_LENGTH = 110;
#else
            duint const MAX_LENGTH = 89;
#endif

            DENG2_FOR_EACH(Instance::EntryList, i, d->toBeFlushed)
            {
                // Error messages will go to stderr instead of stdout.
                QList<IOutputStream *> os;
                os << ((*i)->level() >= LogEntry::WARNING? &errs : &outs) << &fs;

                String const &section = (*i)->section();

                int cutSection = 0;
#ifndef DENG2_DEBUG
                // In a release build we can dispense with the metadata.
                LogEntry::Flags entryFlags = LogEntry::Simple;
#else
                LogEntry::Flags entryFlags;
#endif
                // Compare the current entry's section with the previous one
                // and if there is an opportunity to omit or abbreviate.
                if(!d->sectionOfPreviousLine.isEmpty()
                        && (*i)->sectionDepth() >= 1
                        && d->sectionDepthOfPreviousLine <= (*i)->sectionDepth())
                {
                    if(d->sectionOfPreviousLine == section)
                    {
                        // Previous section is exactly the same, omit completely.
                        entryFlags |= LogEntry::SectionSameAsBefore;
                    }
                    else if(section.startsWith(d->sectionOfPreviousLine))
                    {
                        // Previous section is partially the same, omit the common beginning.
                        cutSection = d->sectionOfPreviousLine.size();
                        entryFlags |= LogEntry::SectionSameAsBefore;
                    }
                    else
                    {
                        int prefix = section.commonPrefixLength(d->sectionOfPreviousLine);
                        if(prefix > 5)
                        {
                            // Some commonality with previous section, we can abbreviate
                            // those parts of the section.
                            entryFlags |= LogEntry::AbbreviateSection;
                            cutSection = prefix;
                        }
                    }
                }
                String message = (*i)->asText(entryFlags, cutSection);

                // Remember for the next line.
                d->sectionOfPreviousLine      = section;
                d->sectionDepthOfPreviousLine = (*i)->sectionDepth();

                // The wrap indentation will be determined dynamically based on the content
                // of the line.
                int wrapIndent = -1;
                int nextWrapIndent = -1;

                // Print line by line.
                String::size_type pos = 0;
                while(pos != String::npos)
                {
#ifdef DENG2_DEBUG
                    int const minimumIndent = 25;
#else
                    int const minimumIndent = 0;
#endif

                    // Find the length of the current line.
                    String::size_type next = message.indexOf('\n', pos);
                    duint lineLen = (next == String::npos? message.size() - pos : next - pos);
                    duint const maxLen = (pos > 0? MAX_LENGTH - wrapIndent : MAX_LENGTH);
                    if(lineLen > maxLen)
                    {
                        // Wrap overly long lines.
                        next = pos + maxLen;
                        lineLen = maxLen;

                        // Maybe there's whitespace we can wrap at.
                        int checkPos = pos + maxLen;
                        while(checkPos > pos)
                        {
                            /// @todo remove isPunct() and just check for the breaking chars
                            if(message[checkPos].isSpace() ||
                                    (message[checkPos].isPunct() && message[checkPos] != '.' &&
                                     message[checkPos] != ','    && message[checkPos] != '-' &&
                                     message[checkPos] != '\''   && message[checkPos] != '"' &&
                                     message[checkPos] != '('    && message[checkPos] != ')' &&
                                     message[checkPos] != '['    && message[checkPos] != ']' &&
                                     message[checkPos] != '_'))
                            {
                                if(!message[checkPos].isSpace())
                                {
                                    // Include the punctuation on this line.
                                    checkPos++;
                                }

                                // Break here.
                                next = checkPos;
                                lineLen = checkPos - pos;
                                break;
                            }
                            checkPos--;
                        }
                    }

                    // Crop this line's text out of the entire message.
                    String lineText = message.substr(pos, lineLen);

                    // For lines other than the first one, print an indentation.
                    if(pos > 0)
                    {
                        lineText = QString(wrapIndent, QChar(' ')) + lineText;
                    }

                    // The wrap indent for this paragraph depends on the first line's content.
                    if(nextWrapIndent < 0)
                    {
                        int w = minimumIndent;
                        int firstNonSpace = -1;
                        for(; w < lineText.size(); ++w)
                        {
                            if(firstNonSpace < 0 && !lineText[w].isSpace())
                                firstNonSpace = w;

                            // Indent to colons automatically (but not too deeply).
                            if(lineText[w] == ':' && w < lineText.size() - 1 && lineText[w + 1].isSpace())
                                firstNonSpace = (w < int(MAX_LENGTH)*2/3? -1 : minimumIndent);
                        }

                        nextWrapIndent = qMax(minimumIndent, firstNonSpace);
                    }

                    // Check for formatting symbols.
                    lineText.replace("$R", String(maxLen - minimumIndent, '-'));

                    foreach(IOutputStream *stream, os)
                    {
                        if(stream) *stream << lineText;
                    }

                    // Advance to the next line.
                    wrapIndent = nextWrapIndent;
                    pos = next;
                    if(pos != String::npos && message[pos].isSpace())
                    {
                        // At a forced newline, reset the wrap indentation.
                        if(message[pos] == '\n')
                        {
                            nextWrapIndent = -1;
                            wrapIndent = minimumIndent;
                        }
                        pos++; // Skip whitespace.
                    }

                    foreach(IOutputStream *stream, os)
                    {
                        if(stream) *stream << "\n";
                    }
                }
            }
        }
        catch(de::Error const &error)
        {
            QList<IOutputStream *> os;
            os << &errs << &fs;
            foreach(IOutputStream *stream, os)
            {
                if(stream) *stream << "Exception during log flush:\n" << error.what() << "\n";
            }
        }

        d->toBeFlushed.clear();

        // Make sure it really gets written now.
        fs.flush();
    }

    d->lastFlushedAt = Time();

    // Too many entries? Now they can be destroyed since we have flushed everything.
    while(d->entries.size() > d->maxEntryCount)
    {
        LogEntry *old = d->entries.front();
        d->entries.pop_front();
        delete old;
    }
}

void LogBuffer::fileBeingDeleted(File const &file)
{
    DENG2_ASSERT(d->outputFile == &file);
    DENG2_UNUSED(file);
    flush();
    d->outputFile = 0;
}

void LogBuffer::setAppBuffer(LogBuffer &appBuffer)
{
    _appBuffer = &appBuffer;
}

LogBuffer &LogBuffer::appBuffer()
{
    DENG2_ASSERT(_appBuffer != 0);
    return *_appBuffer;
}

} // namespace de
