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

    private static $_displayOptions = 0;

    private $_maxItems;
    private $_feedURL;
    private $_rss;
    private $_position;

    public function __construct($feedURL, $maxItems=5)
    {
        $this->_feedURL = $feedURL;
        $this->_maxItems = intval($maxItems);

        $this->_rss = fetch_rss($this->_feedURL);
        $this->_position = 0;
    }

    /**
     * Implements Visual.
     */
    public function displayOptions()
    {
        return $this->_displayOptions;
    }

    public function generateHTML()
    {
        if(count($this) > 0)
        {
?>
<a href="<?php echo preg_replace('/(&)/', '&amp;', $this->_feedURL); ?>" class="link-rss" title="Project News, via RSS"><span class="hidden">RSS</span></a>&nbsp;<span id="projectnews-label">Latest Project News </span>
<ul>
<?php

            $n = (integer) 0;
            foreach($this as $item)
            {
?>
<li><a href="<?php echo preg_replace('/(&)/', '&amp;', $item['link']); ?>" title="<?php echo date("m/d/y", $item['date_timestamp']); ?> - <?php echo $item['title']; ?>"><?php echo $item['title']; ?></a></li>
<?php
                if(++$n >= $this->_maxItems)
                    break;
            }
        }
?>
</ul>
<?php
    }

    public function url()
    {
        return $this->_feedURL;
    }

    /**
     * Implements Countable.
     */
    public function count()
    {
        if(is_object($this->_rss) && is_array($this->_rss->items))
            return sizeof($this->_rss->items);
        return 0;
    }

    /**
     * Implements Iterator.
     */
    public function rewind()
    {
        reset($this->_rss->items);
        $this->_position = key($this->_rss->items);
    }

    public function current()
    {
        return $this->_rss->items[$this->_position];
    }

    public function key()
    {
        return $this->_position;
    }

    public function next()
    {
        next($this->_rss->items);
        $this->_position = key($this->_rss->items);
    }

    public function valid()
    {
        return isset($this->_rss->items[$this->_position]);
    }
}
