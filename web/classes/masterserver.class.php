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

require_once('includes/platform.inc.php');
require_once('includes/utilities.inc.php');

includeGuard('MasterServer');

function get_ident($info)
{
    if(!is_array($info))
        throw new Exception('Invalid info argument, array expected.');
    if(!isset($info['at']))
        throw new Exception('Invalid info, parameter \'at\' not specified.');
    if(!isset($info['port']))
        throw new Exception('Invalid info, parameter \'port\' not specified.');

    return $info['at'] . ":" . $info['port'];
}

class MasterServer
{
    /// Version number
    const VERSION_MAJOR = 1;
    const VERSION_MINOR = 3;

    /// File used to store the current state of the server database.
    const DATA_FILE = 'cache/master/servers.dat';

    /// File used to store the last published server feed.
    const XML_LOG_FILE = 'cache/master/eventfeed.xml';

    public $servers;
    public $lastUpdate;

    private $isWritable;
    private $file;

    public function __construct($writable=false)
    {
        $this->isWritable = $writable;
        $this->file = fopen_recursive(self::DATA_FILE, $this->isWritable? 'r+' : 'r');
        if(!$this->file) die();
        $this->lastUpdate = @filemtime(self::DATA_FILE);
        $this->dbLock();
        $this->load();
        if(!$this->isWritable) $this->dbUnlock();
    }

    public function __destruct()
    {
        // If writable and the data file is open; save it and/or close.
        $this->close();
    }

    private function dbLock()
    {
        flock($this->file, $this->isWritable? 2 : 1);
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
                $info[$parts[0]] = isset($parts[1])? urldecode($parts[1]) : "";
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
        try
        {
            $this->servers[get_ident($info)] = $info;
        }
        catch(Exception $e)
        {
            $errorMsg = 'Unhandled exception "'. $e->getMessage() .'" in '. __CLASS__ .'::'. __METHOD__ .'().';
            trigger_error($errorMsg);
        }
    }

    function close()
    {
        if(!$this->file) return;

        if($this->isWritable)
        {
            $this->save();
            $this->dbUnlock();
            $this->isWritable = false;
        }

        fclose($this->file);
        $this->file = 0;
    }

    /**
     * @return  (Boolean) @c true iff the cached XML log file requires an update.
     */
    private function mustUpdateXmlLog()
    {
        $logPath = self::XML_LOG_FILE;
        if(!file_exists($logPath)) return true;

        return $this->lastUpdate > @filemtime($logPath);
    }

    /**
     * Update the cached XML log file if necessary.
     *
     * @param includeDTD  (Boolean) @c true= Embed the DOCTYPE specification in file.
     *
     * @return  (Boolean) @c true= log was updated successfully (or didn't need updating).
     */
    private function updateXmlLog($includeDTD=true)
    {
        if(!$this->mustUpdateXmlLog()) return TRUE;

        $logFile = fopen_recursive(self::XML_LOG_FILE, 'w+');
        if(!$logFile) throw new Exception('Failed opening master server log (XML)');

        // Obtain write lock.
        flock($logFile, 2);

        $numServers = (is_array($this->servers) ? count($this->servers) : 0);

        $urlStr = ((isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on')? 'https://' : 'http://')
                . $_SERVER['HTTP_HOST'] . parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);

        fwrite($logFile, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");

        if($includeDTD !== 0)
        {
            // Embed our DTD so that our server lists can be transported more easily.
            fwrite($logFile,
              "\n<!DOCTYPE masterserver [
              <!ELEMENT masterserver ((channel?),serverlist)>
              <!ELEMENT channel (generator,(generatorurl?),(pubdate?),(language?))>
              <!ELEMENT generator (#PCDATA)>
              <!ELEMENT generatorurl (#PCDATA)>
              <!ELEMENT pubdate (#PCDATA)>
              <!ELEMENT language (#PCDATA)>
              <!ELEMENT serverlist (server*)>
              <!ATTLIST serverlist size CDATA #REQUIRED>
              <!ELEMENT server (name,info,ip,port,open,version,gameinfo)>
              <!ATTLIST server host CDATA #REQUIRED>
              <!ELEMENT name (#PCDATA)>
              <!ELEMENT info (#PCDATA)>
              <!ELEMENT ip (#PCDATA)>
              <!ELEMENT port (#PCDATA)>
              <!ELEMENT open (#PCDATA)>
              <!ELEMENT version (#PCDATA)>
              <!ATTLIST version doomsday CDATA #REQUIRED>
              <!ATTLIST version game CDATA #REQUIRED>
              <!ELEMENT gameinfo (mode,iwad,(pwads?),setupstring,map,numplayers,maxplayers,(playernames?))>
              <!ELEMENT mode (#PCDATA)>
              <!ELEMENT iwad (#PCDATA)>
              <!ATTLIST iwad crc CDATA #REQUIRED>
              <!ELEMENT pwads (#PCDATA)>
              <!ELEMENT setupstring (#PCDATA)>
              <!ELEMENT map (#PCDATA)>
              <!ELEMENT numplayers (#PCDATA)>
              <!ELEMENT maxplayers (#PCDATA)>
              <!ELEMENT playernames (#PCDATA)>
            ]>");
        }

        fwrite($logFile, "\n<masterserver>");

        fwrite($logFile,
            "\n<channel>".
            "\n<generator>". ('Doomsday Engine Master Server '. MasterServer::VERSION_MAJOR .'.'. MasterServer::VERSION_MINOR) .'</generator>'.
            "\n<generatorurl>". $urlStr .'</generatorurl>'.
            "\n<pubdate>". gmdate("D, d M Y H:i:s \G\M\T") .'</pubdate>'.
            "\n<language>en</language>".
            "\n</channel>");

        fwrite($logFile, "\n<serverlist size=\"". $numServers .'">');
        while(list($ident, $info) = each($this->servers))
        {
            if($info['pwads'] !== '')
            {
                $pwadArr = array_filter(explode(";", $info['pwads']));
                $pwadStr = implode(" ", $pwadArr);
            }
            else
            {
                $pwadStr = "";
            }

            fwrite($logFile,
                "\n<server host=\"{$info['at']}:{$info['port']}\">".
                "\n<name>". $info['name'] .'</name>'.
                "\n<info>". $info['info'] .'</info>'.
                "\n<ip>{$info['at']}</ip>".
                "\n<port>{$info['port']}</port>".
                "\n<open>". ($info['open']? 'yes' : 'no') .'</open>'.
                "\n<version doomsday=\"{$info['ver']}\" game=\"{$info['game']}\"/>".
                "\n<gameinfo>".
                    "\n<mode>{$info['mode']}</mode>".
                    "\n<iwad crc=\"". dechex($info['wcrc']) ."\">{$info['iwad']}</iwad>".
                ($info['pwads'] !== ''? "\n<pwads>$pwadStr</pwads>" : '').
                    "\n<setupstring>{$info['setup']}</setupstring>".
                    "\n<map>{$info['map']}</map>".
                    "\n<numplayers>{$info['nump']}</numplayers>".
                    "\n<maxplayers>{$info['maxp']}</maxplayers>".
                ($info['plrn'] !== ''? "\n<playernames>{$info['plrn']}</playernames>" : '').
                "\n</gameinfo>".
                "\n</server>");
        }
        fwrite($logFile, "\n</serverlist>");
        fwrite($logFile, "\n</masterserver>");

        flock($logFile, 3);
        fclose($logFile);

        return TRUE;
    }

    /**
     * Update and return the path of the server event XML log.
     *
     * @return (Mixed) (Boolean) @c false= Failed
     *                  (String) Local path to the log.
     */
    public function xmlLogFile()
    {
        // Update the cached copy of the XML log if necessary.
        try
        {
            $this->updateXmlLog();
        }
        catch(Exception $e)
        {
            $errorMsg = 'Unhandled exception "'. $e->getMessage() .'" in '. __CLASS__ .'::'. __METHOD__ .'().';
            trigger_error($errorMsg);
            return false;
        }
        return self::XML_LOG_FILE;
    }
}
