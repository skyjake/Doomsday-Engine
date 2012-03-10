<?php
/**
 * @file addonsparser.class.php
 * Parses an addon list XML file.
 *
 * Constructs a collection of Addons by parsing an XML addon list.
 *
 * @section License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * @author Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
 */

includeGuard('AddonsParser');

require_once('baseaddon.class.php');

class AddonsParser
{
    public static function parse($xmlAddonList, &$addons)
    {
        if(!is_array($addons))
            throw new Exception('Invalid addons argument, array expected');

        $listDom = self::constructSimpleXmlElementTree($xmlAddonList);
        if($listDom == FALSE)
            throw new Exception('Failed constructing XML DOM');

        if(!self::parseAddonListDOM($listDom, $addons))
            throw new Exception('Failed parsing XML DOM');

        return TRUE;
    }

    /**
     * Transform the raw XML data into a SimpleXMLElement node tree.
     *
     * @param $raw_XML  XML data to be parsed (scheme independent).
     * @return  (Object) Root element of a SimpleXMLElement tree if successfull
     *     (Boolean) @c FALSE otherwise.
     */
    private static function constructSimpleXmlElementTree($xmlData)
    {
        libxml_use_internal_errors(TRUE);
        try
        {
            $xmlTree = new SimpleXMLElement($xmlData);
        }
        catch(Exception $e)
        {
            $errorMsg = '';
            foreach(libxml_get_errors() as $libXmlError)
            {
                $errorMsg .= "\t" . $libXmlError->message;
            }
            trigger_error($errorMsg);
            return FALSE;
        }
        return $xmlTree;
    }

    /**
     * Parse the specified addon list, constructing from it a collection
     * of Addon objects representing the addons.
     *
     * @param list  (Object) Root element of a SimpleXMLElement DOM
     * @param addons  (Array) Addons collection to be populated.
     * @return  (Boolean) @c TRUE iff parse completed successfully.
     */
    private static function parseAddonListDOM(&$list, &$addons)
    {
        if(!($list instanceof SimpleXMLElement))
            throw new Exception('Received invalid addon list');

        // For each addon.
        foreach($list->children() as $list_addon)
        {
            try
            {
                $addon = self::parseAddon($list_addon);

                foreach($list_addon->children() as $child)
                {
                    switch($child->getName())
                    {
                    case 'games':
                        try
                        {
                            $games = self::parseGames($child);
                            $addon->addGames($games);
                        }
                        catch(Exception $e)
                        {
                            /// \todo Log exception.
                        }
                        break;

                    default: break;
                    }
                }

                // Add this new Addon to the collection.
                $addons[] = $addon;
            }
            catch(Exception $e)
            {
                /// \todo Log exception.
            }
        }

        return TRUE;
    }

    /**
     * Parse the list of games from a SimpleXMLElement.
     *
     * @param $list_game  (Object) SimpleXMLElement node to be "parsed".
     * @return  (Array) Resultant GameList record.
     */
    private static function parseGames(&$list_games)
    {
        if(!($list_games instanceof SimpleXMLElement))
            throw new Exception('Received invalid list_games');

        $games = array();
        foreach($list_games->children() as $child)
        {
            if($child->getName() !== 'game') continue;

            $name = strtolower(clean_text((string)$child));
            $games[$name] = (boolean)TRUE;
        }

        return $games;
    }

    /**
     * Parse a new Addon from a SimpleXMLElement.
     *
     * @param $list_game  (Object) SimpleXMLElement node to be "parsed".
     * @return  (object) Resultant Addon object.
     */
    private static function parseAddon(&$list_addon)
    {
        if(!($list_addon instanceof SimpleXMLElement))
            throw new Exception('Received invalid list_addon');

        $title = clean_text($list_addon->title);
        $version = !empty($list_addon->version)? clean_text($list_addon->version) : NULL;
        $downloadUri = !empty($list_addon->downloadUri)? safe_url($list_addon->downloadUri) : NULL;
        $homepageUri = !empty($list_addon->homepageUri)? safe_url($list_addon->homepageUri) : NULL;

        // Construct and configure a new Addon instance.
        $addon = new Addon($title, $version, $downloadUri, $homepageUri);

        foreach($list_addon->attributes() as $key => $value)
        {
            $attrib = clean_text($key);
            $value = clean_text($value);
            $addon->setAttrib($attrib, (integer)eval('return ('.$value.');'));
        }

        if(!empty($list_addon->description))
            $addon->setDescription(clean_text($list_addon->description));

        if(!empty($list_addon->notes))
            $addon->setNotes(clean_text($list_addon->notes));

        return $addon;
    }
}
