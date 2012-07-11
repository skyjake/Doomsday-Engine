<?php
/*
 * The Doomsday Master Server 2.0
 * by Jaakko Keranen <jaakko.keranen@iki.fi>
 *
 * Master server database access.
 */

// Configuration.
define('DB_NAME', 'dengine');
define('DB_USER', 'dengine');     
define('DB_PASSWORD', 'dengserv');
define('DB_HOST', 'localhost');    
define('PREFIX', 'master_');

// Compose an identifier for a server based on its IP address and port number.
function make_ident(&$info)
{
	return $info['at'] . ":" . $info['port'];
}

// Quote variable to make safe (from php.net)
function quote_smart($value)
{
    // Stripslashes
    if (get_magic_quotes_gpc()) 
    {
        $value = stripslashes($value);
    }
    // Quote if not a number or a numeric string
    if (!is_numeric($value)) 
    {
        $value = "'" . mysql_real_escape_string($value) . "'";
    }
    return $value;
}

function convert_time($stamp)
{
    $year = substr($stamp, 0, 4);
    $month = substr($stamp, 4, 2);
    $day = substr($stamp, 6, 2);
    $hour = substr($stamp, 8, 2);
    $minute = substr($stamp, 10, 2);
    $second = substr($stamp, 12, 2);
    return mktime($hour, $minute, $second, $month, $day, $year);
}

function append_assign(&$query, $key, &$info)
{
    $query .= ', ' . $key . '=' . quote_smart($info[$key]);
}

class Database
{
	//var $filename;
	//var $writable;
	//var $file;
	var $servers;
    var $db;
	
	function Database()
	{
        // Open a database connection to the master server data.
        $this->db = mysql_connect(DB_HOST, DB_USER, DB_PASSWORD)
            OR die(mysql_error());

		mysql_select_db(DB_NAME, $this->db);
	}
        
	function close()
	{
        // Close the database connection.
        // No need to close explicitly.
    }
    
    function query($q)
    {
        //echo '<p>';
        //echo $q;
        $result = mysql_query($q, $this->db);
        $err = mysql_error();
        if($err)
        {
            print "DB error: ".$err;
        }
        return $result;
    }
	
    // Returns the next row of the result as an associative array.
    function next_result($result)
    {
        return mysql_fetch_assoc($result);
    } 
    
    function reset()
    {
        // Recreates the tables in the database.
        $this->query('DROP TABLE IF EXISTS ' . PREFIX.'servers');
        $this->query('DROP TABLE IF EXISTS ' . PREFIX.'historical_servers');

        $this->query('CREATE TABLE '. PREFIX.'servers ' .
                     '('.
                     'id VARCHAR(25) NOT NULL UNIQUE, ' .
                     'at VARCHAR(20) NOT NULL, ' .
                     'port SMALLINT UNSIGNED NOT NULL, ' .
                     'time TIMESTAMP NOT NULL, ' . 
                     'firstseentime TIMESTAMP NOT NULL, ' .
                     'name TEXT NOT NULL, ' .
                     'info TEXT, ' .
                     'ver TEXT NOT NULL, ' .
                     'game VARCHAR(40) NOT NULL, ' .
                     'mode TEXT, ' .
                     'setup TEXT, ' .
                     'iwad VARCHAR(50) NOT NULL, ' .
                     'pwads TEXT, ' .
                     'wcrc INT UNSIGNED NOT NULL, ' .
                     'plrn TEXT, ' .
                     'map VARCHAR(20), ' .
                     'nump INT, ' .
                     'maxp INT, ' .
                     'open TINYINT UNSIGNED NOT NULL, ' .
                     'data0 VARCHAR(10), ' .
                     'data1 VARCHAR(10), ' .
                     'data2 VARCHAR(10), PRIMARY KEY(id))');

        $this->query('CREATE TABLE '. PREFIX.'historical_servers ' .
                     '('.
                     'id BIGINT UNSIGNED NOT NULL UNIQUE AUTO_INCREMENT, ' .
                     'archtime TIMESTAMP NOT NULL, ' .
                     'firstseentime TIMESTAMP NOT NULL, ' .
                     'ver VARCHAR(30) NOT NULL, ' .
                     'game VARCHAR(50) NOT NULL, ' .
                     'mode VARCHAR(50), ' .
                     'setup VARCHAR(100), ' .
                     'iwad VARCHAR(50), ' .
                     'pwads TEXT, ' .
                     'wcrc INT UNSIGNED, ' .
                     'maxp INT, ' .
                     'PRIMARY KEY(id))');
    }
    
    // Reads a list of the non-historical servers into the servers array.
    // Moves old servers into the historical table.
	function load()
	{
		$now = time();
		$max_age = 130; // 2+ mins.
		$this->servers = array();
		
        // Array for expired servers, to be inserted into the historical table.
        $expired_servers = array();
        
        // Make a query for all the current servers.        
        $res = $this->query('SELECT * FROM '.PREFIX.'servers');
        
        // Iterate through the servers.
		while($info = mysql_fetch_assoc($res))
		{
            //print_r($info);
            $age = $now - convert_time($info['time']);
            // Put the information of expired servers into another array.
            if($age < $max_age)
            {
                //print "CURRENT(".$age.")\n";
                // Remove some fields from the info.
                unset($info['id']);
                unset($info['firstseentime']);
                $this->servers[make_ident($info)] = $info;
            }
            else
            {
                //print "HISTORICAL(".strftime("%a, %d %b %Y %H:%M:%S %z",$now).",".
                //    strftime("%a, %d %b %Y %H:%M:%S %z", convert_time($info['time'])).",".$age.")\n";
                $expired_servers[make_ident($info)] = $info;
            }
		}
        
        if(count($expired_servers) > 0)
        {
            // Insert them as new items into the historical array.
            $q = 'INSERT INTO '.PREFIX.'historical_servers '.
                 '(archtime, firstseentime, ver, game, mode, setup, iwad, pwads, wcrc, maxp) VALUES ';
            $the_first = TRUE;
            $delq = 'DELETE FROM '.PREFIX.'servers WHERE ';
            while(list($ident, $info) = each($expired_servers))
            {
                if(!$the_first) 
                {
                    $q .= ', ';
                    $delq .= ' OR ';
                }
                $the_first = FALSE;
                
                $q .= '(NULL, ' . 
                        quote_smart($info['firstseentime']) . ', ' .
                        quote_smart($info['ver']) . ', ' .
                        quote_smart($info['game']) . ', ' .
                        quote_smart($info['mode']) . ', ' .
                        quote_smart($info['setup']) . ', ' .
                        quote_smart($info['iwad']) . ', ' .
                        quote_smart($info['pwads']) . ', ' .
                        quote_smart($info['wcrc']) . ', ' .
                        quote_smart($info['maxp']) . ') ';
                $delq .= 'id='.quote_smart(make_ident($info));
            }
            $this->query($q);
            $this->query($delq);
        }
	}
	
    // Insert/update a server into the database.
	function insert($info)
	{
        // First try to update an existing entry in the current servers table.
        $up = 'UPDATE ' . PREFIX . 'servers SET time=NULL';
        append_assign($up, 'name', $info);
        append_assign($up, 'info', $info);
        append_assign($up, 'ver', $info);
        append_assign($up, 'game', $info);
        append_assign($up, 'mode', $info);
        append_assign($up, 'setup', $info);
        append_assign($up, 'iwad', $info);
        append_assign($up, 'pwads', $info);
        append_assign($up, 'wcrc', $info);
        append_assign($up, 'plrn', $info);
        append_assign($up, 'map', $info);
        append_assign($up, 'nump', $info);
        append_assign($up, 'maxp', $info);
        append_assign($up, 'open', $info);
        append_assign($up, 'data0', $info);
        append_assign($up, 'data1', $info);
        append_assign($up, 'data2', $info);
        $up = $up . ' WHERE id=' . quote_smart(make_ident($info));
        
        $res = $this->query($up);
        if(mysql_affected_rows() > 0)
        {
            // Done.
            return;
        }
        
        // Insert it as a new entry.
        $q = 'REPLACE INTO '. PREFIX."servers (id, at, port, firstseentime, ";
        $q .= "time, name, info, ver, game, mode, setup, iwad, pwads, wcrc, plrn, ";
        $q .= "map, nump, maxp, open, data0, data1, data2) VALUES (";
        $q .= quote_smart(make_ident($info)) .",".
                quote_smart($info['at']) .",".
                quote_smart($info['port']) .", NULL,NULL,".
                quote_smart($info['name']) .",".
                quote_smart($info['info']) .",".
                quote_smart($info['ver']) .",".
                quote_smart($info['game']) .",".
                quote_smart($info['mode']) .",".
                quote_smart($info['setup']) .",".
                quote_smart($info['iwad']) .",".
                quote_smart($info['pwads']) .",".
                quote_smart($info['wcrc']) .",".
                quote_smart($info['plrn']) .",".
                quote_smart($info['map']) .",".
                quote_smart($info['nump']) .",".
                quote_smart($info['maxp']) .",".
                quote_smart($info['open']) .",".
                quote_smart($info['data0']) .",".
                quote_smart($info['data1']) .",".
                quote_smart($info['data2']) .")";

        $this->query($q);
	}
}

function do_reset()
{
    $db = new Database;
    $db->reset();
    $db->close();
    echo 'reseted the database';
}

?>
