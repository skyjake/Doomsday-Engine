<?php
/**
 * @file masterserver.php
 * Master Server implementation version 1.3
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
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
 */

includeGuard('MasterServer');

function get_ident($info)
{
    if(!is_array($info))
        throw new Exception('Invalid info argument, array expected.');

    return $info['at'] . ":" . $info['port'];
}

class MasterServer
{
    /// Version number
    const VERSION_MAJOR = 1;
    const VERSION_MINOR = 3;

    /// File used to store the current state of the server database.
    const DATA_FILE = 'servers.dat';

    public $servers;
    public $lastUpdate;

    private $writable;
    private $file;

    public function __construct($writable=false)
    {
        global $HTTP_SERVER_VARS;
        $this->writable = $writable;
        $this->file = fopen(self::DATA_FILE, $writable? 'r+' : 'r');
        if(!$this->file) die();
        $this->lastUpdate = @filemtime(self::DATA_FILE);
        $this->dbLock();
        $this->load();
        if(!$writable) $this->dbUnlock();
    }

    private function dbLock()
    {
        flock($this->file, $this->writable? 2 : 1);
    }

    private function dbUnlock()
    {
        flock($this->file, 3);
    }

    private function load()
    {
        $now = time();
        $max_age = 130; // 2+ mins.
        $this->servers = array();

        while(!feof($this->file))
        {
            $line = trim(fgets($this->file, 4096));

            if($line == "--")
            {
                // Expired announcements are ignored.
                if($now - $info['time'] < $max_age && count($info) >= 3)
                {
                    $this->servers[get_ident($info)] = $info;
                }
                $info = array();
            }
            else
            {
                $parts = split(" ", $line);
                $info[$parts[0]] = urldecode($parts[1]);
            }
        }
    }

    private function save()
    {
        rewind($this->file);
        while(list($ident, $info) = each($this->servers))
        {
            while(list($label, $value) = each($info))
            {
                fwrite($this->file, $label . " " . urlencode($value) . "\n");
            }
            fwrite($this->file, "--\n");
        }
        // Truncate the rest.
        ftruncate($this->file, ftell($this->file));
    }

    function insert($info)
    {
        $this->servers[get_ident($info)] = $info;
    }

    function close()
    {
        if($this->writable)
        {
            $this->save();
            $this->dbUnlock();
            $this->writable = false;
        }
        fclose($this->file);
    }
}
