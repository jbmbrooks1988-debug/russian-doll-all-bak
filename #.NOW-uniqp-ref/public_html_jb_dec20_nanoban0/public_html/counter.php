<?php
// File paths
$counter_file = "../jbmbrooks/private/counter.txt";
$log_file = "../jbmbrooks/private/visitor_log.txt";

// Get visitor details
$ip = $_SERVER['REMOTE_ADDR'] ?? 'Unknown';
$ip_parts = explode('.', $ip);
$ip_parts[3] = 'x';
$ip = implode('.', $ip_parts); // Mask last octet
$timestamp = date('Y-m-d H:i:s');
$user_agent = $_SERVER['HTTP_USER_AGENT'] ?? 'Unknown';

// Read and increment counter
$counter = 0;
if (file_exists($counter_file)) {
    $counter = (int)file_get_contents($counter_file);
}
$counter++;
file_put_contents($counter_file, $counter, LOCK_EX);

// Log visitor details
$log_entry = "[$timestamp] IP: $ip, User-Agent: $user_agent" . PHP_EOL;
file_put_contents($log_file, $log_entry, FILE_APPEND | LOCK_EX);

// Output JavaScript to display counter
header('Content-Type: text/javascript');
echo "document.getElementById('visitor-counter').innerText = 'Visitors: $counter';";
?>