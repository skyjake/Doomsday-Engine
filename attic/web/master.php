<?php
/*
 * The Doomsday Master Server 2.0
 * by Jaakko Keranen <jaakko.keranen@iki.fi>
 *
 * This file is the main interface to the master server.
 */

require('masterdb.php');

function write_server($info)
{
	// We will not write empty infos.
	if(count($info) <= 10) return;
	
	$db = new Database;	
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
			"wcrc", "plrn",	"data0", "data1", "data2"))
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
	$db = new Database;	
    $db->load();
    
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

function web_page()
{
	$db = new Database;
    $db->load();
	
	print '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">';
	print "\n<html><head><title>Public Doomsday Servers</title></head>\n";
	print "<body>\n";
	print "<p>Server list at " . date("D, j M Y H:i:s O") . ":</p>\n";
	print "<table border=1 cellpadding=4>\n";
	print "<tr><th>Open <th>Location <th>Name <th>Info " .
		"<th>Version\n";
		
	//<th>Map <th>Setup <th>WAD <th>Players <th>Who's Playing?
	
	while(list($ident, $info) = each($db->servers))
	{
		print "<tr><td>";
		print $info['open']? "Yes" : "<font color=\"red\">No</font>";
		print "<td>{$info['at']}:{$info['port']}";
		print "<td>" . $info['name'];
		print "<td>" . $info['info'];
		print "<td>Doomsday {$info['ver']}, {$info['game']}";
		
		print "<tr><td><th>Setup<td>";
		
		print "{$info['mode']} <td colspan=2>{$info['map']} {$info['setup']}";
		
		print "<tr><td><th>WADs<td colspan=3>";
		print "{$info['iwad']} (" . dechex($info['wcrc']) . ") {$info['pwads']}";
		
		print "<tr><td><th>Players<td colspan=3>";
		print "{$info['nump']} / {$info['maxp']}: ";
	  print "{$info['plrn']}";
	}
	
	print "</table></body></html>\n";
		
	$db->close();
}

//global $HTTP_SERVER_VARS;

$query  = $HTTP_SERVER_VARS['QUERY_STRING'];
$remote = $HTTP_SERVER_VARS['REMOTE_ADDR'];
 
// There are three operating modes:
// 1. Server announcement processing.
// 2. Answering a request for servers.
// 3. WWW-friendly presentation for web browsers.

if($HTTP_RAW_POST_DATA && !$query)
{
	update_server($HTTP_RAW_POST_DATA, $remote);
}
else if($query == "list")
{
	answer_request();
}
else
{
	web_page();
}

?>
