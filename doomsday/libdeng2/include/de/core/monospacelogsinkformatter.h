#ifndef LIBDENG2_MONOSPACELOGSINKFORMATTER_H
#define LIBDENG2_MONOSPACELOGSINKFORMATTER_H

#include "../LogSink"

namespace de {

/**
 * Log entry formatter with fixed line length and the assumption of fixed-width
 * fonts. This formatter is for plain text output.
 */
class MonospaceLogSinkFormatter : public LogSink::IFormatter
{
public:
    MonospaceLogSinkFormatter();

    QList<String> logEntryToTextLines(LogEntry const &entry);

private:
    duint _maxLength;
    int _minimumIndent;
    String _sectionOfPreviousLine;
    int _sectionDepthOfPreviousLine;
};

} // namespace de

#endif // LIBDENG2_MONOSPACELOGSINKFORMATTER_H
