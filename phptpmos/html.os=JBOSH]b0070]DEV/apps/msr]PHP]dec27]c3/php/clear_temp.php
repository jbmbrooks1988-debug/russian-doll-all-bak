<?php
header('Content-Type: application/json');
include_once __DIR__ . '/gamestate_utils.php'; // For write_gamestate, write_wsr_clock, write_settings
include_once __DIR__ . '/debug_log.php'; // Include debug logging utility

function deleteDirectory($dir) {
    if (!file_exists($dir)) {
        return true;
    }
    if (!is_dir($dir)) {
        return unlink($dir);
    }
    foreach (scandir($dir) as $item) {
        if ($item == '.' || $item == '..') {
            continue;
        }
        if (!deleteDirectory($dir . DIRECTORY_SEPARATOR . $item)) {
            return false;
        }
    }
    return rmdir($dir);
}

try {
    write_debug_log("php/clear_temp.php: Starting to clear temporary data and reinitialize core files.");
    // --- Clear/Initialize core data files ---
    // Clear gamestate.txt and initialize with defaults
    write_gamestate(read_gamestate('../data/gamestate.txt'));

    // Clear wsr_clock.txt and initialize with defaults
    write_wsr_clock(read_wsr_clock('../data/wsr_clock.txt'));

    // Clear setting.txt and initialize with defaults
    write_settings(read_settings('../data/setting.txt'));

    // Clear/Create empty active_entity.txt
    file_put_contents('../data/active_entity.txt', '');

    // Clear/Create empty master_ledger.txt
    file_put_contents('../data/master_ledger.txt', '');

    // Clear/Create master_reader.txt with default "0"
    file_put_contents('../data/master_reader.txt', '0');

    // Remove news.txt
    if (file_exists('../data/news.txt')) {
        unlink('../data/news.txt');
    }
    // Remove generated corporations
    if (file_exists('../data/corporations/generated/')) {
        deleteDirectory('../data/corporations/generated/');
    }
    // Remove generated governments
    if (file_exists('../data/governments/generated/')) {
        deleteDirectory('../data/governments/generated/');
    }
    // Remove player data
    if (file_exists('../data/players/')) {
        deleteDirectory('../data/players/');
    }

    echo json_encode(['status' => 'success', 'message' => 'Old game data cleared and reinitialized.']);

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => 'Failed to clear old data: ' . $e->getMessage()]);
}
?>
