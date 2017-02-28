<?php
/*
 * Doomsday Web API: Build Information
 * Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

require_once('database.inc.php');

ini_set('display_errors', 1);
error_reporting(E_ALL ^ E_NOTICE);

function generate_header($page_title)
{
    echo("<html><head>\n");
    echo("  <meta http-equiv='Content-Type' content='text/html;charset=UTF-8'>\n");
    echo("  <link href='http://fonts.googleapis.com/css?family=Open+Sans:400italic,400,300,700' rel='stylesheet' type='text/css'>\n");
    echo("  <link href='http://api.dengine.net/1/build_page.css' rel='stylesheet' type='text/css'>\n");
    echo("  <title>$page_title</title>\n");
    echo("</head><body>\n");
    echo("<h1>$page_title</h1>\n");
}

function generate_footer()
{
    echo("</body></html>");                
}

function generate_build_page($number)
{
    $db = db_open();
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_BUILDS." WHERE build=$number");
    if ($row = $result->fetch_assoc()) {
        $type    = $row['type'];
        $version = $row['version'];
        $blurb   = $row['blurb'];
        $ts      = $row['timestamp'];
    
        $files = db_list_build_files($db, $number);
                
        // Output page header.
        generate_header("Build $number");
    
        echo("<p class='links'><a href='http://dengine.net/'>dengine.net</a> | <a href='".DENG_API_URL."/builds?format=html'>Autobuilder Index</a>| <a href='".DENG_API_URL."/builds?format=feed'>RSS Feed</a></p>\n");
    
        echo(db_build_summary($db, $number));
    
        // Output page footer.
        generate_footer();
    }
    $db->close();
}

function generate_build_index_page()
{
    generate_header("Doomsday Autobuilder");

    echo("<p class='links'><a href='http://dengine.net/'>dengine.net</a> | <a href='".DENG_API_URL."/builds?format=feed'>RSS Feed</a></p>"
        .'<h2>Latest Builds</h2>'
        .'<div class="buildlist">');
    
    $all_versions = array();
    $db = db_open();
    $result = db_query($db, "SELECT build, type, UNIX_TIMESTAMP(timestamp), version"
        ." FROM ".DB_TABLE_BUILDS." ORDER BY timestamp DESC");
    while ($row = $result->fetch_assoc()) {
        $build     = $row['build'];
        $type      = build_type_text($row['type']);
        $timestamp = $row['UNIX_TIMESTAMP(timestamp)'];
        $version   = $row['version'];
                
        // Collect all known versions.
        $info = array(
            "build"   => $build,
            "type"    => $type,
            "date"    => strftime('%b %d', $timestamp),
            "version" => $version
        );
        if (in_array($version, $all_versions)) {
            $all_versions[$version][] = $info;
        }
        else {
            $all_versions[$version] = array($info);
        }

        $build_date = $info['date'];
        echo("<div class='build $type'>"
            ."<a href=\"".DENG_API_URL."/builds?number=${build}&format=html\">"
            ."<div class='buildnumber'>$build</div>"
            ."<div class='builddate'>$build_date</div>"
            ."<div class='buildversion'>$version</div></a></div>\n");
    }
    echo("</div>\n");
    $db->close();
    
    echo("<h2>Versions</h2>\n");
    foreach ($all_versions as $version => $builds) {
        echo("<h3>$version</h3>"
            ."<div class='buildlist'>\n");
        foreach ($builds as $info) {
            $type       = $info['type'];
            $build_date = $info['date'];
            $build      = $info['build'];
            echo("<div class='build $type'>"
                ."<a href=\"".DENG_API_URL."/builds?number=${build}&format=html\">"
                ."<div class='buildnumber'>$build</div>"
                ."<div class='builddate'>$build_date</div>"
                ."<div class='buildversion'>$version</div></a></div>\n");
        }
        echo("</div>\n");
    }

    // Output page footer.
    generate_footer();
}

//---------------------------------------------------------------------------------------

if ($_SERVER['REQUEST_METHOD'] == 'GET') {
    $number = (int) $_GET['number'];
    $format = $_GET['format'];
    if ($format == 'html') {
        if ($number > 0) {
            generate_build_page($number);
        }
        else {
            generate_build_index_page();
        }
    }
    else if ($format == 'feed') {
        //generate_build_feed();
    }
}
