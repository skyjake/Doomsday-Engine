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

//ini_set('display_errors', 1);
//error_reporting(E_ALL ^ E_NOTICE);

require_once('include/builds.inc.php');

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

function download_file($filename)
{
    $db = db_open();
    $name = $db->real_escape_string($filename);
    $result = db_query($db, "SELECT id, build FROM ".DB_TABLE_FILES." WHERE name='$name'");
    if ($row = $result->fetch_assoc()) {
        // Increment the download counters.
        db_query($db, "UPDATE ".DB_TABLE_FILES." SET dl_total=dl_total+1 "
            ."WHERE id=$row[id]");
        db_query($db, "UPDATE ".DB_TABLE_BUILDS." SET dl_total=dl_total+1 "
            ."WHERE build=$row[build]");
        // Redirect to the archive.
        header('Status: 307 Temporary Redirect');
        header("Location: ".DENG_ARCHIVE_URL."/".$filename);
    }
    else {
        header('Status: 404 Not Found');
    }
    $db->close();
}

function generate_header($page_title)
{
    header('Content-Type: text/html;charset=UTF-8');
    
    echo("<!DOCTYPE html>\n");
    echo("<html lang=\"en\"><head>\n");
    echo("  <meta charset=\"UTF-8\">\n");
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
    require_once('lib/Browser.php');
    
    // Check what the browser tells us.
    $browser = new Browser();    
    switch ($browser->getPlatform()) {
        case Browser::PLATFORM_WINDOWS:
            $user_platform = 'windows';
            break;
        case Browser::PLATFORM_APPLE:
            $user_platform = 'macx';
            break;
        case Browser::PLATFORM_LINUX:
            $user_platform = 'linux';
            break;
        case Browser::PLATFORM_FREEBSD:
        case Browser::PLATFORM_OPENBSD:
        case Browser::PLATFORM_NETBSD:
            $user_platform = 'any';
            break;
        default:
            $user_platform = '';
            break;
    }
        
    $db = db_open();
    $result = db_query($db, "SELECT * FROM ".DB_TABLE_BUILDS." WHERE build=$number");
    if ($row = $result->fetch_assoc()) {
        $build_info = $row;
        $type    = build_type_text($row['type']);
        $version = $row['version'];

        $files = db_build_list_files($db, $number);

        // Output page header.
        generate_header(human_version($version, $number, $type));

        echo("<p class='links'><a href='http://dengine.net/'>dengine.net</a> | <a href='".DENG_API_URL."/builds?format=html'>Autobuilder Index</a> | <a href='".DENG_API_URL."/builds?format=feed'>RSS Feed</a></p>\n");

        echo(db_build_summary($db, $number));

        // Files to download.
        echo("<h2>Downloads</h2>\n"
            ."<table class='downloads'>\n");
        $last_plat = NULL;
        foreach (db_platform_list($db) as $plat) {
            // Find the binaries for this platform.
            $bins = db_build_list_platform_files($db, $number, $plat['id'], FT_BINARY);
            $bin_count = count($bins);
            for ($i = 0; $i < $bin_count; $i++) {
                $bin = $bins[$i];
                $mb_size = number_format($bin['size']/1000000, 1);
                $row_class = '';
                if (isset($last_plat) && $last_plat['os'] != $plat['os']) {
                    $row_class = 'separator';
                }
                echo("<tr class='$row_class'>");
                $cell_class = ($plat['os'] == $user_platform? 'detected' : '');
                if ($i == 0) {
                    $shown_name = $plat['name'];
                    if (isset($last_plat) && $shown_name == $last_plat['name']) {
                        $shown_name = '';
                    }
                    echo("<td class='platform $cell_class' rowspan='$bin_count'>".$shown_name
                        ."</td>");
                    $last_plat = $plat;
                }
                $main_url   = download_link($bin['name']);
                $mirror_url = sfnet_link($build_info['type'], $bin['name']);
                echo("<td class='binary'>");
                echo("<div class='filename'><a href='$main_url'>$bin[name]</a></div>"
                    ."<div class='fileinfo'>"
                    ."<div class='filesize'>$mb_size MB</div>");
                if (!empty($bin['signature'])) {
                    echo("<a class='signature' href='".DENG_API_URL."/builds?signature=$bin[name]'>Sig &#x21E3;</a> ");
                }
                echo("<div class='hash'>MD5: <span class='digits'>$bin[md5]</span></div>");
                echo("</div>\n");
                $fext = "<span class='fext'>"
                    .strtoupper(pathinfo($bin['name'], PATHINFO_EXTENSION))."</span>";
                unset($bits);
                if ($plat['cpu_bits'] > 0) {
                    $bits = ' '.$plat['cpu_bits'].'-bit ';
                }
                echo("</td><td class='button'><a class='download' href='$main_url'>$fext$bits<span class='downarrow'>&#x21E3;</span></a>");
                echo("</td><td class='button'><a class='download mirror' href='$mirror_url'>Mirror <span class='downarrow'>&#x21E3;</span></a>");
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
                        $github_link = DENG_GITHUB_URL . $commit->hash;
                        $trgit_link = DENG_TRACKER_GIT_URL . $commit->hash;
                        $parsed = date_parse(substr($commit->date, 0, 10));
                        
                        $date = strftime('%b %d', mktime(0, 0, 0, $parsed['month'], $parsed['day'])); //htmlspecialchars(substr($commit->date, 0, 10));
                        $subject = htmlspecialchars($commit->subject);
                        $author = htmlentities($commit->author);
                        $msg = basic_markdown(htmlentities($commit->message));
                        echo("<li><a href='$github_link'>$date</a> "
                            ."<span class='title'><a href='$trgit_link'>$subject</a></span> "
                            ."by <span class='author'>$author</span>".$others
                            ."<div class='message'>$msg</div></li>\n");
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
    
    $this_year_ts = mktime(0, 0, 0, 1, 1);

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
        $time_format = ($timestamp > $this_year_ts? '%b %d' : "%b %d '%y");
        $info = array(
            "build"   => $build,
            "type"    => $type,
            "date"    => strftime($time_format, $timestamp),
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
            ."<a href=\"".DENG_API_URL."/builds?number=${build}&amp;format=html\">"
            ."<div class='buildnumber'>$build</div>"
            ."<div class='builddate'>$build_date</div>"
            ."<div class='buildversion'>".omit_zeroes($version)."</div></a></div>\n");
    }
    echo("</div>\n");
    $db->close();

    echo("<h2 id='versions-subtitle'>Versions</h2><div id='other-versions'>\n");
    foreach ($all_versions as $version => $builds) {
        $relnotes_link = DENG_WIKI_URL."/Doomsday_version_".omit_zeroes($version);
        echo("<div class='version'><h3><a href='$relnotes_link'>".omit_zeroes($version)."</a></h3>"
            ."<div class='buildlist'>\n");
        foreach ($builds as $info) {
            $type       = $info['type'];
            $build_date = $info['date'];
            $build      = $info['build'];
            echo("<div class='build $type'>"
                ."<a href=\"".DENG_API_URL."/builds?number=${build}&amp;format=html\">"
                ."<div class='buildnumber'>$build</div>"
                ."<div class='builddate'>$build_date</div>"
                ."<div class='buildversion'>".omit_zeroes($version)."</div></a></div>\n");
        }
        echo("</div></div>\n");
    }
    echo("</div>\n");

    // Output page footer.
    generate_footer();
}

function generate_build_feed()
{
    header('Content-Type: application/rss+xml');
    
    // Check the time of the latest build.
    $db = db_open();
    $result = db_query($db, 'SELECT UNIX_TIMESTAMP(timestamp) FROM '
        .DB_TABLE_BUILDS.' ORDER BY timestamp DESC LIMIT 1');
    if ($result->num_rows == 0) {
        $db->close();
        return;    
    }
    $row = $result->fetch_assoc();
    $latest_ts = gmstrftime(RFC_TIME, $row['UNIX_TIMESTAMP(timestamp)']);        
    $contact = 'skyjake@dengine.net (Jaakko Keränen)';

    // Feed header.
    echo("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        ."<rss version=\"2.0\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n"
        .'<channel>'
        .'<title>Doomsday Engine Builds</title>'
        .'<link>http://dengine.net/</link>'
        .'<atom:link href="'.DENG_API_URL.'/builds?format=feed" rel="self" type="application/rss+xml" />'
        .'<description>Automated builds of the Doomsday Engine</description>'
        .'<language>en-us</language>'
        ."<webMaster>$contact</webMaster>"
        ."<lastBuildDate>$latest_ts</lastBuildDate>"
        .'<generator>'.DENG_API_URL.'/builds</generator>'
        .'<ttl>180</ttl>'); # 3 hours
            
    #allEvents = []
    
    $max_unstable = 10;
    $unstable_count = 0;
    $result = db_query($db, 'SELECT build, type, UNIX_TIMESTAMP(timestamp) FROM '
        .DB_TABLE_BUILDS.' ORDER BY timestamp DESC');
    while ($row = $result->fetch_assoc()) {
        $build_url = DENG_API_URL."/builds?number=$row[build]&amp;format=html";
        $build_ts = gmstrftime(RFC_TIME, $row['UNIX_TIMESTAMP(timestamp)']);
        $summary = db_build_plaintext_summary($db, $row['build']);
        $report = db_build_summary($db, $row['build']);;
        $guid = 'build'.$row['build'];
        if ($row['type'] != BT_STABLE) {
            if (++$unstable_count > $max_unstable) {
                continue;
            }
        }
        echo("\n<item>\n"
            ."<title>Build $row[build]</title>\n"
            ."<link>$build_url</link>\n"
            ."<author>$contact</author>\n"
            ."<pubDate>$build_ts</pubDate>\n"
            ."<atom:summary><![CDATA[$summary]]></atom:summary>\n"
            ."<description><![CDATA[$report<p><a href='$build_url'>Downloads and change log</a></p>]]></description>\n"
            ."<guid isPermaLink=\"false\">$guid</guid>\n"
            ."</item>\n");
    }    
    echo('</channel></rss>');
    $db->close();
}

function generate_platform_latest_json($platform, $build_type)
{
    header('Content-Type: application/json');

    $db = db_open();
    $plat = db_get_platform($db, $platform);
    if (empty($plat)) {
        echo("{}\n");
        $db->close();
        return;
    }
    $type = build_type_from_text($build_type);
    if ($type == BT_CANDIDATE) {
        $type_cond = "b.type!=".BT_UNSTABLE; // can also be stable
    }
    else {
        $type_cond = "b.type=".$type;
    }
    $result = db_query($db, "SELECT f.name, f.size, f.md5, f.build, b.version, b.major, b.minor, b.patch, UNIX_TIMESTAMP(b.timestamp) FROM ".DB_TABLE_FILES
        ." f LEFT JOIN ".DB_TABLE_BUILDS." b ON f.build=b.build "
        ."WHERE $type_cond AND f.plat_id=$plat[id] ORDER BY b.timestamp DESC, f.name "
        ."LIMIT 1");
    $resp = [];        
    if ($row = $result->fetch_assoc()) {
        $filename = $row['name'];
        $build = $row['build'];
        $version = human_version($row['version'], $build, build_type_text($type));
        $plat_name = $plat['name'];
        $bits = $plat['cpu_bits'];
        $date = gmstrftime(RFC_TIME, $row['UNIX_TIMESTAMP(b.timestamp)']);
        if ($bits > 0) {
            $full_title = "Doomsday $version for $plat_name or later (${bits}-bit)";
        }
        else {
            $full_title = "Doomsday $version $plat_name";
        }
        $resp += [
            'build_uniqueid' => (int) $build,
            'build_type' => build_type_text($type),
            'build_startdate' => $date,
            'platform_name' => $platform,
            'title' => "Doomsday ".omit_zeroes($row['version']),
            'fulltitle' => $full_title,
            'version' => omit_zeroes($row['version']),
            'version_major' => (int) $row['major'],           
            'version_minor' => (int) $row['minor'],           
            'version_patch' => (int) $row['patch'],
            'direct_download_uri' => download_link($filename),
            'direct_download_fallback_uri' => sfnet_link($type, $filename),
            'file_size' => (int) $row['size'],
            'file_md5' => $row['md5'],
            'release_changeloguri' => DENG_API_URL."/builds?number=$build&format=html",
            'release_notesuri' => DENG_WIKI_URL."/Doomsday_version_"
                .omit_zeroes($row['version']),
            'release_date' => $date,
            'is_unstable' => ($type == BT_UNSTABLE)
        ];
        echo(json_encode($resp));    
    }
    else {
        echo("{}\n");
    }
    $db->close();
}

//---------------------------------------------------------------------------------------

setlocale(LC_ALL, 'en_US.UTF-8');

if ($_SERVER['REQUEST_METHOD'] == 'GET') {
    if ($filename = $_GET['signature']) {
        show_signature($filename);
        return;
    }
    if ($filename = $_GET['dl']) {
        download_file($filename);
        return;
    }
    if ($latest_for = $_GET['latest_for']) {
        $type = $_GET['type'];
        if (empty($type)) $type = 'stable';
        generate_platform_latest_json($latest_for, $type);
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
        generate_build_feed();
    }
    else if (empty($number) && empty($format)) {
        generate_build_index_page();
    }
}
