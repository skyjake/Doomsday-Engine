<?php
/** @file master.php Doomsday Master Server interface
 *
 * @authors Copyright Â© 2003-2013 Jaakko KerÃ¤nen <jaakko.keranen@iki.fi>
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

require_once('classes/masterserver.class.php');

function write_server($info)
{
    $ms = new MasterServer(true/*writable*/);
    $ms->insert($info);
}

/**
 * @param announcement  Server announcement string. UTF-8 assumed.
 * @param addr          Remote server address.
 */
function update_server($announcement, $addr)
{
    mb_regex_encoding('UTF-8');
    mb_internal_encoding('UTF-8');

    // First we must determine if the announcement is valid.
    $lines = mb_split("\r\n|\n|\r", $announcement);

    $info = new ServerInfo();

    while(list($line_number, $line) = each($lines))
    {
        // Separate the label from the value.
        $colon = strpos($line, ":");

        // It must exist near the beginning.
        if($colon === false || $colon >= 16) continue;

        $label = substr($line, 0, $colon);
        $value = substr($line, $colon + 1, strlen($line) - $colon - 1);

        if(strlen($value) >= 128) continue;

        // Ensure the label identifies a known property.
        if(isset($info[$label]))
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
    $graph = array();

    $ms = new MasterServer();
    foreach($ms->servers as $info)
    {
        $serverGraph = array();
        $info->populateGraphTemplate($serverGraph);

        $graph[$info->ident()] = $serverGraph;
    }

    $json = json_encode_clean($graph);
    header('Pragma: public');
    header('Cache-Control: public');
    header('Content-Type: application/json');
    header('Last-Modified: '. date(DATE_RFC1123, time()));
    header('Expires: '. date(DATE_RFC1123, strtotime('+5 days')));
    print $json;

    // Thats all folks!
    exit;
}

function return_xmllog()
{
    $ms = new MasterServer();
    // Update the log if necessary.
    $logPath = $ms->xmlLogFile();

    if($logPath !== false)
    {
        $result = file_get_contents_utf8($logPath, 'text/xml', 'utf-8');
        if($result !== false)
        {
            // Return the log to the client.
            header("Content-Type: text/xml; charset=utf-8");
            echo mb_ereg_replace('http', 'https', $result);
        }
    }
}

$query = $HTTP_SERVER_VARS['QUERY_STRING'];

// There are five operating modes:
// 1. Server announcement processing.
// 2. Answering a request for servers in plain text .
// 3. Answering a request for servers with a JSON data graph.
// 4. Retrieve a log of recent events as an XML file.
// 5. Web page.

if(isset($GLOBALS['HTTP_RAW_POST_DATA']) && empty($query))
{
    $announcement = $GLOBALS['HTTP_RAW_POST_DATA'];
    $remote = $HTTP_SERVER_VARS['REMOTE_ADDR'];

    update_server($announcement, $remote);
}
else if($query === 'list')
{
    // A server list request. Our response is a plain-text key ':' value list.
    $ms = new MasterServer();
    $ms->printServerList();

    exit; // Thats all folks!
}
else if($query === 'json')
{
    answer_request();
}
else if($query === 'xml')
{
    return_xmllog();
}
else
{
    // Forward this request to the server browser interface.
    header("Location: http://dengine.net/masterserver");
}
