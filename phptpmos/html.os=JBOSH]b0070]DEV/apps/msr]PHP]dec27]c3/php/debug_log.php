<?php

function write_debug_log($message) {
    $filepath = '../data/debug.log';
    $timestamp = date('Y-m-d H:i:s');
    file_put_contents($filepath, "$timestamp: $message\n", FILE_APPEND);
}

?>