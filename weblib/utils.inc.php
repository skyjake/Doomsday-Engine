<?php

require_once('cache.inc.php');

define('BT_UNSTABLE',  0);
define('BT_CANDIDATE', 1);
define('BT_STABLE',    2);

function build_type_text($build_type)
{
    switch ($build_type) {
        case BT_UNSTABLE:
            return 'unstable';
        case BT_CANDIDATE:
            return 'candidate';
        case BT_STABLE:
            return 'stable';
    }
    return '';
}

function build_type_from_text($text)
{
    return ($text == 'stable'? BT_STABLE : ($text == 'candidate'? BT_CANDIDATE : BT_UNSTABLE));    
}

function detect_user_platform()
{
    require_once('Browser.php');
    
    // Check what the browser tells us.
    $browser = new Browser();    
    switch ($browser->getPlatform()) {
        case Browser::PLATFORM_WINDOWS:
            $user_platform = 'windows';
            break;
        case Browser::PLATFORM_APPLE:
        case Browser::PLATFORM_IPHONE:
        case Browser::PLATFORM_IPAD:
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
    return $user_platform;
}

function user_download_platform()
{
    // Assume 64-bit.
    switch (detect_user_platform()) {
        case 'windows': $dl_plat = 'win-x64';         break;
        case 'macx':    $dl_plat = 'mac10_10-x86_64'; break;
        case 'linux':   $dl_plat = 'ubuntu18-x86_64'; break;
        default:        $dl_plat = 'source';          break;
    }
    return $dl_plat;
}

function platform_download_link()
{
    switch (detect_user_platform()) {
        case 'windows': 
            $dl_link = '/windows'; break;
        case 'macx':
            $dl_link = '/macos'; break;
        case 'linux':
            $dl_link = '/linux'; break;
        default:
            $dl_link = '/source'; break;
    }
    return SITE_ROOT.$dl_link;
}

function starts_with($needle, $haystack)
{
     $length = strlen($needle);
     return (substr($haystack, 0, $length) === $needle);
}

function timestamp_from_date($date_text)
{
    $date = date_parse($date_text);
    $ts = gmmktime($date['hour'], $date['minute'], $date['second'],
                   $date['month'], $date['day'], $date['year']);
    return $ts;
}

function reformat_date($date_text)
{
    $date = date_parse($date_text);
    $ts = timestamp_from_date($date_text);
    if (date('Y') != $date['year']) {
        $fmt = '%Y %B %e';
    }
    else {
        $fmt = '%B %e';
    }
    return strftime($fmt, $ts);        
}

function omit_zeroes($version)
{
    $parts = explode(".", $version);
    while (count($parts) > 2 && array_slice($parts, -1)[0] == '0') {
        $parts = array_slice($parts, 0, -1);
    }
    return join('.', $parts);
}

function human_version($version, $build, $release_type)
{    
    return omit_zeroes($version)." ".ucwords($release_type)." [#${build}]";
}

function webfont_loader_header()
{
    return "<link rel=\"preconnect\" href=\"https://fontlibrary.org/face/web-symbols\" crossorigin>
<script type=\"text/javascript\">
    WebFontConfig = {
      custom: {
        families: [ 'WebSymbolsRegular' ],
        urls: [ 'https://fontlibrary.org/face/web-symbols' ]
      },
      timeout: 5000
    };  
    (function(d) {
      var wf = d.createElement('script'), s = d.scripts[0];
      wf.src = 'https://ajax.googleapis.com/ajax/libs/webfont/1.5.18/webfont.js';
      s.parentNode.insertBefore(wf, s);
    })(document);
</script>";
}
