# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2005
#   Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not: http://www.opensource.org/

## @file language.py Localisation

import os, sys, paths, cfparser, events

# This dictionary holds all the known languages.  It maps language
# identifiers to dictionaries of strings.
library = {}

# This dictionary holds all the strings of the current language.  The
# strings may originate from multiple .lang files.
localText = None


def _newString(language, id, text):
    """Insert a new text string into the dictionary.

    @param language The identifier of the language.

    @param id String identifier.

    @param text The translated text.
    """
    #if sys.platform == 'win32':
    #    ## @todo Check this later... Apparently not supported?
    #    text = text.replace('\n', ' ')

    if not library.has_key(language):
        # Start a new language.
        library[language] = {}
        
    library[language][id] = text


def processLanguageBlock(block, prefix=None):
    """Process a block of language strings.  The block must contain
    key elements.

    @param block A cfparser.Block object that contains the language
    strings as cfparser.KeyElement objects.

    @param prefix Prefix to add to the beginning of the string ids.
    """
    for e in block.getContents():
        if e.isKey():
            # The 'category-' strings are NOT affected by the local
            # prefix because they are part of the global information.
            # What a kludge...
            if prefix and e.getName()[:9] != 'category-':
                ident = prefix + '-' + e.getName()
            else:
                ident = e.getName()

            _newString(block.getName(), ident, e.getValue())

            # Define language identifiers always in english, too.
            if ident[:9] == 'language-':
                _newString('english', ident, e.getValue())


def load(fileName):
    """Loads a language file and stores the strings into the database.
    @param fileName Name of the language file.
    """
    p = cfparser.FileParser(fileName)

    # Construct a block element to hold all the strings.
    languageName = paths.getBase(fileName)
    impliedBlock = cfparser.BlockElement('language', languageName)

    try:
        while True:
            # Get the next string from the language file.
            e = p.get()
            
            # Only key elements are recognized.
            if e.isKey():
                impliedBlock.add(e)
    
    except cfparser.OutOfElements:
        # All OK.
        pass

    except:
        import logger
        logger.add(logger.MEDIUM, 'warning-read-language', languageName, 
                   fileName)

    processLanguageBlock(impliedBlock)


def loadAll():
    """Load the <tt>.lang</tt> files from the lang directories.
    """
    for fileName in paths.listFiles(paths.LANG, False):
        # We'll load all the files with the .lang extension.
        if paths.hasExtension('lang', fileName):
            # Load this language file.
            load(fileName)


def loadPath(path):
    """Land all the <tt>.lang</tt> files from the specified directory.

    @param path Directory to read from.
    """
    if not os.path.exists(path):
        return

    for name in os.listdir(path):
        if paths.hasExtension('lang', name):
            load(os.path.join(path, name))


def translate(id, notDefined=None):
    """Look up a translated string from the language database.

    @param id String id.  If this is None or an empty string, an empty
    string is returned.

    @param notDefined The string that is returned if the identifier is
    not defined in the current language or in the default language.
    If None, undefined identifiers are returned untranslated, in
    parentheses.
    """
    # Unspecified IDs are translated to an empty string.
    if not id:
        return ''
    
    try:
        return localText[id]

    except:
        # If the selected language isn't the default (english), see if
        # there is a translation in it.
        if localText != library['english']:
            english = library['english']
            if english.has_key(id):
                return english[id]

        # Return a placeholder.
        if notDefined != None:
            return notDefined
        else:
            return '(' + id + ')'


def define(language, id, text):
    """Define a translation for an identifier.

    @param id String id.

    @param text The translated text.
    """
    _newString(language, id, text)


def isDefined(id):
    """Checks if the string identifier will return a meaningful
    translation.

    @param id String id.
    """
    if localText.has_key(id) or library['english'].has_key(id):
        return True
    
    return False
    

def select(language):
    """Select the language to use for translations.

    @param language The identifier of the language.
    """
    global localText
    localText = library[language]


def change(language):
    """Change the language at runtime.

    @param language Identifier of the new language.
    """
    try:
        if language:
            actual = language
        else:
            actual = 'language-english'

        # Select the new language.
        select(actual[9:])

        # Send a notification so others may update their content.
        events.send(events.LanguageNotify(actual))
        
    except KeyError:
        # Language doesn't exist?
        pass
    

def expand(text, *args):
    """Place the arguments into the text message.

    @param text The text where replacements are done.  A placeholder
    is marked by a percent character and a number (e.g. <tt>%2</tt>).

    @return The expanded version of the string. 
    """
    for i in range(len(args)):
        text = text.replace('%' + str(i + 1), args[i])
    return text.replace('%%', '%')


def getLanguages():
    langs = []
    for fileName in paths.listFiles(paths.LANG):
        # We'll load all the files with the .lang extension.
        if paths.hasExtension('lang', fileName):
            langs.append(paths.getBase(fileName))

    return langs                    


#
# Module Initialization
#

# Strings from the default English language are used for everything by
# default.
loadAll()
select('english')
