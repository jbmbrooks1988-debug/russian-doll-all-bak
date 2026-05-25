<?php
// PHP Built-in Server Router

// Get the request URI and method
$uri = $_SERVER['REQUEST_URI'];
$method = $_SERVER['REQUEST_METHOD'];

// Parse the path from the URI
$path = parse_url($uri, PHP_URL_PATH);

// Check if the requested path is a backend API route
if (preg_match('#^/backend/#', $path)) {
    // Set SCRIPT_NAME to backend/api.php so the routing logic works correctly
    $_SERVER['SCRIPT_NAME'] = '/backend/api.php';
    
    // Calculate the PATH_INFO as the part after '/backend'
    $pathInfo = substr($path, strlen('/backend'));
    if ($pathInfo === false || $pathInfo === '') {
        $pathInfo = '/';
    }
    $_SERVER['PATH_INFO'] = $pathInfo;
    
    // Set PHP_SELF for completeness
    $_SERVER['PHP_SELF'] = '/backend/api.php' . $pathInfo;
    
    // Include the API handler
    if (file_exists(__DIR__ . '/backend/api.php')) {
        require_once __DIR__ . '/backend/api.php';
        exit;
    } else {
        // Return 404 if api.php doesn't exist
        http_response_code(404);
        echo "Backend API handler not found.";
        exit;
    }
}

// For all other requests, serve the static files normally
return false;