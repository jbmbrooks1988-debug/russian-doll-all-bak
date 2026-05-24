<?php
header('Content-Type: application/json');

$base_gov_path = '../data/governments/generated/';
$governments = [];

if (is_dir($base_gov_path)) {
    $gov_dirs = array_filter(scandir($base_gov_path), function($dir) use ($base_gov_path) {
        return is_dir($base_gov_path . $dir) && $dir !== '.' && $dir !== '..';
    });

    foreach ($gov_dirs as $gov_name) {
        $gov_file = $base_gov_path . $gov_name . '/' . $gov_name . '.txt';
        $balance_sheet_file = $base_gov_path . $gov_name . '/' . 'balance_sheet.txt';
        
        $gov_data = [];
        if (file_exists($gov_file)) {
            $lines = file($gov_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
            foreach ($lines as $line) {
                if (strpos($line, 'GDP:') !== false) {
                    $gov_data['gdp'] = (float)filter_var($line, FILTER_SANITIZE_NUMBER_FLOAT, FILTER_FLAG_ALLOW_FRACTION);
                } elseif (strpos($line, 'Population:') !== false) {
                    $gov_data['population'] = (int)filter_var($line, FILTER_SANITIZE_NUMBER_INT);
                }
            }
        }

        $balance_sheet_data = [];
        if (file_exists($balance_sheet_file)) {
            $lines = file($balance_sheet_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
            foreach ($lines as $line) {
                if (strpos($line, "Cash (CD's)..") !== false) {
                    preg_match('/Cash \(CD\'s\)\.\.\s*([\-]?\d+\.?\d*)/', $line, $matches);
                    if (isset($matches[1])) $balance_sheet_data['cash'] = (float)$matches[1];
                }
                if (strpos($line, "Less Debt....") !== false) {
                    preg_match('/Less Debt\.\.\.\.\s*([\-]?\d+\.?\d*)/', $line, $matches);
                    if (isset($matches[1])) $balance_sheet_data['debt'] = abs((float)$matches[1]);
                }
            }
        }
        
        if (!empty($gov_data) || !empty($balance_sheet_data)) {
            $governments[] = array_merge(['name' => str_replace('_', ' ', $gov_name)], $gov_data, $balance_sheet_data);
        }
    }
}

if (empty($governments)) {
    echo json_encode(['error' => 'No government data found or generated.']);
    exit;
}

echo json_encode($governments);
?>