<?php
header('Content-Type: application/json');

$csvFile = '../../data/pieces.csv';

// Get the JSON data from the POST request
$json = file_get_contents('php://input');
$data = json_decode($json, true);

if ($data && isset($data['pieceId']) && isset($data['newX']) && isset($data['newY']) && isset($data['newZ'])) {
    $pieceId = $data['pieceId'];
    $newX = $data['newX'];
    $newY = $data['newY'];
    $newZ = $data['newZ'];

    // Validate coordinates
    if ($newX < 0 || $newX > 7 || $newY < 0 || $newY > 10 || $newZ < 0 || $newZ > 7) {
        http_response_code(400);
        echo json_encode(['status' => 'error', 'message' => 'Coordinates out of bounds! Valid ranges: X=0-7, Y=0-10, Z=0-7']);
        exit;
    }

    // Load the existing pieces
    $pieces = [];
    if (($handle = fopen($csvFile, 'r')) !== FALSE) {
        // Get the header row
        $headers = fgetcsv($handle, 1000, ',');
        
        // Process each row
        while (($row = fgetcsv($handle, 1000, ',')) !== FALSE) {
            $pieces[] = array_combine($headers, $row);
        }
        fclose($handle);
    } else {
        http_response_code(500);
        echo json_encode(['status' => 'error', 'message' => 'Could not open pieces file for reading.']);
        exit;
    }

    // Find the specific piece by its ID (trim whitespace to be safe)
    $updated = false;
    $updatedPiece = null;
    $targetIndex = -1;
    
    // First, find the index of the piece with the matching ID
    for ($i = 0; $i < count($pieces); $i++) {
        $currentId = isset($pieces[$i]['id']) ? trim($pieces[$i]['id']) : '';
        if ($currentId === $pieceId) {
            $targetIndex = $i;
            $updated = true;
            
            // Update the piece values
            $pieces[$i]['x'] = $newX;
            $pieces[$i]['y'] = $newY;
            $pieces[$i]['z'] = $newZ;
            $updatedPiece = $pieces[$i];
            break;
        }
    }
    
    if (!$updated) {
        http_response_code(404);
        echo json_encode(['status' => 'error', 'message' => 'Piece with specified ID not found.']);
        exit;
    }

    if (!$updated) {
        http_response_code(404);
        echo json_encode(['status' => 'error', 'message' => 'Piece with specified ID not found.']);
        exit;
    }

    // Save the updated pieces back to the CSV file
    if (($handle = fopen($csvFile, 'w')) !== FALSE) {
        // Write the header
        fputcsv($handle, ['type', 'x', 'y', 'z', 'id']);
        
        // Write the data
        foreach ($pieces as $piece) {
            fputcsv($handle, [
                $piece['type'],
                $piece['x'],
                $piece['y'],
                $piece['z'],
                $piece['id']
            ]);
        }
        fclose($handle);

        echo json_encode([
            'status' => 'success', 
            'message' => 'Piece position updated successfully.',
            'piece' => $updatedPiece // Return updated piece data
        ]);
    } else {
        http_response_code(500);
        echo json_encode(['status' => 'error', 'message' => 'Could not open pieces file for writing.']);
    }
} else {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'Invalid data received.']);
}
?>