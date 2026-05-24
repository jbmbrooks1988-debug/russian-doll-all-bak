<?php

header('Content-Type: application/json');
include_once 'helpers.php'; // Include helper functions
include_once __DIR__ . '/gamestate_utils.php'; // Include gamestate utility functions

// --- Paths ---
$local_corporations_file = '../data/starting_corporations.txt';
$php_generated_corporations_base_path = '../data/corporations/generated/';
$template_path_base = '../data/templates/';

// --- Helper Functions (Ported from C) ---
function rand_float($min, $max) {
    return $min + (mt_rand() / mt_getrandmax()) * ($max - $min);
}

function is_bank($industry) {
    return strtoupper($industry) === 'BANKING';
}

function is_insurance($industry) {
    return strtoupper($industry) === 'INSURANCE';
}

function create_directory($path) {
    if (!is_dir($path)) {
        mkdir($path, 0777, true);
    }
}

function generate_financial_sheet($corp_name, $ticker, $industry, $template_path) {
    global $php_generated_corporations_base_path; // Access global path

    $dir_path = $php_generated_corporations_base_path . $ticker . '/';
    create_directory($dir_path);

    $file_path = $dir_path . $ticker . '.txt';

    $template_content = file_get_contents($template_path);
    if ($template_content === false) {
        throw new Exception("Could not read template file: " . $template_path);
    }

    $new_corp_content = $template_content;

    // --- General Replacements ---
    // Replace Name and Ticker in headers
    $new_corp_content = preg_replace(
        '/FINANCIAL PROFILE FOR\s+[^(\r\n]+?\s+\([^)]+\)/',
        "FINANCIAL PROFILE FOR $corp_name ($ticker)",
        $new_corp_content,
        1
    );
    $new_corp_content = preg_replace(
        '/FINANCIAL BALANCE SHEET:\s+[^(\r\n]+?\s+\([^)]+\)/',
        "FINANCIAL BALANCE SHEET: $corp_name ($ticker)",
        $new_corp_content,
        1
    );
    // Replace Industry Group
    $new_corp_content = preg_replace('/(Industry Group:\s+)([^\n\r]+)/', "$1$industry", $new_corp_content, 1);
    // Replace Controlled by and Wholly owned by (simplified to 'public' for initial setup)
    $new_corp_content = preg_replace('/(Controlled (and actively managed) by:\s+)([^\n\r]+)/', "$1public", $new_corp_content, 1);
    $new_corp_content = preg_replace('/(Wholly owned by:\s+)([^\n\r]+)/', "$1public", $new_corp_content, 1);
    
    // --- Generate Random Financial Data and Replace ---
    if (is_bank($industry)) {
        $free_cash = rand_float(100.0, 3000.0);
        $business_loans = rand_float(500.0, 2000.0);
        $consumer_loans = rand_float(100.0, 500.0);
        $mortgage_loans = rand_float(200.0, 1000.0);
        $bad_debt_reserves = rand_float(-300.0, -100.0);
        $total_assets = $free_cash + $business_loans + $consumer_loans + $mortgage_loans + $bad_debt_reserves;
        $demand_deposits = rand_float(100.0, 500.0);
        $cd = rand_float(500.0, 1500.0);
        $total_liabilities = $demand_deposits + $cd;
        $net_worth = $total_assets - $total_liabilities;
        $shares_outstanding = rand_float(10.0, 100.0);
        $net_worth_per_share = $net_worth / $shares_outstanding;
        $stock_price = $net_worth_per_share * rand_float(0.8, 1.5);
        $week_52_high = $stock_price * rand_float(1.1, 2.0);
        $week_52_low = $stock_price * rand_float(0.5, 0.9);
        $total_cap = $stock_price * $shares_outstanding;
        $bad_debt_reserve_ratio = (-$bad_debt_reserves / ($business_loans + $consumer_loans + $mortgage_loans + 0.01)) * 100.0; // Added 0.01 to prevent division by zero
        $reserves_to_deposits = (-$bad_debt_reserves / ($demand_deposits + $cd + 0.01)) * 100.0; // Added 0.01 to prevent division by zero

        $new_corp_content = preg_replace('/(Free Cash and Equivalents:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $free_cash), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Business Loan Portfolio:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $business_loans), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Consumer Loan Portfolio:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $consumer_loans), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Mortgage Loans\/Securities:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $mortgage_loans), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Less: Bad Debt Reserves:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $bad_debt_reserves), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Total Assets:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $total_assets), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Demand Deposits:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $demand_deposits), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Certificates of Deposit:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $cd), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Total Liabilities:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $total_liabilities), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Equity (Net Worth):\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $net_worth), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Net Worth Per Share:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $net_worth_per_share), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Current Stock Price:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $stock_price), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(52-Week High\/Low Stock Price:\s+)([0-9\.\-]+\s*\/\s*[0-9\.\-]+)/', "$1" . sprintf("%.2f / %.2f", $week_52_high, $week_52_low), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Shares of Stock Outstanding:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $shares_outstanding), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Total Stock Capitalization:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $total_cap), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Bad Debt Reserve Ratio:\s+)([0-9\.\-]+%)/', "$1" . sprintf("%.2f%%", $bad_debt_reserve_ratio), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Reserves to Deposits %:\s+)([0-9\.\-]+%)/', "$1" . sprintf("%.2f%%", $reserves_to_deposits), $new_corp_content, 1);


    } elseif (is_insurance($industry)) {
        $free_cash = rand_float(1000.0, 3000.0);
        $bank_loan = rand_float(0.0, 500.0);
        $insurance_reserves = rand_float(500.0, 1500.0);
        $total_assets = $free_cash;
        $total_liabilities = $bank_loan + $insurance_reserves;
        $net_worth = $total_assets - $total_liabilities;
        $shares_outstanding = rand_float(10.0, 100.0);
        $net_worth_per_share = $net_worth / $shares_outstanding;
        $stock_price = $net_worth_per_share * rand_float(0.8, 1.5);
        $week_52_high = $stock_price * rand_float(1.1, 2.0);
        $week_52_low = $stock_price * rand_float(0.5, 0.9);
        $total_cap = $stock_price * $shares_outstanding;
        $debt_to_equity = $bank_loan / ($net_worth > 0 ? $net_worth : 1.0);

        $new_corp_content = preg_replace('/(Free Cash and Equivalents:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $free_cash), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Total Assets:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $total_assets), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Bank Loan:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $bank_loan), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Insurance Policy Reserves:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $insurance_reserves), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Total Liabilities:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $total_liabilities), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Equity (Net Worth):\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $net_worth), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Net Worth Per Share:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $net_worth_per_share), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Current Stock Price:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $stock_price), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(52-Week High\/Low Stock Price:\s+)([0-9\.\-]+\s*\/\s*[0-9\.\-]+)/', "$1" . sprintf("%.2f / %.2f", $week_52_high, $week_52_low), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Shares of Stock Outstanding:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $shares_outstanding), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Total Stock Capitalization:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $total_cap), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Debt to Equity Ratio:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $debt_to_equity), $new_corp_content, 1);
    
    } else { // General Corporation
        $free_cash = rand_float(100.0, 1000.0);
        $working_capital = rand_float(50.0, 500.0);
        $business_assets = rand_float(500.0, 2000.0);
        $total_assets = $free_cash + $working_capital + $business_assets;
        $bank_loan = rand_float(0.0, 500.0);
        $total_liabilities = $bank_loan;
        $net_worth = $total_assets - $total_liabilities;
        $shares_outstanding = rand_float(10.0, 100.0);
        $net_worth_per_share = $net_worth / $shares_outstanding;
        $stock_price = $net_worth_per_share * rand_float(0.8, 1.5);
        $week_52_high = $stock_price * rand_float(1.1, 2.0);
        $week_52_low = $stock_price * rand_float(0.5, 0.9);
        $total_cap = $stock_price * $shares_outstanding;
        $debt_to_equity = $bank_loan / ($net_worth > 0 ? $net_worth : 1.0);

        $new_corp_content = preg_replace('/(Free Cash and Equivalents:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $free_cash), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Working Capital (A\/R, Inven\.):\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $working_capital), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Business Assets\/Equipment:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $business_assets), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Total Assets:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $total_assets), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Bank Loan:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $bank_loan), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Total Liabilities:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $total_liabilities), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Equity (Net Worth):\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $net_worth), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Net Worth Per Share:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $net_worth_per_share), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Current Stock Price:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $stock_price), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(52-Week High\/Low Stock Price:\s+)([0-9\.\-]+\s*\/\s*[0-9\.\-]+)/', "$1" . sprintf("%.2f / %.2f", $week_52_high, $week_52_low), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Shares of Stock Outstanding:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $shares_outstanding), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Total Stock Capitalization:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $total_cap), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Debt to Equity Ratio:\s+)([0-9\.\-]+)/', "$1" . sprintf("%.2f", $debt_to_equity), $new_corp_content, 1);
        $new_corp_content = preg_replace('/(Accrued R&D and\/or Investment Tax Credits:\s+\$)([0-9\.\-]+)/', "$1" . sprintf("%.2f", rand_float(50.0, 200.0)), $new_corp_content, 1);
    }
    
    file_put_contents($file_path, $new_corp_content);
    
    // Create weights.txt file for corporation
    $risk_appetite = rand(1, 100);
    $weights_content = "risk: " . $risk_appetite . "\n";
    file_put_contents($dir_path . 'weights.txt', $weights_content);
}


// --- Main Setup Logic ---
$response = ['status' => 'error', 'message' => 'An unknown error occurred.'];

try {
    mt_srand(time()); // Seed the random number generator

    if (!file_exists($local_corporations_file)) {
        throw new Exception("Local corporations file not found at " . $local_corporations_file);
    }

    if (!is_dir($php_generated_corporations_base_path)) {
        if (!mkdir($php_generated_corporations_base_path, 0777, true)) {
            throw new Exception("Failed to create generated corporations directory.");
        }
    }

    $corporations_data = file($local_corporations_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);

    if ($corporations_data === false) {
        throw new Exception("Could not read corporations data from " . $local_corporations_file);
    }

    // Clear previous generated data
    array_map('unlink', glob("$php_generated_corporations_base_path*/*.*"));
    array_map('rmdir', glob("$php_generated_corporations_base_path*"));

    foreach ($corporations_data as $line) {
        if (preg_match('/^(.*?) \((.*?)\) Ind: (.*)$/', $line, $matches)) {
            $full_name = trim($matches[1]);
            $ticker = trim($matches[2]);
            $industry = trim($matches[3]);

            $template_file = 'corporation_example.txt';
            if (is_bank($industry)) {
                $template_file = 'bank_example.txt';
            } elseif (is_insurance($industry)) {
                $template_file = 'insurance_example.txt';
            }
            $full_template_path = $template_path_base . $template_file;
            
            generate_financial_sheet($full_name, $ticker, $industry, $full_template_path);
        }
    }
    
    // After all corporations are set up, run the analysis loop equivalent to set initial stock prices
    // This calls the economic logic already present in game_tick.php, but without advancing time
    // We need to simulate a "day passed" for the analysis to run.
    $gamestate = read_gamestate(); // Read from gamestate.txt
    
    // Temporarily set the last_tick_time to force a day passed
    $gamestate['last_tick_time'] = 0; 
    write_gamestate($gamestate); // Save to gamestate.txt

    // Execute game_tick.php directly to run economic analysis
    // This is a direct execution, so no need for fetch. We just want the side effect of updating corp files.
    $game_tick_response = include 'game_tick.php'; 
    
    $response = [
        'status' => 'success', 
        'message' => 'Corporations setup complete. Initial stock prices generated.',
        'game_tick_status' => $game_tick_response['status'] ?? 'unknown',
        'game_tick_message' => $game_tick_response['message'] ?? 'Game tick executed.'
    ];

} catch (Exception $e) {
    http_response_code(500);
    $response = ['status' => 'error', 'message' => $e->getMessage()];
}

echo json_encode($response);

?>