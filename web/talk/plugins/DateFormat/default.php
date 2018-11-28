<?php

// Define the plugin:
$PluginInfo['DateFormat'] = array(
    'Name' => 'Date Format',
    'Description' => 'Customized formatting for timestamps.',
    'Version' => '1.0',
    'Author' => "skyjake",
    'AuthorEmail' => 'skyjake@dengine.net',
    'AuthorUrl' => 'http://skyjake.fi',
    'MobileFriendly' => true
);

class DateFormatPlugin extends Gdn_Plugin
{
}

if (!function_exists('FormatDateCustom')) {
    function FormatDateCustom($Timestamp, $Format) {        
        $this_year_ts = mktime(0, 0, 0, 1, 1);
        $Session = Gdn::Session();    
        if ($Session->IsValid()) {
            $Timestamp    += $Session->User->HourOffset * 3600;
            $this_year_ts += $Session->User->HourOffset * 3600;
        }
        if ($Timestamp >= $this_year_ts) {
            return strftime("%b %e, %H:%M", $Timestamp);
        }
        else {
            return strftime("%Y %b %e", $Timestamp);
        }
    }
}

?>
