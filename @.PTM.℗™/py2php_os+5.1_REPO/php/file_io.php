<?php
// file_io.php - File Read/Write API for PyOS-TPM Web
// Handles all file operations for the TPM OS system
// Now with proper CORS headers and error handling

// CORS headers for browser requests
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
header('Content-Type: application/json');

// Handle preflight OPTIONS request
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit(0);
}

// Get the base path (parent directory of php/)
$basePath = dirname(__DIR__) . '/';

// Get action from query string
$action = $_GET['action'] ?? '';

// Log all requests for debugging
error_log("[file_io.php] Action: $action");

switch ($action) {
    case 'test':
        // Simple test endpoint to verify PHP is working
        echo json_encode([
            'success' => true,
            'message' => 'PHP file_io.php is working',
            'timestamp' => date('c'),
            'php_version' => phpversion()
        ]);
        break;
        
    case 'read':
        handleRead($basePath);
        break;
        
    case 'write':
        handleWrite($basePath);
        break;
        
    case 'append':
        handleAppend($basePath);
        break;
        
    case 'list':
        handleList($basePath);
        break;
        
    case 'delete':
        handleDelete($basePath);
        break;
        
    default:
        http_response_code(400);
        echo json_encode(['error' => 'Invalid action. Use: read, write, append, list, or delete']);
}

/**
 * Handle file read operation
 */
function handleRead($basePath) {
    $path = $_GET['path'] ?? '';
    
    if (empty($path)) {
        http_response_code(400);
        echo json_encode(['error' => 'Missing path parameter']);
        return;
    }
    
    // Security: Prevent directory traversal
    $path = sanitizePath($path);
    $fullPath = $basePath . $path;
    
    if (!file_exists($fullPath)) {
        http_response_code(404);
        echo json_encode(['error' => 'File not found: ' . $path]);
        return;
    }
    
    if (!is_readable($fullPath)) {
        http_response_code(403);
        echo json_encode(['error' => 'File not readable: ' . $path]);
        return;
    }
    
    $content = file_get_contents($fullPath);
    $extension = pathinfo($fullPath, PATHINFO_EXTENSION);
    
    // Return JSON directly for .json files, wrapped for others
    if ($extension === 'json') {
        // Validate JSON
        $data = json_decode($content, true);
        if (json_last_error() === JSON_ERROR_NONE) {
            echo $content;
        } else {
            echo json_encode(['content' => $content]);
        }
    } elseif ($extension === 'csv') {
        echo json_encode(['content' => $content, 'format' => 'csv']);
    } else {
        echo json_encode(['content' => $content]);
    }
}

/**
 * Handle file write operation
 */
function handleWrite($basePath) {
    $input = file_get_contents('php://input');
    $data = json_decode($input, true);
    
    if (json_last_error() !== JSON_ERROR_NONE) {
        http_response_code(400);
        echo json_encode(['error' => 'Invalid JSON in request body']);
        return;
    }
    
    $path = $data['path'] ?? '';
    $content = $data['content'] ?? '';
    $format = $data['format'] ?? 'json'; // Support both JSON and key-value format
    
    if (empty($path)) {
        http_response_code(400);
        echo json_encode(['error' => 'Missing path in request body']);
        return;
    }
    
    // Security: Prevent directory traversal
    $path = sanitizePath($path);
    $fullPath = $basePath . $path;
    
    // Ensure directory exists
    $dir = dirname($fullPath);
    if (!is_dir($dir)) {
        if (!mkdir($dir, 0755, true)) {
            http_response_code(500);
            echo json_encode(['error' => 'Failed to create directory: ' . $dir]);
            return;
        }
    }
    
    // Convert JSON state to key-value format if requested (TPM pattern)
    if ($format === 'keyvalue' && is_string($content)) {
        $stateData = json_decode($content, true);
        if (json_last_error() === JSON_ERROR_NONE && isset($stateData['state'])) {
            // Convert to key-value format like Python TPM
            $kvLines = [];
            foreach ($stateData['state'] as $key => $value) {
                $kvLines[] = "$key $value";
            }
            $content = implode("\n", $kvLines) . "\n";
        }
    }
    
    // Write the file
    $result = file_put_contents($fullPath, $content);
    
    if ($result !== false) {
        echo json_encode(['success' => true, 'path' => $path, 'bytes_written' => $result]);
    } else {
        http_response_code(500);
        echo json_encode(['error' => 'Failed to write file: ' . $path]);
    }
}

/**
 * Handle file append operation
 */
function handleAppend($basePath) {
    $input = file_get_contents('php://input');
    $data = json_decode($input, true);
    
    if (json_last_error() !== JSON_ERROR_NONE) {
        http_response_code(400);
        echo json_encode(['error' => 'Invalid JSON in request body']);
        return;
    }
    
    $path = $data['path'] ?? '';
    $entry = $data['data'] ?? null;
    
    if (empty($path)) {
        http_response_code(400);
        echo json_encode(['error' => 'Missing path in request body']);
        return;
    }
    
    if ($entry === null) {
        http_response_code(400);
        echo json_encode(['error' => 'Missing data in request body']);
        return;
    }
    
    // Security: Prevent directory traversal
    $path = sanitizePath($path);
    $fullPath = $basePath . $path;
    
    // Ensure directory exists
    $dir = dirname($fullPath);
    if (!is_dir($dir)) {
        if (!mkdir($dir, 0755, true)) {
            http_response_code(500);
            echo json_encode(['error' => 'Failed to create directory: ' . $dir]);
            return;
        }
    }
    
    // Load existing data if file exists
    $entries = [];
    if (file_exists($fullPath)) {
        $content = file_get_contents($fullPath);
        $existing = json_decode($content, true);
        if (json_last_error() === JSON_ERROR_NONE) {
            $entries = is_array($existing) ? $existing : [$existing];
        }
    }
    
    // Append new entry (or entries)
    if (is_array($entry)) {
        $entries = array_merge($entries, $entry);
    } else {
        $entries[] = $entry;
    }
    
    // Write back
    $result = file_put_contents($fullPath, json_encode($entries, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE));
    
    if ($result !== false) {
        echo json_encode(['success' => true, 'path' => $path, 'total_entries' => count($entries)]);
    } else {
        http_response_code(500);
        echo json_encode(['error' => 'Failed to append to file: ' . $path]);
    }
}

/**
 * Handle directory listing
 */
function handleList($basePath) {
    $path = $_GET['path'] ?? '';
    $recursive = $_GET['recursive'] ?? 'false';
    
    // Security: Prevent directory traversal
    $path = sanitizePath($path);
    $fullPath = $basePath . $path;
    
    if (!is_dir($fullPath)) {
        http_response_code(404);
        echo json_encode(['error' => 'Directory not found: ' . $path]);
        return;
    }
    
    $items = [];
    $iterator = $recursive === 'true' 
        ? new RecursiveIteratorIterator(new RecursiveDirectoryIterator($fullPath))
        : new DirectoryIterator($fullPath);
    
    foreach ($iterator as $item) {
        if ($item->isDot()) continue;
        
        $relativePath = str_replace($basePath, '', $item->getPathname());
        $items[] = [
            'name' => $item->getFilename(),
            'path' => $relativePath,
            'is_dir' => $item->isDir(),
            'size' => $item->isFile() ? $item->getSize() : 0,
            'modified' => $item->getMTime()
        ];
    }
    
    echo json_encode(['items' => $items]);
}

/**
 * Handle file delete operation
 */
function handleDelete($basePath) {
    $input = file_get_contents('php://input');
    $data = json_decode($input, true);
    
    if (json_last_error() !== JSON_ERROR_NONE) {
        http_response_code(400);
        echo json_encode(['error' => 'Invalid JSON in request body']);
        return;
    }
    
    $path = $data['path'] ?? '';
    
    if (empty($path)) {
        http_response_code(400);
        echo json_encode(['error' => 'Missing path in request body']);
        return;
    }
    
    // Security: Prevent directory traversal
    $path = sanitizePath($path);
    $fullPath = $basePath . $path;
    
    if (!file_exists($fullPath)) {
        http_response_code(404);
        echo json_encode(['error' => 'File not found: ' . $path]);
        return;
    }
    
    if (is_dir($fullPath)) {
        $result = rmdir($fullPath);
    } else {
        $result = unlink($fullPath);
    }
    
    if ($result) {
        echo json_encode(['success' => true, 'path' => $path]);
    } else {
        http_response_code(500);
        echo json_encode(['error' => 'Failed to delete: ' . $path]);
    }
}

/**
 * Sanitize path to prevent directory traversal attacks
 */
function sanitizePath($path) {
    // Remove any ../ sequences
    $path = str_replace('..', '', $path);
    
    // Remove leading slashes to prevent absolute path access
    $path = ltrim($path, '/');
    
    // Normalize path separators
    $path = str_replace('\\', '/', $path);
    
    // Remove any null bytes
    $path = str_replace(chr(0), '', $path);
    
    return $path;
}
?>
