<?php
/*
 * Doomsday Web API: News
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

require_once('include/cache.inc.php');

function fetch_blog_feed($category)
{
    header("Cache-Control: max-age=900");
    header('Content-Type: application/json');
    
    $json_url = "http://dengine.net/blog/category/$category/?json=true&count=3";
    $ckey = cache_key('news', $category);
    if (cache_try_load($ckey)) {
        cache_dump();
        return;
    }
    
    $ch = curl_init(); 
    curl_setopt($ch, CURLOPT_URL, $json_url); 
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1); // as a string
    $output = curl_exec($ch); 
    curl_close($ch);          
    
    cache_echo($output);
    
    cache_dump();
    cache_store($ckey);
}

//---------------------------------------------------------------------------------------

setlocale(LC_ALL, 'en_US.UTF-8');

if ($_SERVER['REQUEST_METHOD'] == 'GET') {
    if ($category = $_GET['category']) {
        if ($category == 'news' || $category == 'dev') {
            fetch_blog_feed($category);
        }
    }
}