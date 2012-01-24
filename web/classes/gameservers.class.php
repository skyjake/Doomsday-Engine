<?php
/**
 * @file gameservers.class.php
 * GameServer objects and Servers object collection.
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

includeGuard('GameServers');

/**
 * @defgroup gameTypeFlags Game Type Flags
 * @ingroup masterserver
 */
///{
define('GT_COOP', 0);
define('GT_DM', 1);
///}

class GameServerRecord
{
    private $_name = 'Empty Record';
    private $_version = 'NULL';
    private $_numPlayers = 0;
    private $_maxPlayers = 0;
    private $_gameType = GT_COOP;
    private $_gameRules = 'none';

    public function __construct($name, $version, $numPlayers, $maxPlayers, $gameRules)
    {
        $this->_name = trim($name);
        $this->_version = trim($version);
        $this->_numPlayers = intval($numPlayers);
        $this->_maxPlayers = intval($maxPlayers);
        $this->_gameRules = trim($gameRules);

        $rules = explode(' ', $this->_gameRules);
        if($rules[1] == "coop")
            $this->_gameType = GT_COOP;
        else
            $this->_gameType = GT_DM;
    }

    public function name()
    {
        return $this->_name;
    }

    public function version()
    {
        return $this->_version;
    }

    public function numPlayers()
    {
        return $this->_numPlayers;
    }

    public function maxPlayers()
    {
        return $this->_maxPlayers;
    }

    public function gameType()
    {
        return $this->_gameType;
    }

    public function gameRules()
    {
        return $this->_gameRules;
    }
}

class GameServers implements Visual, Iterator, Countable
{
    private $_displayOptions = 0;
    private $_servers;
    private $_url;
    private $_position = 0;

    public function __construct($url)
    {
        $properties = array('name', 'ver', 'setup', 'nump', 'maxp');

        $this->_url = $url;
        $this->_servers = array();

        try
        {
            if(!($fh = fopen($this->_url, 'r')))
                throw new Exception(sprintf('Failed opening %s.', $this->_url));

            $server = array();
            while(!feof($fh))
            {
                $line = fgets($fh);

                $prop = explode(':', $line);

                foreach($properties as $p)
                {
                    if(!strcmp($p, $prop[0]))
                    {
                        $server[$prop[0]] = $prop[1];

                        if(isset($server['name']) &&
                           isset($server['ver']) &&
                           isset($server['setup']) &&
                           isset($server['nump']) &&
                           isset($server['maxp']))
                        {
                            $this->_servers[] = new GameServerRecord(
                                $server['name'], $server['ver'], $server['nump'],
                                $server['maxp'], $server['setup']);

                            $server = array();
                        }

                        break;
                    }
                }
            }

            fclose($fh);
        }
        catch(Exception $e)
        {
            throw new Exception('GameServers: Parse failed.');
        }
    }

    /**
     * Implements Visual.
     */
    public function displayOptions()
    {
        return $this->_displayOptions();
    }

    public function generateHTML()
    {
        $gameTypeIcons = array('cp2.png', 'dm2.png');

        if($this->count())
        {
?>
<span id="servers-label">Servers</span><br />
<ul class ="list">
<?php

            foreach($this as $elm => $server)
            {
                $gameTypeIcon = DIR_IMAGES.'/'.$gameTypeIcons[$server->gameType()];
?>
<li><span class="server" title="<?php echo htmlspecialchars($server->name()); ?>"><?php echo htmlspecialchars($server->name()); ?> (<?php echo $server->numPlayers(); ?> / <?php echo $server->maxPlayers(); ?>) <img src="<?php echo $gameTypeIcon; ?>" /></span></li>
<?php
            }
?>
</ul>
<?php
        }
    }

    /**
     * Implements Countable.
     */
    public function count()
    {
        return sizeof($this->_servers);
    }

    /**
     * Implements Iterator.
     */
    public function rewind()
    {
        $this->_position = key($this->_servers);
    }

    public function current()
    {
        return $this->_servers[$this->_position];
    }

    public function key()
    {
        return $this->_position;
    }

    public function next()
    {
        next($this->_servers);
        $this->_position = key($this->_servers);
    }

    public function valid()
    {
        return isset($this->_servers[$this->_position]);
    }
}
