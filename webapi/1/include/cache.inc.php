<?php
/*
 * Doomsday Web API: Cache
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

require_once('api_config.inc.php');

$_cache_buf = '';
$_cache_ts = 0;

function cache_clear()
{
    global $_cache_buf;
    global $_cache_ts;
    $_cache_buf = '';
    $_cache_ts = 0;
}

function cache_echo($msg)
{
    global $_cache_buf;
    $_cache_buf .= $msg;
}

function cache_dump()
{
    global $_cache_buf;
    echo($_cache_buf);
}

function cache_timestamp()
{
    global $_cache_ts;
    return $_cache_ts;
}

function cache_key($id, $data)
{
    return "/$id/".urlencode(json_encode($data));
}

function cache_try_load($key, $max_age = DENG_CACHE_MAX_AGE)
{
    $fn = DENG_CACHE_PATH.$key;
    if (!file_exists($fn)) return false;
    $ts = filemtime($fn);
    if ($max_age >= 0 && (time() - $ts) > $max_age) return false; // Too old.
    if (($value = file_get_contents($fn)) === false) return false;
    global $_cache_buf;
    global $_cache_ts;
    $_cache_buf = $value;
    $_cache_ts = $ts;
    return true;
}

function cache_store($key)
{
    global $_cache_buf;
    file_put_contents(DENG_CACHE_PATH.$key, $_cache_buf);
}
