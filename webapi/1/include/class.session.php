<?php

require_once('database.inc.php');

class Session
{
    private static $_instance = NULL;    
    public static function &get()
    {
        if (is_null(self::$_instance)) {
            self::$_instance = new Session();
        }
        return self::$_instance;
    }
    
    private $_db;
    
    private function __construct()
    {
        $this->_db = db_open();        
    }
    
    public function &database() 
    {
        return $this->_db;
    }

    private function __clone() {}
}
