<?php
/*
The Doomsday Master Server Script 1.1
by Jaakko Keränen <skyjake@doomsdayhq.com>
*/

function get_ident($info)
{
	return $info['at'] . ":" . $info['port'];
}

class Database
{
	var $filename;
	var $writable;
	var $file;
	var $servers;
	
	function Database($writable = false)
	{
		global $HTTP_SERVER_VARS;
		$this->filename = "servers.dat";
		$this->writable = $writable;
		$this->file = fopen($this->filename, $writable? 'r+' : 'r');
		if(!$this->file) die();
		$this->lock();
		$this->load();
		if(!$writable) $this->unlock();
	}
	
	function lock()
	{
		flock($this->file, $this->writable? 2 : 1);
	}
	
	function unlock()
	{
		flock($this->file, 3);
	}
	
	function load()
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
	
	function save()
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
			$this->unlock();
			$this->writable = false;
		}
		fclose($this->file);
	}
}

function write_server($info)
{
	// We will not write empty infos.
	if(count($info) <= 10) return;
	
	$db = new Database(true);	
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
