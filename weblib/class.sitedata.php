<?php

// Requirements:
// - define SITE_DATA (path to data JSON)

class SiteData
{
    private static $_instance = NULL;    
    public static function &get()
    {
        if (is_null(self::$_instance)) {
            self::$_instance = new SiteData();
        }
        return self::$_instance;
    }
    
    private $_data;
    
    private function __construct()
    {
        $this->_data = json_decode(file_get_contents(SITE_DATA), true);        
    }
    
    public function &data() 
    {
        return $this->_data;
    }

    private function __clone() {}
}
