<?php

/*
 * Doomsday Web API: Build Database Administration
 * Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

require_once(__DIR__.'/../database.inc.php');

function add_build($json_args)
{
    $args = json_decode($json_args);
    if ($args == NULL) return; // JSON parse error.

    $build   = (int) $args->build;
    $type    = ($args->type == 'stable'?    BT_STABLE :
                $args->type == 'candidate'? BT_CANDIDATE : BT_UNSTABLE);
    $version = $db->real_escape_string($args->version);
    $major   = (int) $args->major;
    $minor   = (int) $args->minor;
    $patch   = (int) $args->patch;
    $label   = $db->real_escape_string($args->label);
    $changes = $db->real_escape_string($args->changes);

    $db = db_open();
    db_query($db, "INSERT INTO ".DB_TABLE_BUILDS 
        . " (build, type, version, major, minor, patch, label, changes) VALUES ("
        . "$build, $type, '$version', $major, $minor, $patch, '$label', '$changes')");
    $db->close();
}

function get_platform_id($db, $platform)
{
    $result = db_query($db, "SELECT id FROM ".DB_TABLE_PLATFORMS
        ." WHERE platform='$platform'");
    while ($row = $result->fetch_assoc()) {
        return $row['id'];
    }
    return 0;
}

function add_file($json_args)
{
    $args = json_decode($json_args);
    if ($args == NULL) return; // JSON parse error.
    
    $db = db_open();
        
    $build = (int) $args->build;
    $plat_id = get_platform_id($db, $args->platform);
    $type = ($args->type == 'binary'? FT_BINARY :
             $args->type == 'log'?    FT_LOG : 
                                      FT_NONE);
    $name = $db->real_escape_string($args->name);
    
    $header = 'build, plat_id, type, name';
    $values = "$build, $plat_id, $type, '$name'";
    
    if (property_exists('md5', $args)) {
        $md5 = $db->real_escape_string($args->md5);
        $header .= ', md5';
        $values .= ", '$md5'";
    }
    if (property_exists('signature', $args)) {
        $sigature = $db->real_escape_string($args->signature);
        $header .= ', signature';
        $values .= ", '$signature'";
    }
    
    db_query($db, "INSERT INTO ".DB_TABLE_FILES." ($header) VALUES ($values)");
    $db->close();
}

function remove_file($db, $file_id)
{
    if ($path = db_file_path($db, $file_id)) {
        echo("Deleting $path...\n");
        unlink($path);        
    }
    db_query($db, "DELETE FROM ".DB_TABLE_FILES." WHERE id=$file_id");
}

function purge_old_builds()
{
    $expire_ts = time() - 100 * 24 * 60 * 60;
    
    // Find non-stable builds that have become obsolete.
    $db = db_open();        
    $result = db_query($db, "SELECT build FROM ".DB_TABLE_BUILDS
        ." WHERE type != ".BT_STABLE." AND UNIX_TIMESTAMP(timestamp) < $expire_ts");
    $builds_sql = "";
    $file_paths = array();
    while ($row = $result->fetch_assoc()) {
        $build = $row['build'];
        echo("Purging build $build...");
        if ($builds_sql) $builds_sql .= " OR ";
        $builds_sql .= "build=$build";
        foreach (db_build_list_files($db, $build) as $file) {
            $file_paths[] = db_file_path($db, file);
        }
    }
    if ($builds_sql) {
        db_query($db, "DELETE FROM ".DB_TABLE_BUILDS." WHERE $builds_sql");
        db_query($db, "DELETE FROM ".DB_TABLE_FILES ." WHERE $builds_sql");
    }
    $db->close();
    
    // Remove the files, too.
    foreach ($file_paths as $path) {
        echo("Deleting: $path");
        unlink($path);        
    }
}

//---------------------------------------------------------------------------------------

// Check arguments.
$op = $argv[1];

if ($op == 'drop_tables')
{
    echo("Dropping database tables...\n");
    $db = db_open();
    db_query($db, "DROP TABLE IF EXISTS ".DB_TABLE_BUILDS);
    db_query($db, "DROP TABLE IF EXISTS ".DB_TABLE_FILES);
    db_query($db, "DROP TABLE IF EXISTS ".DB_TABLE_PLATFORMS);
    $db->close();
}
else if ($op == 'init') 
{
    echo("Initializing database tables...\n");
    $db = db_open();
        
    $table = DB_TABLE_PLATFORMS;
    $sql = "CREATE TABLE $table ("
        . "id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        . "ord INT NOT NULL, "
        . "platform VARCHAR(50) NOT NULL, "
        . "name VARCHAR(100) NOT NULL, "
        . "os VARCHAR(20) NOT NULL, "
        . "cpu_arch VARCHAR(20) NOT NULL, "
        . "cpu_bits INT NOT NULL"
        . ") CHARACTER SET utf8";
    db_query($db, $sql);

    // Set up the known platforms.    
    db_query($db, "INSERT INTO $table (ord, platform, name, os, cpu_arch, cpu_bits) VALUES "
        . "(100, 'win-x64', 'Windows 7 (64-bit)', 'windows', 'x64', 64), "
        . "(200, 'win-x86', 'Windows 7 (32-bit)', 'windows', 'x86', 32), "
        . "(300, 'mac10_8-x86_64', 'macOS 10.8', 'macx', 'x86_64', 64), "
        . "(400, 'ubuntu-x86_64', 'Ubuntu 16.04 LTS', 'linux', 'amd64', 64), "
        . "(500, 'fedora-x86_64', 'Fedora 23', 'linux', 'x86_64', 64), "
        . "(600, 'source', 'Source', 'any', 'any', 0);");
    
    // File archive.
    $table = DB_TABLE_FILES;
    $sql = "CREATE TABLE $table ("
        . "id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
        . "build INT UNSIGNED NOT NULL, "
        . "plat_id INT UNSIGNED NOT NULL, "
        . "type TINYINT NOT NULL, "
        . "name VARCHAR(200) NOT NULL, "
        . "size INT UNSIGNED NOT NULL, "
        . "md5 CHAR(32), "
        . "signature TEXT, "
        . "timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        . ") CHARACTER SET utf8";
    db_query($db, $sql);
    
    // Builds.
    $table = DB_TABLE_BUILDS;
    $sql = "CREATE TABLE $table ("
        . "build INT UNSIGNED NOT NULL PRIMARY KEY, "
        . "type TINYINT NOT NULL, "
        . "version VARCHAR(50) NOT NULL, "
        . "major INT NOT NULL, "
        . "minor INT NOT NULL, "
        . "patch INT NOT NULL, "
        . "label VARCHAR(30) NOT NULL, "
        . "blurb TEXT, "
        . "changes TEXT, "
        . "timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        . ") CHARACTER SET utf8";
    db_query($db, $sql);        
    $db->close();
}
else if ($op == 'add_build')
{
    add_build(file_get_contents("php://input"));
}
else if ($op == 'add_file')
{
    add_file(file_get_contents("php://input"));
}
else if ($op == 'purge')
{
    purge_old_builds();
}

echo("Admin done.\n");
