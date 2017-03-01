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

ini_set('display_errors', 1);
error_reporting(E_ALL ^ E_NOTICE);

require_once('builds.inc.php');

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
        $build_info = $row;
        $type    = $row['type'];
        $version = $row['version'];

        $files = db_build_list_files($db, $number);

        // Output page header.
        generate_header("Build $number");

        echo("<p class='links'><a href='http://dengine.net/'>dengine.net</a> | <a href='".DENG_API_URL."/builds?format=html'>Autobuilder Index</a>| <a href='".DENG_API_URL."/builds?format=feed'>RSS Feed</a></p>\n");

        echo(db_build_summary($db, $number));

        // Files to download.
        echo("<h2>Downloads</h2>\n"
            ."<table class='downloads'>\n");
        $last_plat = '';
        foreach (db_platform_list($db) as $plat) {
            // Find the binaries for this platform.
            $bins = db_build_list_platform_files($db, $number, $plat['id'], FT_BINARY);
            $bin_count = count($bins);
            for ($i = 0; $i < $bin_count; $i++) {
                $bin = $bins[$i];
                $mb_size = number_format($bin['size']/1000000, 1);
                echo("<tr>");
                if ($i == 0) {
                    $shown_name = $plat['name'];
                    if ($shown_name == $last_plat) $shown_name = '';
                    echo("<td class='platform' rowspan='$bin_count'>".$shown_name
                        ."</td>");
                    $last_plat = $plat['name'];
                }
                echo("<td class='binary'>");
                echo("<div class='filename'>$bin[name]</div>"
                    ."<div class='fileinfo'>"
                    ."<div class='filesize'>$mb_size MB</div>");
                if (!empty($bin['signature'])) {
                    echo("<a class='signature' href='".DENG_API_URL."/builds?signature=$bin[name]'>PGP Signature &#x21E3;</a>");
                }
                echo("<div class='hash'>MD5: <span class='digits'>$bin[md5]</span></div>");
                echo("</div>\n");
                $mirror_url = sfnet_link($build_info, $bin['name']);
                $fext = "<span class='fext'>".strtoupper(pathinfo($bin['name'], PATHINFO_EXTENSION))."</span>";
                unset($bits);
                if ($plat['cpu_bits'] > 0) {
                    $bits = ' '.$plat['cpu_bits'].'-bit ';
                }
                echo("</td><td><a class='download' href='".DENG_ARCHIVE_URL."/$bin[name]'>$fext$bits<span class='downarrow'>&#x21E3;</span></a>");
                echo("</td><td><a class='mirror' href='$mirror_url'>$fext$bits<span class='downarrow'>&#x21E3;</span></a>");
                echo("</td></tr>\n");
            }
        }
        echo("</table>\n");

        // Change log.
        if (($changes = json_decode($build_info['changes'])) != NULL) {
            if (count($changes->commits)) {
                echo("<h2>Commits</h2>\n");
                $groups = group_commits_by_tag($changes->commits);
                $keys = array_keys($groups);
                natcasesort($keys);
                foreach ($keys as $key) {
                    if (empty($groups[$key])) {
                        continue;
                    }
                    echo("<h3 class='commitgroup'>$key</h3><ul class='commitlist'>\n");
                    foreach ($groups[$key] as $commit) {
                        // Which other groups this commit could belong to?
                        $other_groups = [];
                        foreach ($commit->tags + $commit->guessedTags as $tag) {
                            if ($tag != $key) {
                                $other_groups[] = "<div class='tag'>"
                                    .htmlspecialchars($tag)."</div>";
                            }
                        }
                        $others = join($other_groups);
                        if ($others) {
                            $others = " <div class='other-groups'>$others</div>";
                        }
                        $link = DENG_GIT_URL . $commit->hash;
                        $date = htmlspecialchars(substr($commit->date, 0, 10));
                        $subject = htmlspecialchars($commit->subject);
                        $author = htmlentities($commit->author);
                        $msg = basic_markdown(htmlentities($commit->message));
                        echo("<li><a href='$link'>$date</a> "
                            ."<span class='title'>$subject</span> "
                            ."by <span class='author'>$author</span>".$others
                            ."<blockquote class='message'>$msg</blockquote></li>\n");
                    }
                    echo("</ul>\n");
                }
            }
        }

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

    $all_versions = [];
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
        if (array_key_exists($version, $all_versions)) {
            $all_versions[$version][] = $info;
        }
        else {
            $all_versions += [$version => array($info)];
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

    echo("<h2>Versions</h2><div id='other-versions'>\n");
    foreach ($all_versions as $version => $builds) {
        echo("<div class='version'><h3>$version</h3>"
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
        echo("</div></div>\n");
    }
    echo("</div>\n");

    // Output page footer.
    generate_footer();
}

function show_signature($filename)
{
    $db = db_open();
    $result = db_query($db, "SELECT signature, name FROM ".DB_TABLE_FILES
        ." WHERE name='".$db->real_escape_string($filename)."'");
    $row = $result->fetch_assoc();
    $signature = $row['signature'];
    $db->close();
    if (!empty($signature)) {
        header('Content-Type: application/pgp-signature');
        header('Content-Disposition: attachment; filename='.$row['name'].'.sig');
        echo $signature;
    }
}

//---------------------------------------------------------------------------------------

setlocale(LC_ALL, 'en_US.UTF-8');

if ($_SERVER['REQUEST_METHOD'] == 'GET') {
    if ($filename = $_GET['signature']) {
        show_signature($filename);
        return;
    }
    $number = $_GET['number'];
    $format = $_GET['format'];
    if ($format == 'html') {
        if ($number > 0) {
            generate_build_page((int)$number);
        }
        else {
            generate_build_index_page();
        }
    }
    else if ($format == 'feed') {
        //generate_build_feed();
    }
}
