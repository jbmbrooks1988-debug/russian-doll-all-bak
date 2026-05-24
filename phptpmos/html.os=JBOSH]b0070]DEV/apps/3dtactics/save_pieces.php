<?php
header('Content-Type: application/json');

$csvFile = '../data/pieces.csv';

// Get the JSON data from the POST request
$json = file_get_contents('php://input');
$data = json_decode($json, true);

if ($data && is_array($data)) {
    if (($handle = fopen($csvFile, 'w')) !== FALSE) {
        // Write the header - consistent with our expected format
        fputcsv($handle, ['type', 'x', 'y', 'z', 'id']);

        // Write the data
        foreach ($data as $piece) {
            // Ensure each piece has an ID - use existing ID or generate new one
            $id = isset($piece['id']) && !empty($piece['id']) ? $piece['id'] : uniqid('piece_');
            fputcsv($handle, [
                isset($piece['type']) ? $piece['type'] : '',
                isset($piece['x']) ? $piece['x'] : '',
                isset($piece['y']) ? $piece['y'] : '',
                isset($piece['z']) ? $piece['z'] : '',
                $id
            ]);
        }
        fclose($handle);

        echo json_encode(['status' => 'success', 'message' => 'Game state saved.']);
    } else {
        http_response_code(500);
        echo json_encode(['status' => 'error', 'message' => 'Could not open file for writing.']);
    }
} else {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'Invalid data received.']);
}
?>
