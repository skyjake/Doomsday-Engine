/** @file font.h  Font with metrics.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_FONT_H
#define LIBGUI_FONT_H

#include <de/libcore.h>
#include <de/Rule>
#include <de/Rectangle>
#include <de/String>
#include <de/Vector>
#include <de/Range>
#include <de/EscapeParser>

#include <QFont>
#include <QImage>
#include <QList>

#include "../gui/libgui.h"

namespace de {

class NativeFont;

/**
 * Font with metrics.
 */
class LIBGUI_PUBLIC Font
{
public:   
    typedef QVector<int> TabStops;

    /**
     * Rich formatting instructions for a string of plain text.
     *
     * The formatting instructions are composed of a sequence of ranges that
     * specify various modifications to the original font. It is important to
     * note that a RichFormat instance always needs to be set up for a specific
     * source text string. Also, RichFormat is out-of-band data: when operating
     * on a piece of rich text (using the methods of Font), the formatting is
     * always provided in parallel to the plain version of the text.
     *
     * Use RichFormat::fromPlainText() to set up a RichFormat for a string of
     * plain text.
     *
     * Use RichFormat::initFromStyledText() to set up a RichFormat for a string
     * of text that contains style information (as escape sequences that start
     * with the Esc ASCII code 0x1b).
     *
     * @par Indentation and tab stops
     *
     * The escape sequences ">" and "T" can be used to define indentation
     * points and tab stops for visual alignment of text.
     *
     * The indentation mark ">" determines the width at which the following
     * lines are left-indented. It does not affect the line the mark itself is
     * on. Newlines (\n) do not reset the indentation. If multiple marks are
     * used in the same text content, each mark is applied cumulatively on the
     * previous indentation. Also note that placing an indentation mark at the
     * beginning of a line or immediately after a new line does nothing (a
     * width of zero does not increase the indentation).
     *
     * Tab stops are used to divide text on a line into smaller segments that
     * can then be freely moved when drawn. All segments with the same tab stop
     * number are aligned which the same stops at other lines of the content,
     * providing there is enough horizontal space available. If tab stop
     * alignment runs out of space, width of the rightmost segments is shrunk
     * slightly to fit.
     *
     * The escape sequence "Ta" sets the tab stop to zero for the following
     * text. The default tab stop is zero. "Tb" sets the tab stop to 1, "Tc" to
     * 2, etc.
     */
    class LIBGUI_PUBLIC RichFormat
    {
    public:
        enum ContentStyle {
            NormalStyle    = 0,
            MajorStyle     = 1,
            MinorStyle     = 2,
            MetaStyle      = 3,
            MajorMetaStyle = 4,
            MinorMetaStyle = 5,
            AuxMetaStyle   = 6
        };
        enum Weight {
            OriginalWeight = -1,
            Normal         = 0,
            Light          = 1,
            Bold           = 2
        };
        enum Style {
            OriginalStyle  = -1,
            Regular        = 0,
            Italic         = 1,
            Monospace      = 2
        };
        enum Color {
            OriginalColor  = -1,
            NormalColor    = 0,
            HighlightColor = 1,
            DimmedColor    = 2,
            AccentColor    = 3,
            DimAccentColor = 4,
            AltAccentColor = 5
        };

        /**
         * Interface for an object providing style information: fonts and
         * colors.
         */
        class LIBGUI_PUBLIC IStyle
        {
        public:
            typedef Vector4ub Color;

            virtual ~IStyle() {}

            /**
             * Returns a color from the style's palette.
             * @param index  Color index (see RichFormat::Color).
             * @return Color values (RGBA 0...255).
             */
            virtual Color richStyleColor(int index) const = 0;

            virtual void richStyleFormat(int contentStyle, float &sizeFactor, Weight &fontWeight,
                                         Style &fontStyle, int &colorIndex) const = 0;

            /**
             * Returns a font to be used with a particular style.
             * @param fontStyle  Style.
             * @return @c NULL to use the default font.
             */
            virtual Font const *richStyleFont(Style fontStyle) const {
                DENG2_UNUSED(fontStyle);
                return NULL; // Use default.
            }
        };

        /**
         * Reference to a (portion) of an existing RichFormat instance.
         */
        class LIBGUI_PUBLIC Ref
        {
        public:
            Ref(Ref const &ref);
            Ref(Ref const &ref, Rangei const &subSpan);
            Ref(RichFormat const &richFormat);
            Ref(RichFormat const &richFormat, Rangei const &subSpan);

            Ref subRef(Rangei const &subSpan) const;

            /// Returns the original referred RichFormat instance.
            RichFormat const &format() const;

            int rangeCount() const;
            Rangei range(int index) const;
            Rangei rangeIndices() const { return _indices; }

        private:
            void updateIndices();

            RichFormat const &_ref;
            Rangei _span;
            Rangei _indices; ///< Applicable indices in the referred format's ranges list.
        };

    public:
        RichFormat();
        RichFormat(IStyle const &style);
        RichFormat(RichFormat const &other);

        RichFormat &operator = (RichFormat const &other);

        void clear();

        bool hasStyle() const;
        void setStyle(IStyle const &style);
        IStyle const &style() const;

        /**
         * Constructs a RichFormat that specifies no formatting instructions.
         *
         * @param plainText  Plain text.
         *
         * @return RichFormat instance with a single range that uses the
         * default formatting.
         */
        static RichFormat fromPlainText(String const &plainText);

        /**
         * Initializes this RichFormat instance with the styles found in the
         * provided styled text (using escape sequences).
         *
         * @param styledText  Text with style markup.
         *
         * @return Corresponding plain text for use with the methods of Font.
         */
        String initFromStyledText(String const &styledText);

        /**
         * Clips this RichFormat so that it covers only the specified range.
         * The indices are translated to be relative to the provided range.
         *
         * @param range  Target range for clipping.
         *
         * @return RichFormat with only those ranges covering @a range.
         */
        Ref subRange(Rangei const &range) const;

        TabStops const &tabStops() const;

        int tabStopXWidth(int stop) const;

        /**
         * Iterates the rich format ranges of a RichFormat.
         *
         * @note Iterator::next() must be called at least once after
         * constructing the instance to move the iterator onto the first range.
         */
        struct LIBGUI_PUBLIC Iterator
        {
            Ref format;
            int index;

        public:
            Iterator(Ref const &ref);

            int size() const;
            bool hasNext() const;
            void next();

            /// Determines if all the style parameters are the same as the default ones.
            bool isDefault() const;

            Rangei range() const;
            float sizeFactor() const;
            Weight weight() const;
            Style style() const;
            int colorIndex() const;
            IStyle::Color color() const;
            bool markIndent() const;
            bool resetIndent() const;
            int tabStop() const;
            bool isTabless() const; ///< Tabstop < 0.
        };

    private:
        DENG2_PRIVATE(d)
    };

    typedef RichFormat::Ref RichFormatRef;

public:
    Font();

    Font(Font const &other);
    //Font(NativeFont const &font);
    Font(QFont const &font);

    //QFont toQFont() const;

    /**
     * Determines the size of the given line of text, i.e., how large an area
     * is covered by the glyphs. (0,0) is at the baseline, left edge of the
     * line. The rectangle may extend into negative coordinates.
     *
     * @param textLine  Text to measure.
     *
     * @return Rectangle covered by the text.
     */
    Rectanglei measure(String const &textLine) const;

    /**
     * Determines the size of the given line of rich text, i.e., how large an
     * area is covered by the glyphs. (0,0) is at the baseline, left edge of
     * the line. The rectangle may extend into negative coordinates.
     *
     * @param textLine     Text to measure.
     * @param format       Rich formatting for @a textLine.
     *
     * @return Rectangle covered by the text.
     */
    Rectanglei measure(String const &textLine, RichFormatRef const &format) const;

    /**
     * Returns the advance width of a line of text. This may not be the same as
     * the width of the rectangle returned by measure().
     *
     * @param textLine  Text to measure.
     *
     * @return Width of the line of text (including non-visible parts like
     * whitespace).
     */
    int advanceWidth(String const &textLine) const;

    int advanceWidth(String const &textLine, RichFormatRef const &format) const;

    /**
     * Rasterizes a line of text onto a 32-bit RGBA image.
     *
     * @param textLine    Text to rasterize.
     * @param foreground  Text foreground color.
     * @param background  Background color.
     *
     * @return Image containing the rasterized text.
     */
    QImage rasterize(String const &textLine,
                     Vector4ub const &foreground = Vector4ub(255, 255, 255, 255),
                     Vector4ub const &background = Vector4ub(255, 255, 255, 0)) const;

    QImage rasterize(String const &textLine,
                     RichFormatRef const &format,
                     Vector4ub const &foreground = Vector4ub(255, 255, 255, 255),
                     Vector4ub const &background = Vector4ub(255, 255, 255, 0)) const;

    Rule const &height() const;
    Rule const &ascent() const;
    Rule const &descent() const;
    Rule const &lineSpacing() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_FONT_H
