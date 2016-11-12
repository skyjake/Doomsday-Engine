<?php
/*
 * Doomsday Web API: Master Server
 * Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * License: GPL v2+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses/gpl.html
 */

/*
 * The Master Server has the following responsibilities:
 *
 * - Receive server announcements (in JSON via HTTP POST), parse them, and store
 *   them in a database.
 * - Answer HTTP GET queries about which servers are currently running
 *   (also JSON).
 * - Remove expired entries from the database.
 */

require_once('api_config.inc.php'); // database config

define('DB_TABLE', 'servers');
define('EXPIRE_SECONDS', 300);

// Opens the database connection.
// @return MySQLi object.
function db_open()
{
    return new mysqli(DENG_DB_HOST, DENG_DB_USER, DENG_DB_PASSWORD, DENG_DB_NAME);
}

function db_query(&$db, $sql)
{
    $result = $db->query($sql);
    if (!$result)
    {
        echo "Database error: " . $db->error;
        echo "Query was: " . $sql;
        exit;
    }
    return $result;
}

// Initializes the Servers database table.
function db_init()
{
    $table = DB_TABLE;

    $db = db_open();
    db_query($db, "DROP TABLE IF EXISTS $table");

    $sql = "CREATE TABLE $table ("
         . "timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP "
            ."ON UPDATE CURRENT_TIMESTAMP, "
         . "address INT NOT NULL, "
         . "port SMALLINT UNSIGNED NOT NULL, "
         . "name VARCHAR(100) NOT NULL, "
         . "description VARCHAR(500), "
         . "version VARCHAR(30) NOT NULL, "
         . "compat INT NOT NULL, "
         . "plugin VARCHAR(50), "
         . "packages TEXT, "
         . "game_id VARCHAR(50) NOT NULL, "
         . "game_config VARCHAR(200), "
         . "map VARCHAR(50), "
         . "player_count SMALLINT, "
         . "player_max SMALLINT NOT NULL, "
         . "player_names TEXT, "
         . "flags INT UNSIGNED NOT NULL, "
         . "PRIMARY KEY (address, port)) CHARACTER SET utf8";

    db_query($db, $sql);
    $db->close();
}

function parse_announcement($json_data)
{
    $server_info = json_decode($json_data);
    if ($server_info == NULL) return; // JSON parse error.

    $address      = ip2long($_SERVER['REMOTE_ADDR']);
    $port         = (int) $server_info->port;
    $name         = urlencode($server_info->name);
    $description  = urlencode($server_info->desc);
    $version      = urlencode($server_info->ver);
    $compat       = (int) $server_info->cver;
    $plugin       = urlencode($server_info->plugin);
    $packages     = urlencode(json_encode($server_info->pkgs));
    $game_id      = urlencode($server_info->game);
    $game_config  = urlencode($server_info->cfg);
    $map          = urlencode($server_info->map);
    $player_count = (int) $server_info->pnum;
    $player_max   = (int) $server_info->pmax;
    $player_names = urlencode(json_encode($server_info->plrs));
    $flags        = (int) $server_info->flags;

    $db = db_open();
    $table = DB_TABLE;
    db_query($db, "DELETE FROM $table WHERE address = $address AND port = $port");
    db_query($db, "INSERT INTO $table (address, port, name, description, version, compat, plugin, packages, game_id, game_config, map, player_count, player_max, player_names, flags) "
        . "VALUES ($address, $port, '$name', '$description', '$version', $compat, '$plugin', '$packages', '$game_id', '$game_config', '$map', $player_count, $player_max, '$player_names', $flags)");
    $db->close();
}

function fetch_servers()
{
    $servers = array();

    $db = db_open();
    $table = DB_TABLE;

    // Expire old announcements.
    $expire_ts = time() - EXPIRE_SECONDS;
    db_query($db, "DELETE FROM $table WHERE UNIX_TIMESTAMP(timestamp) < $expire_ts");

    // Get all the remaining servers.
    $result = db_query($db, "SELECT * FROM $table");
    while ($row = $result->fetch_assoc())
    {
        $sv = array(
            "__obj__" => "Record",
            "host"    => long2ip($row['address']),
            "port"    => (int) $row['port'],
            "name"    => urldecode($row['name']),
            "desc"    => urldecode($row['description']),
            "ver"     => urldecode($row['version']),
            "cver"    => (int) $row['compat'],
            "plugin"  => urldecode($row['plugin']),
            "pkgs"    => json_decode(urldecode($row['packages'])),
            "game"    => urldecode($row['game_id']),
            "cfg"     => urldecode($row['game_config']),
            "map"     => urldecode($row['map']),
            "pnum"    => (int) $row['player_count'],
            "pmax"    => (int) $row['player_max'],
            "plrs"    => json_decode(urldecode($row['player_names'])),
            "flags"   => (int) $row['flags']
        );
        $servers[] = $sv;
    }
    $db->close();

    return $servers;
}

//---------------------------------------------------------------------------------------

if ($_SERVER['REQUEST_METHOD'] == 'GET')
{
    // GET requests are for querying information about servers.
    $op = $_GET['op'];
    if ($op == 'list')
    {
        $servers = fetch_servers();
        echo json_encode($servers);
    }
    else if (DENG_SETUP_ENABLED && $op == 'setup')
    {
        echo "Initializing database...";
        db_init();
    }
}
else if ($_SERVER['REQUEST_METHOD'] == 'POST')
{
    // POST requests are for submitting info about running servers.
    parse_announcement(file_get_contents("php://input"));
}
