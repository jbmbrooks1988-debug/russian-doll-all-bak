<?php
// File path
$log_file = "../jbmbrooks/private/applications.txt";

// Get visitor details
$ip = $_SERVER['REMOTE_ADDR'] ?? 'Unknown';
$ip_parts = explode('.', $ip);
$ip_parts[3] = 'x';
$ip = implode('.', $ip_parts); // Mask last octet
$timestamp = date('Y-m-d H:i:s');

// Get form data
$name = $_POST['name'] ?? '';
$age = $_POST['age'] ?? '';
$gender = $_POST['gender'] ?? '';
$phone = $_POST['phone'] ?? '';
$address = $_POST['address'] ?? '';
$organization = $_POST['organization'] ?? '';
$comments = $_POST['comments'] ?? '';

// Sanitize inputs to prevent log file corruption
$fields = [$name, $age, $gender, $phone, $address, $organization, $comments];
$fields = array_map(function($field) {
    return preg_replace('/[\r\n\t]/', '', $field);
}, $fields);
list($name, $age, $gender, $phone, $address, $organization, $comments) = $fields;

// Build log entry
$log_entry = "[$timestamp] IP: $ip, Name: $name, Age: $age, Gender: $gender, Phone: $phone, Address: $address, Organization: $organization, Comments: $comments" . PHP_EOL;

// Write to log file
file_put_contents($log_file, $log_entry, FILE_APPEND | LOCK_EX);

// Return success response
header('Content-Type: application/json');
echo json_encode(['status' => 'success']);
?>