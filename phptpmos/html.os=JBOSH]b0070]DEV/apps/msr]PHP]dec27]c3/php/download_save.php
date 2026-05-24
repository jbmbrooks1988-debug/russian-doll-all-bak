<?php
// Include helper functions for directory manipulation
// Function to copy directory recursively
function recursive_copy($src, $dst) {
    $dir = opendir($src);
    if (!is_dir($dst)) {
        mkdir($dst, 0777, true);
    }
    while (false !== ($file = readdir($dir))) {
        if (($file != '.') && ($file != '..')) {
            if (is_dir($src . '/' . $file)) {
                recursive_copy($src . '/' . $file, $dst . '/' . $file);
            } else {
                copy($src . '/' . $file, $dst . '/' . $file);
            }
        }
    }
    closedir($dir);
}

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


$save_name = isset($_GET['save_name']) ? preg_replace('/[^a-zA-Z0-9_-]/', '', $_GET['save_name']) : 'MSR_Save';
$final_zip_name = $save_name . '.zip';

$base_data_dir = '../data/';
$temp_working_dir = $base_data_dir . 'temp_save_export_' . uniqid() . '/';
$temp_zip_path = $temp_working_dir . $final_zip_name;

try {
    // 1. Create a temporary directory structure for the current game state
    mkdir($temp_working_dir, 0777, true);

    // 2. Copy all generated game data into this temporary directory
    recursive_copy($base_data_dir . 'corporations/generated/', $temp_working_dir . 'corporations/generated/');
    recursive_copy($base_data_dir . 'governments/generated/', $temp_working_dir . 'governments/generated/');
    recursive_copy($base_data_dir . 'players/', $temp_working_dir . 'players/');
    
    // Copy individual files
    if (file_exists($base_data_dir . 'gamestate.json')) {
        copy($base_data_dir . 'gamestate.json', $temp_working_dir . 'gamestate.json');
    }
    if (file_exists($base_data_dir . 'news.txt')) {
        copy($base_data_dir . 'news.txt', $temp_working_dir . 'news.txt');
    }
    if (file_exists($base_data_dir . 'master_ledger.txt')) {
        copy($base_data_dir . 'master_ledger.txt', $temp_working_dir . 'master_ledger.txt');
    }
     if (file_exists($base_data_dir . 'master_reader.txt')) {
        copy($base_data_dir . 'master_reader.txt', $temp_working_dir . 'master_reader.txt');
    }

    // 3. Create a zip archive of the temporary directory
    $zip = new ZipArchive();
    if ($zip->open($temp_zip_path, ZipArchive::CREATE | ZipArchive::OVERWRITE) === TRUE) {
        $files = new RecursiveIteratorIterator(
            new RecursiveDirectoryIterator($temp_working_dir),
            RecursiveIteratorIterator::LEAVES_ONLY
        );

        foreach ($files as $name => $file) {
            if (!$file->isDir()) {
                $filePath = $file->getRealPath();
                $relativePath = substr($filePath, strlen($temp_working_dir));
                $zip->addFile($filePath, $relativePath);
            }
        }
        $zip->close();
    } else {
        throw new Exception("Could not create zip archive.");
    }

    // 4. Send the zip file to the client for download
    if (file_exists($temp_zip_path)) {
        header('Content-Type: application/zip');
        header('Content-Disposition: attachment; filename="' . $final_zip_name . '"');
        header('Content-Length: ' . filesize($temp_zip_path));
        readfile($temp_zip_path);
    } else {
        throw new Exception("Generated zip file not found for download.");
    }

} catch (Exception $e) {
    // Log the error
    error_log("Download Save Error: " . $e->getMessage());
    // Fallback for user
    header('Content-Type: text/plain');
    echo "Error generating save file: " . $e->getMessage();
} finally {
    // 5. Clean up the temporary directory and the generated zip file on the server
    if (isset($temp_working_dir) && is_dir($temp_working_dir)) {
        delete_directory_recursive($temp_working_dir);
    }
    if (isset($temp_zip_path) && file_exists($temp_zip_path)) {
        unlink($temp_zip_path);
    }
}
?>
