<?php
session_start();

// File path
$log_file = "../jbmbrooks/private/activity_log.txt";

// Get visitor details
$ip = $_SERVER['REMOTE_ADDR'] ?? 'Unknown';
$ip_parts = explode('.', $ip);
$ip_parts[3] = 'x';
$ip = implode('.', $ip_parts); // Mask last octet
$timestamp = date('Y-m-d H:i:s');
$referrer = $_SERVER['HTTP_REFERER'] ?? 'none';
$referrer = preg_replace('/[\r\n\t]/', '', $referrer); // Sanitize referrer

// Handle session for visit duration
if (!isset($_SESSION['visit_start'])) {
    $_SESSION['visit_start'] = time();
}

// Handle incoming request
$action = $_POST['action'] ?? 'unknown';
$button_id = $_POST['button_id'] ?? 'none';
$username = $_POST['username'] ?? '';
$password = $_POST['password'] ?? '';
$duration = time() - $_SESSION['visit_start'];

// Sanitize inputs to prevent log file corruption
$username = preg_replace('/[\r\n\t]/', '', $username);
$password_hash = $password !== '' ? hash('sha256', $password) : '[empty]';

// Build log entry
$log_entry = "[$timestamp] IP: $ip, Action: $action, Button: $button_id, VisitDuration: {$duration}s, Referrer: $referrer";
if ($button_id === 'login-btn') {
    $log_entry .= ", Username: " . ($username !== '' ? $username : '[empty]') . ", PasswordHash: $password_hash";
}
$log_entry .= PHP_EOL;

// Write to log file
file_put_contents($log_file, $log_entry, FILE_APPEND | LOCK_EX);

// If action is 'end_session', clear visit start time
if ($action === 'end_session') {
    unset($_SESSION['visit_start']);
}

// Return success response
header('Content-Type: application/json');
echo json_encode(['status' => 'success']);
?>