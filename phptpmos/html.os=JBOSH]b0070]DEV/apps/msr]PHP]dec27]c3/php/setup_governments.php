<?php

header('Content-Type: application/json');

// Use local data files
const LOCAL_GOV_LIST_FILE = '../data/starting_gov-list.txt';
const PHP_GENERATED_GOV_BASE_PATH = '../data/governments/generated/';
const LOCAL_PRESET_BANK_FILE = '../data/preset_bank.txt';
const LOCAL_FINANCIAL_PROFILE_TEMPLATE = '../data/financial_profile_template]gov.txt';

$response = ['status' => 'error', 'message' => 'An unknown error occurred.'];

function sanitize_filename_gov($name) {
    $sanitized = preg_replace('/[^a-zA-Z0-9\s]/', '', $name);
    $sanitized = str_replace(' ', '_', $sanitized);
    return trim($sanitized, '_');
}

function format_number_php_gov($num) {
    return sprintf("%.4f", $num);
}

try {
    // --- Initialize total_population and total_gdp ---
    $total_population = 0;
    $total_gdp = 0;
    if (!file_exists(LOCAL_PRESET_BANK_FILE)) throw new Exception("Local preset bank file not found.");
    $preset_bank_data = file(LOCAL_PRESET_BANK_FILE, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    foreach ($preset_bank_data as $line) {
        $parts = explode(' ', $line);
        if (count($parts) >= 3 && $parts[0] === 'humanoid_bank') {
            $total_population = (int)$parts[1];
            $total_gdp = (int)$parts[1] * (int)$parts[2];
            break;
        }
    }
    if ($total_population === 0 || $total_gdp === 0) throw new Exception("Could not determine totals from local preset_bank.txt.");

    // --- Read financial_profile_template]gov.txt ---
    if (!file_exists(LOCAL_FINANCIAL_PROFILE_TEMPLATE)) throw new Exception("Local financial profile template not found.");
    $financial_profile_template = file_get_contents(LOCAL_FINANCIAL_PROFILE_TEMPLATE);
    if ($financial_profile_template === false) throw new Exception("Could not read local financial profile template.");

    // --- Create Base Government Directory ---
    if (!is_dir(PHP_GENERATED_GOV_BASE_PATH)) {
        if(!mkdir(PHP_GENERATED_GOV_BASE_PATH, 0777, true)) throw new Exception("Failed to create governments directory.");
    }

    // --- Read starting_gov-list.txt and Process ---
    if (!file_exists(LOCAL_GOV_LIST_FILE)) throw new Exception("Local government list file not found.");
    $gov_list_data = file(LOCAL_GOV_LIST_FILE, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    array_shift($gov_list_data); // Skip header

    foreach ($gov_list_data as $line) {
        $parts = preg_split('/[\t\s]+/', $line, -1, PREG_SPLIT_NO_EMPTY);
        if (count($parts) < 4) continue;

        $rank = array_shift($parts);
        array_pop($parts); // Remove the trailing '0'
        $gdp_share_perc = array_pop($parts);
        $pop_perc = array_pop($parts);
        $country_name = implode(' ', $parts);

        $pop_fraction = (float)str_replace('%', '', $pop_perc) / 100.0;
        $gdp_fraction = (float)str_replace('%', '', $gdp_share_perc) / 100.0;
        
        $gov_population = (int)round($pop_fraction * $total_population);
        $gov_gdp = (int)round($gdp_fraction * $total_gdp);
        if ($gov_population < 1) $gov_population = 1;
        if ($gov_gdp < 1) $gov_gdp = 1;

        $sanitized_country_name = sanitize_filename_gov($country_name);
        $gov_dir = PHP_GENERATED_GOV_BASE_PATH . $sanitized_country_name . '/';
        if (!is_dir($gov_dir)) mkdir($gov_dir, 0777, true);
        
        $gov_data_content = "GOVERNMENT FINANCIAL DATA: " . $country_name . "\n";
        $gov_data_content .= "---------------------------------------------\\n";
        $gov_data_content .= "GDP: " . $gov_gdp . "\n";
        $gov_data_content .= "Population: " . $gov_population . "\n";
        file_put_contents($gov_dir . $sanitized_country_name . '.txt', $gov_data_content);
        
        $cash_equivalents = $gov_gdp * 0.1000;
        $physical_assets = $gov_gdp * 0.0800;
        $total_assets = $cash_equivalents + ($gov_gdp * 0.0500) + ($gov_gdp * 0.0300) + $physical_assets + ($gov_gdp * 0.0700);
        $public_debt = $gov_gdp * 0.2000;
        $net_worth = $total_assets - ($public_debt + ($gov_gdp * 0.0500) + ($gov_gdp * 0.0300) + ($gov_gdp * 0.0100) + ($gov_gdp * 0.0050));
        
        $balance_sheet_content = "My Balance Sheet: " . $country_name . "\n";
        $balance_sheet_content .= "Cash (CD's)..\t\t" . format_number_php_gov($cash_equivalents) . "\n";
        $balance_sheet_content .= "Other Assets....\t\t" . format_number_php_gov($physical_assets) . "\n";
        $balance_sheet_content .= "Total Assets.....\t\t" . format_number_php_gov($total_assets) . "\n";
        $balance_sheet_content .= "Less Debt....\t\t-" . format_number_php_gov($public_debt) . "\n";
        $balance_sheet_content .= "Net Worth........\t\t" . format_number_php_gov($net_worth) . "\n";
        file_put_contents($gov_dir . 'balance_sheet.txt', $balance_sheet_content);
    }
    
    $response = ['status' => 'success', 'message' => 'Governments setup complete.'];

} catch (Exception $e) {
    http_response_code(500);
    $response = ['status' => 'error', 'message' => $e->getMessage()];
}

echo json_encode($response);
?>