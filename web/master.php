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

// Global Master Server instance.
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
    $ms = MasterServer_inst();

    $logPath = $ms->xmlLogFile();
    if($logPath === false) exit;

    $result = file_get_contents_utf8($logPath, 'text/xml', 'utf-8');
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
