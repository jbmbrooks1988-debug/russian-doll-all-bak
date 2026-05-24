<?php
// bridge.php - Simple File-Based IPC for GoDaddy/Web environments
header('Content-Type: application/json');

$action = $_GET['action'] ?? '';
$path = $_GET['path'] ?? '';
// TPM-Compliant: Dynamic root resolution relative to this script
$root = realpath(__DIR__ . '/../');

if (!$path || strpos(realpath($root . '/' . $path), $root) !== 0) {
    echo json_encode(['error' => 'Invalid path']);
    exit;
}

$full_path = $root . '/' . $path;

function pulseMarker($path, $full_path) {
    if (strpos($path, 'current_frame.txt') !== false || strpos($path, 'history.txt') !== false) {
        $marker = (strpos($path, 'history') !== false ? 'history_changed.txt' : 'frame_changed.txt');
        file_put_contents(dirname($full_path) . '/' . $marker, "X\n", FILE_APPEND);
    }
}

switch ($action) {
    case 'read':
        if (file_exists($full_path)) {
            echo json_encode(['content' => file_get_contents($full_path)]);
        } else {
            echo json_encode(['error' => 'File not found']);
        }
        break;

    case 'write':
        $data = file_get_contents('php://input');
        // Create directory if it doesn't exist
        $dir = dirname($full_path);
        if (!is_dir($dir)) {
            mkdir($dir, 0755, true);
        }
        $f = fopen($full_path, 'w');
        if ($f) {
            flock($f, LOCK_EX);
            fwrite($f, $data);
            fflush($f);
            flock($f, LOCK_UN);
            fclose($f);
            
            pulseMarker($path, $full_path);
            echo json_encode(['status' => 'ok']);
        } else {
            echo json_encode(['error' => 'Write failed']);
        }
        break;

    case 'append':
        $data = file_get_contents('php://input');
        // Create directory if it doesn't exist
        $dir = dirname($full_path);
        if (!is_dir($dir)) {
            mkdir($dir, 0755, true);
        }
        $f = fopen($full_path, 'a');
        if ($f) {
            flock($f, LOCK_EX);
            fwrite($f, $data);
            fflush($f);
            flock($f, LOCK_UN);
            fclose($f);
            
            pulseMarker($path, $full_path);
            echo json_encode(['status' => 'ok']);
        } else {
            echo json_encode(['error' => 'Append failed']);
        }
        break;

    case 'stat':
        if (file_exists($full_path)) {
            echo json_encode(['size' => filesize($full_path), 'mtime' => filemtime($full_path)]);
        } else {
            echo json_encode(['error' => 'File not found']);
        }
        break;

    default:
        echo json_encode(['error' => 'Invalid action']);
}
