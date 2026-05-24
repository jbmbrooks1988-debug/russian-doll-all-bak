<?php
header('Content-Type: application/json');

// Function to delete directory recursively
function delete_directory_recursive($dir) {
    if (!file_exists($dir)) return true;
    if (!is_dir($dir)) return unlink($dir);
    foreach (scandir($dir) as $item) {
        if ($item == '.' || $item == '..') continue;
        if (!delete_directory_recursive($dir . DIRECTORY_SEPARATOR . $item)) return false;
    }
    return rmdir($dir);
}

$base_data_dir = '../data/';
$uploaded_file_path = '';

try {
    // 1. Validate uploaded file
    if (!isset($_FILES['save_file']) || $_FILES['save_file']['error'] !== UPLOAD_ERR_OK) {
        throw new Exception("Error uploading file: " . ($_FILES['save_file']['error'] ?? 'No file provided.'));
    }

    $file_tmp_name = $_FILES['save_file']['tmp_name'];
    $file_name = $_FILES['save_file']['name'];
    $file_type = $_FILES['save_file']['type'];
    $file_size = $_FILES['save_file']['size'];

    // Basic validation
    if ($file_type !== 'application/zip' && $file_type !== 'application/x-zip-compressed') {
        throw new Exception("Invalid file type. Only ZIP files are allowed.");
    }
    if ($file_size > 5 * 1024 * 1024) { // Max 5MB
        throw new Exception("File too large. Maximum 5MB allowed.");
    }

    // Move uploaded file to a temporary location
    $uploaded_file_path = $base_data_dir . 'temp_upload_' . uniqid() . '.zip';
    if (!move_uploaded_file($file_tmp_name, $uploaded_file_path)) {
        throw new Exception("Failed to move uploaded file.");
    }

    // 2. Clear current game data
    if (file_exists($base_data_dir . 'gamestate.json')) {
        unlink($base_data_dir . 'gamestate.json');
    }
    if (file_exists($base_data_dir . 'news.txt')) {
        unlink($base_data_dir . 'news.txt');
    }
     if (file_exists($base_data_dir . 'master_ledger.txt')) {
        unlink($base_data_dir . 'master_ledger.txt');
    }
     if (file_exists($base_data_dir . 'master_reader.txt')) {
        unlink($base_data_dir . 'master_reader.txt');
    }
    delete_directory_recursive($base_data_dir . 'corporations/generated/');
    delete_directory_recursive($base_data_dir . 'governments/generated/');
    delete_directory_recursive($base_data_dir . 'players/');

    // Ensure base directories exist before unzipping
    if (!is_dir($base_data_dir . 'corporations/generated/')) mkdir($base_data_dir . 'corporations/generated/', 0777, true);
    if (!is_dir($base_data_dir . 'governments/generated/')) mkdir($base_data_dir . 'governments/generated/', 0777, true);
    if (!is_dir($base_data_dir . 'players/')) mkdir($base_data_dir . 'players/', 0777, true);

    // 3. Unzip the uploaded file's contents into the ../data/ directory
    $zip = new ZipArchive();
    if ($zip->open($uploaded_file_path) === TRUE) {
        $zip->extractTo($base_data_dir); // Extract contents to the data directory
        $zip->close();
    } else {
        throw new Exception("Could not open zip archive for extraction.");
    }

    echo json_encode(['status' => 'success', 'message' => "Game loaded successfully from $file_name!"]);

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => $e->getMessage()]);
} finally {
    // 4. Clean up the temporary uploaded ZIP file
    if (file_exists($uploaded_file_path)) {
        unlink($uploaded_file_path);
    }
}
?>
