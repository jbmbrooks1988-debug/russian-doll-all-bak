<?php
header('Content-Type: application/json');

// --- Helper function to parse a corporation file ---
function parse_corporation_file($filepath, $ticker_from_filename) {
    if (!file_exists($filepath)) return null;

    $lines = file($filepath, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    $corp = [
        'name' => $ticker_from_filename,
        'ticker' => $ticker_from_filename,
        'net_worth_per_share' => 0.0,
        'current_stock_price' => 0.0,
        'total_stock_capitalization' => 0.0,
        'long_term_bonds_issued' => 0.0,
        'return_on_equity' => 0,
        'pe_ratio' => 0,
        'dividend_yield' => 0,
        'management_rated' => 'N/A',
        'credit_rating' => 'N/A',
        'analyst_rating' => 'N/A',
    ];

    foreach ($lines as $line) {
        // Extract corporation name
        if (preg_match('/FINANCIAL PROFILE FOR\s+(.+?)\s+\((.*?)\)/', $line, $matches)) {
            $corp['name'] = trim($matches[1]);
        }
        // Extract values
        if (strpos($line, "Net Worth Per Share:") !== false) sscanf($line, "Net Worth Per Share: %lf", $corp['net_worth_per_share']);
        if (strpos($line, "Current Stock Price:") !== false) sscanf($line, "Current Stock Price: %lf", $corp['current_stock_price']);
        if (strpos($line, "Total Stock Capitalization:") !== false) sscanf($line, "Total Stock Capitalization: %lf million", $corp['total_stock_capitalization']);
        if (strpos($line, "Long-Term Bonds Issued:") !== false) sscanf($line, "Long-Term Bonds Issued: %lf", $corp['long_term_bonds_issued']);
    }
    return $corp;
}

// --- Main DB Search Logic ---
$corp_base_path = '../data/corporations/generated/';
$results = [];

// Get search criteria from POST request
$min_market_cap = isset($_POST['min_market_cap']) ? (float)$_POST['min_market_cap'] : 0.0;
$max_market_cap = isset($_POST['max_market_cap']) ? (float)$_POST['max_market_cap'] : PHP_INT_MAX;
$publicly_traded_only = isset($_POST['publicly_traded_only']) && $_POST['publicly_traded_only'] === '1';
$has_bonds_issued = isset($_POST['has_bonds_issued']) && $_POST['has_bonds_issued'] === '1';

try {
    if (!is_dir($corp_base_path)) {
        throw new Exception("Corporations directory not found.");
    }

    $corporation_dirs = scandir($corp_base_path);

    foreach ($corporation_dirs as $corp_ticker) {
        if ($corp_ticker === '.' || $corp_ticker === '..') continue;
        if ($corp_ticker === 'martian_corporations.txt') continue; // Skip this file

        $corp_dir_path = $corp_base_path . $corp_ticker . '/';
        if (!is_dir($corp_dir_path)) continue;

        $corp_file = $corp_dir_path . $corp_ticker . '.txt';
        $corp_data = parse_corporation_file($corp_file, $corp_ticker);

        if ($corp_data) {
            $matches_criteria = true;

            // Apply filters
            if ($corp_data['total_stock_capitalization'] < $min_market_cap || $corp_data['total_stock_capitalization'] > $max_market_cap) {
                $matches_criteria = false;
            }
            if ($publicly_traded_only && $corp_data['total_stock_capitalization'] <= 0) {
                $matches_criteria = false;
            }
            if ($has_bonds_issued && $corp_data['long_term_bonds_issued'] <= 0) {
                $matches_criteria = false;
            }

            if ($matches_criteria) {
                $results[] = $corp_data;
            }
        }
    }

    echo json_encode($results);

} catch (Exception $e) {
    http_response_code(500);
    echo json_encode(['error' => $e->getMessage()]);
}
?>
