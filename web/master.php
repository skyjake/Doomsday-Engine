<?php
/**
 * @file master.php
 * Doomsday Master Server interface
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

require_once('classes/masterserver.class.php');

define("XML_LOG_FILE", "masterfeed.xml");

$ms = NULL;

function MasterServer_inst()
{
    global $ms;
    if(is_null($ms))
    {
        $ms = new MasterServer(true);
    }
    return $ms;
}

function write_server($info)
{
    // We will not write empty infos.
    if(count($info) <= 10) return;

    $ms = MasterServer_inst();
    $ms->insert($info);
    $ms->close();
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
    $ms = MasterServer_inst();

    while(list($ident, $info) = each($ms->servers))
    {
        while(list($label, $value) = each($info))
        {
            if($label != "time") print "$label:$value\n";
        }
        // An empty line ends the server.
        print "\n";
    }
    $ms->close();
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

/**
 * @return  (Boolean) @c true iff the cached XML log file requires an update.
 */
function mustUpdateXmlLog()
{
    $logPath = XML_LOG_FILE;
    if(!file_exists($logPath)) return true;

    $ms = MasterServer_inst();
    return $ms->lastUpdate > @filemtime($logPath);
}

/**
 * Update the cached XML log file if necessary.
 *
 * @param includeDTD  (Boolean) @c true= Embed the DOCTYPE specification in file.
 *
 * @return  (Boolean) @c true= log was updated successfully (or didn't need updating).
 */
function updateXmlLog($includeDTD=true)
{
    if(!mustUpdateXmlLog()) return TRUE;

    $logFile = fopen(XML_LOG_FILE, 'w+');
    if(!$logFile) throw new Exception('Failed opening master server log (XML)');

    // Obtain write lock.
    flock($logFile, 2);

    $ms = MasterServer_inst();
    $numServers = (is_array($ms->servers) ? count($ms->servers) : 0);

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
    while(list($ident, $info) = each($ms->servers))
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

    $ms->close();

    return TRUE;
}

$query  = $HTTP_SERVER_VARS['QUERY_STRING'];

// There are four operating modes:
// 1. Server announcement processing.
// 2. Answering a request for servers.
// 3. XML output.

if(isset($GLOBALS['HTTP_RAW_POST_DATA']) && !$query)
{
    $remote = $HTTP_SERVER_VARS['REMOTE_ADDR'];

    update_server($GLOBALS['HTTP_RAW_POST_DATA'], $remote);
}
else if($query == 'list')
{
    answer_request();
}
else if($query == 'xml')
{
    // Update the cached copy of the XML log if necessary.
    try
    {
        updateXmlLog();
    }
    catch(Exception $e)
    {
        // @todo log the error.
        exit;
    }

    $result = file_get_contents_utf8(XML_LOG_FILE, 'text/xml', 'utf-8');
    if($result === false) exit; // Most peculiar...

    // Return the log to the client.
    header("Content-Type: text/xml; charset=utf-8");
    echo mb_ereg_replace('http', 'https', $result);
}
else
{
    // Forward this request to the server browser interface.
    header("Location: http://dengine.net/masterserver");
}
