<?php
header('Content-Type: application/json');

$csvFile = '../data/pieces.csv';
$pieces = [];

if (($handle = fopen($csvFile, 'r')) !== FALSE) {
    // Get the header row
    $headers = fgetcsv($handle, 1000, ',');

    // Check if 'id' column exists in the headers
    $idColumnExists = in_array('id', $headers);
    
    // Process each row
    while (($data = fgetcsv($handle, 1000, ',')) !== FALSE) {
        $piece = array_combine($headers, $data);
        
        // If the 'id' column doesn't exist or the piece doesn't have an ID, generate one
        if (!$idColumnExists || !isset($piece['id']) || empty($piece['id'])) {
            $piece['id'] = uniqid('piece_');
        }
        
        $pieces[] = $piece;
    }
    fclose($handle);
}

echo json_encode($pieces);
?>
