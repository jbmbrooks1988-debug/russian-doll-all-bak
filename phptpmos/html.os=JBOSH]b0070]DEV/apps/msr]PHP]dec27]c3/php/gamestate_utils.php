<?php

function read_text_data($filepath) {
    clearstatcache(); // Clear file stat cache to ensure we read the freshest data
    $data = [];
    if (file_exists($filepath)) {
        $lines = file($filepath, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
        foreach ($lines as $line) {
            $parts = explode(':', $line, 2);
            if (count($parts) === 2) {
                $key = trim($parts[0]);
                $value = trim($parts[1]);
                // Handle booleans, integers, and lists
                if (strtolower($value) === 'true') {
                    $data[$key] = true;
                } elseif (strtolower($value) === 'false') {
                    $data[$key] = false;
                } elseif (is_numeric($value) && strpos($value, '.') === false) {
                    $data[$key] = (int)$value;
                } elseif (strpos($value, ',') !== false) {
                    $data[$key] = array_map('trim', explode(',', $value));
                } else {
                    $data[$key] = $value;
                }
            }
        }
    }
    return $data;
}

function write_text_data($filepath, array $data) {
    $content = '';
    foreach ($data as $key => $value) {
        if (is_bool($value)) {
            $content .= "$key:" . ($value ? 'true' : 'false') . "\n";
        } elseif (is_array($value)) {
            $content .= "$key:" . implode(',', $value) . "\n";
        } else {
            $content .= "$key:$value\n";
        }
    }
    file_put_contents($filepath, $content);
    clearstatcache();
}

// Function to read gamestate from gamestate.txt
function read_gamestate($filepath = '../data/gamestate.txt') {
    $gamestate_data = read_text_data($filepath);

    // Ensure 'players' is always an array
    if (isset($gamestate_data['players']) && !is_array($gamestate_data['players'])) {
        $gamestate_data['players'] = [$gamestate_data['players']];
    } elseif (!isset($gamestate_data['players'])) {
        $gamestate_data['players'] = [];
    }

    // Default values if file doesn't exist or is empty
    $defaults = [
        'current_player_index' => 0,
        'players' => [],
        'last_tick_time' => 0,
        // Player-specific ticker_on flags will be dynamically added/read
    ];

    return array_merge($defaults, $gamestate_data);
}

// Function to write gamestate to gamestate.txt
function write_gamestate(array $gamestate, $filepath = '../data/gamestate.txt') {
    write_text_data($filepath, $gamestate);
}

// Function to read settings from setting.txt
function read_settings($filepath = '../data/setting.txt') {
    $settings_data = read_text_data($filepath);

    // Default values if file doesn't exist or is empty
    $defaults = [
        'ticker_speed' => 'sec',
        'currency' => 'usd',
        'cheat_mode' => 'off',
        'suppress_popups' => 'off',
        'suppress_earn_rept' => 'off',
        'autosave' => 'off',
        'excersize_options' => 'no',
        'make_physical_delivery' => 'no',
        'take_physical_delivery' => 'yes',
        'sweep_cash_to_repay_margin' => 'yes',
        'stock_chart_size' => 'small',
        'autoadd_to_streamlist' => 'on',
        'autopilot' => 'off'
    ];

    return array_merge($defaults, $settings_data);
}

// Function to write settings to setting.txt
function write_settings(array $settings, $filepath = '../data/setting.txt') {
    write_text_data($filepath, $settings);
}

// Function to read wsr_clock from wsr_clock.txt
function read_wsr_clock($filepath = '../data/wsr_clock.txt') {
    $clock_data = read_text_data($filepath);

    $defaults = [
        'year' => (int)date('Y'),
        'month' => (int)date('m'),
        'day' => (int)date('d'),
        'hour' => 0,
        'minute' => 0,
        'second' => 0,
        'centisecond' => 0,
    ];

    return array_merge($defaults, $clock_data);
}

// Function to write wsr_clock to wsr_clock.txt
function write_wsr_clock(array $clock_data, $filepath = '../data/wsr_clock.txt') {
    write_text_data($filepath, $clock_data);
}

?>