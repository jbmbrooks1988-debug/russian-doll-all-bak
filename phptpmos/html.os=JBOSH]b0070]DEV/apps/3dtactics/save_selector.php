<?php
header('Content-Type: application/json');

$csvFile = '../data/selector_state.csv';

// Get the JSON data from the POST request
$json = file_get_contents('php://input');
$data = json_decode($json, true);

if ($data && isset($data['x']) && isset($data['y']) && isset($data['z'])) {
    if (($handle = fopen($csvFile, 'w')) !== FALSE) {
        // Write the header
        fputcsv($handle, ['x', 'y', 'z']);
        // Write the data
        fputcsv($handle, [$data['x'], $data['y'], $data['z']]);
        fclose($handle);

        echo json_encode(['status' => 'success', 'message' => 'Selector state saved.']);
    } else {
        http_response_code(500);
        echo json_encode(['status' => 'error', 'message' => 'Could not open selector state file for writing.']);
    }
} else {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'Invalid selector data received.']);
}
?>
