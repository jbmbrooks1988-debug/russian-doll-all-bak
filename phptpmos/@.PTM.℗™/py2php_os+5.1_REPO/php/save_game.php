<?php
/**
 * save_game.php - Save complete game state for PyOS-TPM Web
 * 
 * POST: {
 *   "session_id": "default",
 *   "game_state": {
 *     "fuzzpet": {...},
 *     "clock": {...},
 *     "counter": {...}
 *   }
 * }
 * 
 * Returns: {
 *   "success": true,
 *   "saved_at": "2026-02-17T00:00:00+00:00",
 *   "file": "save_default.json"
 * }
 */

header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
header('Content-Type: application/json');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

$input = file_get_contents('php://input');
$data = json_decode($input, true);

if (json_last_error() !== JSON_ERROR_NONE) {
    http_response_code(400);
    echo json_encode(['error' => 'Invalid JSON', 'details' => json_last_error_msg()]);
    exit;
}

$sessionId = $data['session_id'] ?? 'default';
$gameState = $data['game_state'] ?? null;

// Sanitize session_id (prevent directory traversal)
$sessionId = preg_replace('/[^a-zA-Z0-9_-]/', '', $sessionId);
if (empty($sessionId)) {
    $sessionId = 'default';
}

if (!$gameState) {
    http_response_code(400);
    echo json_encode(['error' => 'No game state provided']);
    exit;
}

$basePath = dirname(__DIR__) . '/saves/';

// Create saves directory if it doesn't exist
if (!is_dir($basePath)) {
    if (!mkdir($basePath, 0755, true)) {
        http_response_code(500);
        echo json_encode(['error' => 'Failed to create saves directory']);
        exit;
    }
}

$saveFile = $basePath . "save_{$sessionId}.json";
$saveData = [
    'session_id' => $sessionId,
    'saved_at' => date('c'),
    'game_state' => $gameState,
    'version' => '1.0'
];

$result = file_put_contents($saveFile, json_encode($saveData, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE));

if ($result !== false) {
    echo json_encode([
        'success' => true,
        'saved_at' => $saveData['saved_at'],
        'file' => "save_{$sessionId}.json",
        'path' => $saveFile
    ]);
} else {
    http_response_code(500);
    echo json_encode(['error' => 'Failed to write save file']);
}
?>
