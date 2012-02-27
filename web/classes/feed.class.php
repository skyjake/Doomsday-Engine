<?php
/**
 * @file feed.class.php
 * Abstract class which encapsulates a content feed behind an object interface.
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

includeGuard('Feed');

require_once(DIR_CLASSES.'/visual.interface.php');
require_once(DIR_EXTERNAL.'/magpierss/rss_fetch.inc');

class Feed implements Visual, Iterator, Countable
{
    public static $name = 'pages';

    private static $displayOptions = 0;

    private $maxItems;
    private $feedUri;
    private $feed;
    private $position;

    private $title = 'Untitled';
    private $labelSpanClass = NULL;

    // Callback to make when generating Html for a feed item.
    private $func_genElementHtml = NULL;
    private $func_genElementHtmlParams = NULL;

    public function __construct($feedURL, $maxItems=5)
    {
        $this->_feedUri = $feedURL;
        $this->_maxItems = intval($maxItems);

        $this->_feed = fetch_rss($this->_feedUri);
        $this->_feedFormat = 'RSS';
        $this->_position = 0;
    }

    public function setTitle($title, $labelSpanClass=NULL)
    {
        $this->_title = "$title";
        if(!is_null($labelSpanClass))
            $this->_labelSpanClass = "$labelSpanClass";
    }

    public function setGenerateElementHtmlCallback($funcName, $params=NULL)
    {
        $funcName = "$funcName";
        if(!is_callable($funcName)) return FALSE;

        $this->func_genElementHtml = $funcName;
        $this->func_genElementHtmlParams = $params;
        return TRUE;
    }

    /**
     * Implements Visual.
     */
    public function displayOptions()
    {
        return $this->_displayOptions;
    }

    private function generateElementHtml(&$item)
    {
        if(!is_array($item))
            throw new Exception('Received invalid item, array expected.');

        if(!is_null($this->func_genElementHtml))
        {
            return call_user_func_array($this->func_genElementHtml, array($item, $this->func_genElementHtmlParams));
        }

        $html = '<a href="'. preg_replace('/(&)/', '&amp;', $item['link'])
               .'" title="'. date("m/d/y", $item['date_timestamp']) .' - '. htmlspecialchars($item['title'])
                       .'">'. htmlspecialchars($item['title']) .'</a>';
        return $html;
    }

    public function generateHtml()
    {
        if(count($this) <= 0) return;

        $feedTitle = "$this->_title via $this->_feedFormat";

?><a href="<?php echo preg_replace('/(&)/', '&amp;', $this->_feedUri); ?>" class="link-rss" title="<?php echo htmlspecialchars($feedTitle); ?>"><span class="hidden"><?php echo htmlspecialchars($this->_feedFormat); ?></span></a><?php

        if(!is_null($this->_labelSpanClass))
        {
?>&nbsp;<span class="<?php echo $this->_labelSpanClass; ?>"><?php
        }


        echo htmlspecialchars($feedTitle);

        if(!is_null($this->_labelSpanClass))
        {
?></span><?php
        }

?><ul><?php

        $n = (integer) 0;
        foreach($this as $item)
        {
            $elementHtml = $this->generateElementHtml($item);

?><li><?php echo $elementHtml; ?></li><?php

            if(++$n >= $this->_maxItems) break;
        }

?></ul><?php
    }

    public function url()
    {
        return $this->_feedUri;
    }

    /**
     * Implements Countable.
     */
    public function count()
    {
        if(is_object($this->_feed) && is_array($this->_feed->items))
            return sizeof($this->_feed->items);
        return 0;
    }

    /**
     * Implements Iterator.
     */
    public function rewind()
    {
        reset($this->_feed->items);
        $this->_position = key($this->_feed->items);
    }

    public function current()
    {
        return $this->_feed->items[$this->_position];
    }

    public function key()
    {
        return $this->_position;
    }

    public function next()
    {
        next($this->_feed->items);
        $this->_position = key($this->_feed->items);
    }

    public function valid()
    {
        return isset($this->_feed->items[$this->_position]);
    }
}
