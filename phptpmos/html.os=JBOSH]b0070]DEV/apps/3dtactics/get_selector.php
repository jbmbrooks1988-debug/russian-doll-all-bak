<?php
header('Content-Type: application/json');

$csvFile = '../data/selector_state.csv';
$selector_pos = ['x' => 0, 'z' => 0]; // Default position

if (($handle = fopen($csvFile, 'r')) !== FALSE) {
    // Get the header row
    $headers = fgetcsv($handle, 1000, ',');
    // Get the first data row
    if (($data = fgetcsv($handle, 1000, ',')) !== FALSE) {
        $selector_pos = array_combine($headers, $data);
    }
    fclose($handle);
}

echo json_encode($selector_pos);
?>
