<?php

require_once('config.inc.php');
require_once(DENG_API_DIR.'/include/builds.inc.php');
    
function generate_page_header($title)
{
    echo("<!DOCTYPE html>\n"
        ."<html lang='en'>\n"
        ."  <head>\n"
        ."  <meta charset='UTF-8'>\n"
        ."  <link href='".SITE_ROOT."/theme/stylesheets/site.css' rel='stylesheet' type='text/css'>\n"
        ."  <title>$title</title>\n"
        ."</head>\n");    
}

function platform_download_link()
{
    switch (detect_user_platform()) {
        case 'windows': 
            $dl_link = SITE_ROOT.'/windows'; break;
        case 'macx':
            $dl_link = SITE_ROOT.'/mac_os'; break;
        case 'linux':
            $dl_link = SITE_ROOT.'/linux'; break;
        default:
            $dl_link = SITE_ROOT.'/source'; break;
    }
    return $dl_link;
}

function generate_download_button()
{
    // Check the latest build.
    $user_platform = detect_user_platform();
    $ckey = cache_key('home', ['dl_button', $user_platform]);
    if (!cache_try_load($ckey)) {
        $dl_link = platform_download_link();
        // Find out the latest stable build.
        $db = Session::get()->database();
        $result = db_query($db, "SELECT version FROM ".DB_TABLE_BUILDS
            ." WHERE type=".BT_STABLE." ORDER BY timestamp DESC LIMIT 1");
        if ($row = $result->fetch_assoc()) {
            $button_label = omit_zeroes($row['version']);
        }
        else {
            $button_label = 'Download';            
        }    
        cache_echo("<a href='$dl_link' class='button' "
            ."title='Download latest stable release'>$button_label <span "
            ."class='downarrow'>&#x21E3;</span></a>");
        cache_store($ckey);
    }            
    cache_dump();    
}
