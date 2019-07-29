<?php

// For development: show PHP errors.
ini_set('display_errors', 1);
error_reporting(E_ALL ^ E_NOTICE);

define('SITE_ROOT',     'https://dengine.net');
define('SITE_DATA',     '/home/skyjake/files/sitedata.json');
define('ROOT_PATH',     '/home/skyjake/public_html');

define('DENG_API_URL',      'http://api.dengine.net/1');
define('DENG_MANUAL_URL',   'https://manual.dengine.net');
define('DENG_BLOG_URL',     'https://blog.dengine.net');

define('DENG_LIB_DIR',          '/home/skyjake/public_html/lib');
define('DENG_CACHE_PATH',       '/home/skyjake/public_html/cache/api');
define('DENG_CACHE_MAX_AGE',    14*60);
