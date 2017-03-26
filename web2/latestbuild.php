<?php
// Backwards compatibility for older versions of Doomsday

require_once('include/config.inc.php');
require_once(DENG_API_DIR.'/include/builds.inc.php');

$platform = $_GET['platform'];
if (key_exists('unstable', $_GET)) {
    $type = 'unstable';
}
else {
    $type = 'stable';
} 

generate_platform_latest_json($platform, $type);
