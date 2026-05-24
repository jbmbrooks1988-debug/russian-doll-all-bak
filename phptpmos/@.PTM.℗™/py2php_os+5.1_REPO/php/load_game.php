<?php
/**
 * load_game.php - Load saved game state for PyOS-TPM Web
 * 
 * GET: ?session_id=default
 * POST: { "session_id": "default" }
 * 
 * Returns: {
 *   "success": true,
 *   "saved_at": "2026-02-17T00:00:00+00:00",
 *   "game_state": {...}
 * }
 */

header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
header('Content-Type: application/json');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

// Get session_id from GET or POST
$sessionId = $_GET['session_id'] ?? 'default';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $input = file_get_contents('php://input');
    $data = json_decode($input, true);
    if (isset($data['session_id'])) {
        $sessionId = $data['session_id'];
    }
}

// Sanitize session_id
$sessionId = preg_replace('/[^a-zA-Z0-9_-]/', '', $sessionId);
if (empty($sessionId)) {
    $sessionId = 'default';
}

$basePath = dirname(__DIR__) . '/saves/';
$saveFile = $basePath . "save_{$sessionId}.json";

if (!file_exists($saveFile)) {
    http_response_code(404);
    echo json_encode([
        'success' => false,
        'error' => 'No save found',
        'session_id' => $sessionId,
        'file' => "save_{$sessionId}.json"
    ]);
    exit;
}

$saveData = json_decode(file_get_contents($saveFile), true);

if (json_last_error() !== JSON_ERROR_NONE) {
    http_response_code(500);
    echo json_encode(['error' => 'Invalid save file', 'details' => json_last_error_msg()]);
    exit;
}

echo json_encode([
    'success' => true,
    'saved_at' => $saveData['saved_at'] ?? 'unknown',
    'game_state' => $saveData['game_state'] ?? null,
    'version' => $saveData['version'] ?? '1.0'
]);
?>
