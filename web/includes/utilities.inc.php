<?php
/**
 * @file utilities.inc.php
 * Miscellaneous utility functions.
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

includeGuard('Utils');

function checkImagePath($path, $formats)
{
    if(!isset($path) || !isset($formats) || !is_array($formats))
        return -1;

    $idx = 0;
    foreach($formats as $fmt)
    {
        $fullPath = $path . '.' . $fmt;

        if(file_exists($fullPath))
            return $idx;
        $idx++;
    }
}

function includeHTML($page, $pluginName=NULL)
{
    $page = trim($page);
    $page = stripslashes($page);

    $filepath = '';
    if(!is_null($pluginName))
    {
        $pluginName = trim($pluginName);
        $pluginName = stripslashes($pluginName);

        $filepath .= DIR_PLUGINS.'/'.$pluginName .'/';
    }
    $filepath .= 'html/' . $page . '.html';

    if(file_exists($filepath))
    {
        readfile($filepath);
    }
    else
    {
        echo "Internal error: Unknown page \"$filepath\"";
    }
}

function fopen_recursive($path, $mode, $chmod=0755)
{
    $matches = NULL;
    preg_match('`^(.+)/([a-zA-Z0-9_]+\.[a-zA-Z0-9_]+)$`i', $path, $matches);

    $directory = $matches[1];
    $file = $matches[2];

    if(!is_dir($directory))
    {
        if(!mkdir($directory, $chmod, 1))
        {
            return false;
        }
    }

    return fopen($path, $mode);
}

/**
 * Case insensitive version of array_key_exists.
 * Returns the matching key on success, else false.
 *
 * @param key  (string)
 * @param search  (array)
 * @return  (bool) false if not found | (string) found key
 */
function array_casekey_exists($key, $search)
{
    if(array_key_exists($key, $search))
    {
        return $key;
    }

    if(!(is_string($key) && is_array($search) && count($search)))
    {
        return false;
    }

    $key = strtolower($key);
    foreach($search as $k => $v)
    {
        if(strtolower($k) == $key)
        {
            return $k;
        }
    }
    return false;
}

function clean_text($text, $length = 0)
{
    $html = html_entity_decode($text, ENT_QUOTES, 'UTF-8');
    $text = strip_tags($html);
    if($length > 0 && strlen($text) > $length)
    {
        $cut_point = strrpos(substr($text, 0, $length), ' ');
        $text = substr($text, 0, $cut_point) . '…';
    }
    $text = htmlentities($text, ENT_QUOTES, 'UTF-8');
    return $text;
}

function safe_url($raw_url)
{
    $url_scheme = parse_url($raw_url, PHP_URL_SCHEME);
    if($url_scheme == 'http' || $url_scheme == 'https')
    {
        return htmlspecialchars($raw_url, ENT_QUOTES, 'UTF-8', false);
    }
    // parse_url failed, or the scheme was not hypertext-based.
    return false;
}
