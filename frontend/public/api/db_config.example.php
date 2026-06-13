<?php
// Copy this file to db_config.php on the live host and fill in real
// credentials. db_config.php is .gitignore'd and must never be committed.
// The .htaccess in this directory blocks direct HTTP access to db_config*.php.
return [
    'dsn'  => 'mysql:host=localhost;dbname=psion_analytics;charset=utf8mb4',
    'user' => 'psion_user',
    'pass' => 'REPLACE_ME',
];
