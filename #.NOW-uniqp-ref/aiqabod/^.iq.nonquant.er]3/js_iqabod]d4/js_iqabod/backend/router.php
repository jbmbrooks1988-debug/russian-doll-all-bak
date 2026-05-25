<?php
// router.php - Routing for PHP backend API
// This file serves as the entry point for all API requests
// All requests are routed through this file to api.php

// Get the requested URI
$requestUri = $_SERVER['REQUEST_URI'];

// Remove the script name and query string from the URI
$scriptName = $_SERVER['SCRIPT_NAME'];
$basePath = dirname($scriptName);
$relativePath = substr($requestUri, strlen($basePath));
if ($relativePath[0] === '/') {
    $relativePath = substr($relativePath, 1);
}

// Include the main API file
require_once __DIR__ . '/api.php';