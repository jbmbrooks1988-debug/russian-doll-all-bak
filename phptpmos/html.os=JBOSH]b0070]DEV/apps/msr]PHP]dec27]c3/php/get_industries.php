<?php
header('Content-Type: application/json');

$industries_file = '../data/36_industries_wsr.txt';
$industries = [];

if (file_exists($industries_file)) {
    $lines = file($industries_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    foreach ($lines as $line) {
        $parts = explode('.', $line, 2); // Split on the first dot
        if (count($parts) > 1) {
            $industries[] = trim($parts[1]);
        } else {
            $industries[] = trim($line); // Fallback if no dot found
        }
    }
}

echo json_encode($industries);
?>
