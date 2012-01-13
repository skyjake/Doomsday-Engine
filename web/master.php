<?php
/**
 * @file master.php
 * Doomsday Master Server Script version 1.3
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
if(!defined("MasterServer"))
{
define("MasterServer", "MasterServer");

define("SCRIPT_NICE_NAME", "Doomsday Engine Master Server");

function get_ident($info)
{
    return $info['at'] . ":" . $info['port'];
}

class MasterServer
{
    /// Version number
    const VERSION_MAJOR = 1;
    const VERSION_MINOR = 3;

    /// File used to store the current state of the server database.
    const DATA_FILE = "servers.dat";

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

function write_server($info)
{
    // We will not write empty infos.
    if(count($info) <= 10) return;

    $db = new MasterServer(true);
    $db->insert($info);
    $db->close();
}

function update_server($announcement, $addr)
{
    $info = array();

    // First we must determine if the announcement is valid.
    $lines = split("\n", $announcement);
    while(list($line_number, $line) = each($lines))
    {
        // Separate the label from the value.
        $colon = strpos($line, ":");

        // It must exist near the beginning.
        if($colon === false || $colon >= 16) continue;

        $label = substr($line, 0, $colon);
        $value = substr($line, $colon + 1, strlen($line) - $colon - 1);

        // Let's make sure we know the label.
        if(in_array($label, array("port", "ver", "map", "game", "name",
            "info", "nump", "maxp", "open", "mode", "setup", "iwad", "pwads",
            "wcrc", "plrn", "data0", "data1", "data2"))
            && strlen($value) < 128)
        {
            // This will be included in the datafile.
            $info[$label] = $value;
        }
    }

    // Also include the remote address.
    $info['at'] = $addr;
    $info['time'] = time();

    // Let's write this info into the datafile.
    write_server($info);
}

function answer_request()
{
    $db = new MasterServer;
    while(list($ident, $info) = each($db->servers))
    {
        while(list($label, $value) = each($info))
        {
            if($label != "time") print "$label:$value\n";
        }
        // An empty line ends the server.
        print "\n";
    }
    $db->close();
}

function file_get_contents_utf8($fn, $contentType, $charset) {
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

function html_page()
{
    $logPath = "masterfeed.html";

    if(file_exists($logPath))
    {
        $db = new MasterServer();
        if(!($db->lastUpdate > @filemtime($logPath)))
            $db = 0;
    }
    else
    {
        $updateLogFile = 1;
        $db = new MasterServer();
    }

    if($db)
    {
        $logFile = fopen($logPath, 'w+');
        if(!$logFile) die();

        // Obtain write lock.
        flock($logFile, 2);

        fwrite($logFile,
            "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">".
            "\n<html>".
            "\n<head>".
            "\n<title>Public Doomsday Servers</title>".
            "\n</head>".
            "\n<body>".
            "\n<p>Server list published on " . gmdate("D, d M Y H:i:s \G\M\T") . "</p>".
            "\n<table border=\"1\" cellpadding=\"4\">".
            "\n<tr><th>Open <th>Location <th>Name <th>Info <th>Version ");

        //<th>Map <th>Setup <th>WAD <th>Players <th>Who's Playing?

        while(list($ident, $info) = each($db->servers))
        {
            $pwadArr = array_filter(explode(";", $info['pwads']));
            $pwadStr = implode(" ", $pwadArr);

            fwrite($logFile,
                "\n<tr><td>".
                ($info['open']? "Yes" : "<font color=\"red\">No</font>").
                "<td>{$info['at']}:{$info['port']}".
                "<td>". $info['name'].
                "<td>". $info['info'].
                "<td>Doomsday {$info['ver']}, {$info['game']}".
                "<tr><td><th>Setup<td>".
                "{$info['mode']} <td colspan=2>{$info['map']} {$info['setup']}".
                "<tr><td><th>WADs<td colspan=3>".
                "{$info['iwad']} (" . dechex($info['wcrc']) . ") $pwadStr".
                "<tr><td><th>Players<td colspan=3>".
                "{$info['nump']} / {$info['maxp']}: ".
                "{$info['plrn']}");
        }

        fwrite($logFile, "\n</table>\n</body>\n</html>\n");
        flock($logFile, 3);
        fclose($logFile);

        $db->close();
    }

    if(!file_exists($logPath))
    {
        //throw new Exception(sprintf('file %s not present in content cache.', $logFile));
        return;
    }

    header("Content-Type: text/html; charset=utf-8");
    $result = file_get_contents_utf8($logPath, "text/html", "utf-8");
    if($result == false)
    {
        header("Location: https:".$logPath); // fallback
        exit();
    }
    else
    {
        echo mb_ereg_replace("http","https",$result);
        exit;
    }
}

function xml_page($includeDTD=1)
{
    $logPath = "masterfeed.xml";
    if(file_exists($logPath))
    {
        $db = new MasterServer();
        if(!($db->lastUpdate > @filemtime($logPath)))
            $db = 0;
    }
    else
    {
        $updateLogFile = 1;
        $db = new MasterServer();
    }

    if($db)
    {
        $logFile = fopen($logPath, 'w+');
        if(!$logFile) die();

        // Obtain write lock.
        flock($logFile, 2);

        $numServers = (is_array($db->servers) ? count($db->servers) : 0);

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
            "\n<generator>". (SCRIPT_NICE_NAME .' '. MasterServer::VERSION_MAJOR .'.'. MasterServer::VERSION_MINOR) .'</generator>'.
            "\n<generatorurl>". $urlStr .'</generatorurl>'.
            "\n<pubdate>". gmdate("D, d M Y H:i:s \G\M\T") .'</pubdate>'.
            "\n<language>en</language>".
            "\n</channel>");

        fwrite($logFile, "\n<serverlist size=\"". $numServers .'">');
        while(list($ident, $info) = each($db->servers))
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

        $db->close();
    }

    header("Content-Type: text/xml; charset=utf-8");
    $result = file_get_contents_utf8($logPath, "text/xml", "utf-8");
    if($result == false)
    {
        header("Location: https:".$logPath); // fallback
        exit();
    }
    else
    {
        echo mb_ereg_replace("http","https",$result);
        exit;
    }
}

$query  = $HTTP_SERVER_VARS['QUERY_STRING'];
$remote = $HTTP_SERVER_VARS['REMOTE_ADDR'];

// There are four operating modes:
// 1. Server announcement processing.
// 2. Answering a request for servers.
// 3. WWW-friendly presentation for web browsers.
// 4. XML output.

if(isset($GLOBALS['HTTP_RAW_POST_DATA']) && !$query)
{
    update_server($GLOBALS['HTTP_RAW_POST_DATA'], $remote);
}
else if($query == 'list')
{
    answer_request();
}
else if($query == 'xml')
{
    xml_page();
}
else
{
    html_page();
}

} /// End include guard MasterServer
