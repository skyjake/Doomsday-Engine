<?php
/** @file feed.class.php Abstract feed.
 *
 * Encapsulates a content feed with object API and interface semantics.
 *
 * @authors Copyright Â© 2009-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

includeGuard('Feed');

require_once(DIR_EXTERNAL.'/magpierss/rss_fetch.inc');

class FeedItem
{
    public $author;
    public $timestamp; // unix
    public $title;
    public $link;
    public $description;
}

class Feed implements Iterator, Countable
{
    private $feedFormat;
    private $feedUri;

    private $items;
    private $position;

    public function __construct($uri, $format = 'RSS')
    {
        $this->feedUri    = (string)$uri;
        $this->feedFormat = $format;

        $this->items = array();
        $this->position = 0;

        $feed = fetch_rss($this->feedUri);
        if(!$feed || $feed->ERROR)
            return;

        foreach($feed->items as &$rawItem)
        {
            $newItem = new FeedItem();
            $newItem->author      = (string)$rawItem['author'];
            $newItem->timestamp   = $rawItem['date_timestamp'];
            $newItem->title       = (string)$rawItem['title'];
            $newItem->link        = (string)$rawItem['link'];
            $newItem->description = (string)$rawItem['description'];

            $this->items[] = $newItem;
        }
    }

    public function uri()
    {
        return $this->feedUri;
    }

    public function format()
    {
        return $this->feedFormat;
    }

    /**
     * Implements Countable.
     */
    public function count()
    {
        return count($this->items);
    }

    /**
     * Implements Iterator.
     */
    public function rewind()
    {
        reset($this->items);
        $this->position = key($this->items);
    }

    public function current()
    {
        return $this->items[$this->position];
    }

    public function key()
    {
        return $this->position;
    }

    public function next()
    {
        next($this->items);
        $this->position = key($this->items);
    }

    public function valid()
    {
        return isset($this->items[$this->position]);
    }
}
