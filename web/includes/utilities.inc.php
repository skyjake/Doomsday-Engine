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
    preg_match('`^(.+)/([a-zA-Z0-9_-]+\.[a-zA-Z0-9_-]+)$`i', $path, $matches);

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

function file_get_contents_utf8($fn, $contentType, $charset)
{
    $opts = array(
        'http' => array(
            'method'=>"GET",
            'header'=>"Content-Type:$contentType; charset=$charset"
        )
    );

    $context = stream_context_create($opts);
    $result = @file_get_contents($fn,false,$context);
    return $result;
}

function implode_keys($glue="", $pieces=array())
{
    $keys = array_keys($pieces);
    return implode($glue, $keys);
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
    $text = html_entity_decode($text, ENT_QUOTES, 'UTF-8');
    if($length > 0 && strlen($text) > $length)
    {
        $cut_point = strrpos(substr($text, 0, $length), ' ');
        $text = substr($text, 0, $cut_point) . '…';
    }
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

/**
 * Given an associative array generate a textual representation
 * using JSON markup.
 *
 * @note json_encode is practically useless for real world data
 *       in PHP <= 5.3 hence this hand-rolled imitation.
 *
 * @param array  (Array) Array to be interpreted.
 * @param flags  (Integer) Reserved for future use.
 * @param indent_level  (Integer) Level of indent to add to the output.
 * @return  (Mixed) Textual representation in JSON format, else Boolean @c FALSE
 */
function json_encode_clean(&$array, $flags=0, $indent_level=0)
{
    if(!is_array($array)) return FALSE;

    $staged = NULL;
    foreach($array as $key => $value)
    {
        $key = '"'. addslashes($key) .'"';

        // Format the value:
        if(is_array($value))
        {
            // Descend to the subobject.
            $value = json_encode_clean($value, $flags, $indent_level+1);
        }
        else if(is_bool($value))
        {
            $value = ((boolean)$value)? 'true' : 'false';
        }
        else if(!is_numeric($value) || is_string($value))
        {
            $value = '"'. addslashes($value) .'"';
        }

        // Time to construct the staging array?
        if(is_null($staged))
        {
            $staged = array();
        }

        $staged[] = "$key: $value";
    }

    if(is_null($staged)) return '';

    // Collapse into JSON comma-delimited form.
    $indent = '    ';
    $glue = ", \n";
    $tmp = '';
    foreach($staged as $item)
    {
        $tmp .= $indent . $item . $glue;
    }
    $result = "{\n" . substr($tmp, 0, -strlen($glue)) . "\n}";

    // Determine scope-level indent depth.
    $indent_level = (integer)$indent_level;
    if($indent_level < 0) $indent_level = 0;

    // Apply a scope-level indent?
    if($indent_level != 0)
    {
        $result = str_repeat($indent, $indent_level) . $result;
    }

    return $result;
}

function generateHyperlinkHTML($uri, $maxLength=40, $cssClass=NULL)
{
    $uri = strval($uri);
    $maxLength = (integer)$maxLength;
    if($maxLength < 0) $maxLength = 0;
    if(!is_null($cssClass))
    {
        $cssClass = strval($cssClass);
    }
    else
    {
        $cssClass = '';
    }

    if($maxLength > 0 && strlen($uri) > $maxLength)
        $shortUri = substr($uri, 0, $maxLength).'...';
    else
        $shortUri = $uri;

    $html = '<a';
    if(strlen($cssClass) > 0)
    {
        $html .= " class={$cssClass}";
    }
    $html .= " href=\"{$uri}\">". htmlspecialchars($shortUri) .'</a>';
    return $html;
}
