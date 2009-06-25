/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_STRING_H
#define LIBDENG2_STRING_H

#include <string>

#include <de/deng.h>
#include <de/Error>
#include <de/IByteArray>

namespace de
{
    /**
     * The String class extends the STL string class with the IByteArray
     * interface.
     */
	class PUBLIC_API String : public std::string, public IByteArray
	{
	public:
	    /// This exception is thrown if an encoding conversion fails.	    
        DEFINE_ERROR(ConversionError);

	    /// This exception is thrown when an error is encountered in 
	    /// string pattern replacements.
        DEFINE_ERROR(IllegalPatternError);

	    /**
	     * Data argument for the pattern formatter.
	     * @see String::patternFormat()
	     */
	    class IPatternArg 
	    {
        public:
            /// This exception is thrown if an incompatible type is requested
            /// in asText() or asNumber().
            DEFINE_ERROR(TypeError);
	        
	    public:
            virtual ~IPatternArg() {}
            
            /// Returns the value of the argument as a text string.
            virtual String asText() const = 0;
            
            /// Returns the value of the argument as a number.
            virtual ddouble asNumber() const = 0;
        };
	    
	public:
		String(const std::string& text = "");
        String(const char* cStr);
		String(const IByteArray& array);
		String(const String& other);

        /// Does a path concatenation on this string and the argument.
        String concatenatePath(const std::string& path) const;

        /// Removes whitespace from the beginning and end of the string.
        /// @return Copy of the string without whitespace.
        String strip() const;

        /// Removes whitespace from the beginning of the string.
        /// @return Copy of the string without whitespace.
        String leftStrip() const;

        /// Removes whitespace from the end of the string.
        /// @return Copy of the string without whitespace.
        String rightStrip() const;

		// The IByteArray interface:
		duint size() const;
		void get(Offset at, Byte* values, duint count) const;
		void set(Offset at, const Byte* values, duint count);	
	
		/// Converts the string to a wide-character STL wstring.
        std::wstring wide() const;
            
    public:
        /// Extracts the base name from the string (includes extension).
        static String fileName(const std::string& path);
        
        /**
         * Extracts the file name extension from a path. A valid extension
         * is the part of a file name after a period where the file name
         * itself is at least one character long. For instance:
         * with "a.ext" the extension is ".ext", but with ".ext" there is
         * no extension. 
         * 
         * @return The extension, including the period in the beginning. 
         * An empty string is returned if the string contains no period.
         */
        static String fileNameExtension(const std::string& path);
        
        /// Extracts the path of the string.
        static String fileNamePath(const std::string& path);

        /// Routine for string to wstring conversions.
        static std::wstring stringToWide(const std::string& str);
        
        /// Routine for wstring to string conversions.
        static String wideToString(const std::wstring& str);
        
        /**
         * Compare two strings (case sensitive).
         *
         * @return 0, if @c a and @c b are identical. Positive, if @c a > @c b.
         *         Negative, if @c a < @c b.
         */
        static dint compareWithCase(const std::string& a, const std::string& b);
        
        /**
         * Compare two strings (case insensitive).
         *
         * @return 0, if @c a and @c b are identical. Positive, if @c a > @c b.
         *         Negative, if @c a < @c b.
         */
        static dint compareWithoutCase(const std::string& a, const std::string& b);

        /**
         * Advances the iterator until a nonspace character is encountered.
         * 
         * @param i  Iterator to advance.
         * @param end  End of the string. Will not advance past this.
         */
        static void skipSpace(std::string::const_iterator& i, const std::string::const_iterator& end);
        
        /**
         * Formats data according to formatting instructions. Outputs a
         * string containing the formatted data.
         *
         * @param formatIter  Will be moved past the formatting instructions.
         *                    After the call, will remain past the end of
         *                    the formatting characters.
         * @param formatEnd   Iterator to the end of the format string.
         * @param arg         Argument to format.
         *
         * @return  Formatted argument as a string.
         */
        static String patternFormat(std::string::const_iterator& formatIter, 
            const std::string::const_iterator& formatEnd, 
            const IPatternArg& arg);
            
        static void advanceFormat(std::string::const_iterator& i, 
            const std::string::const_iterator& end);
	};
}

#endif /* LIBDENG2_STRING_H */
