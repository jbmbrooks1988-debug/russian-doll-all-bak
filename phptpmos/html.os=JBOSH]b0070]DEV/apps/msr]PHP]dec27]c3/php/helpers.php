<?php

// --- Helper function to parse a corporation file for analysis ---
function parse_corp_for_analysis($filepath, $ticker) {
    if (!file_exists($filepath)) return null;

    $lines = file($filepath, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    $data = [
        'ticker' => $ticker,
        'name' => $ticker, // Default name to ticker
        'total_assets' => 0.0,
        'total_liabilities' => 0.0,
        'debt_to_equity' => 0.0,
        'shares_outstanding' => 0.0,
        'current_stock_price' => 0.0,
    ];

    foreach ($lines as $line) {
        if (strpos($line, "FINANCIAL PROFILE FOR") !== false) {
             if (preg_match('/FINANCIAL PROFILE FOR\s+(.+?)\s+\(/', $line, $matches)) {
                $data['name'] = trim($matches[1]);
            }
        }
        if (strpos($line, "Total Assets:") !== false) sscanf($line, "Total Assets: %f", $data['total_assets']);
        if (strpos($line, "Total Liabilities:") !== false) sscanf($line, "Total Liabilities: %f", $data['total_liabilities']);
        if (strpos($line, "Debt to Equity Ratio:") !== false) sscanf($line, "Debt to Equity Ratio: %f", $data['debt_to_equity']);
        if (strpos($line, "Shares of Stock Outstanding:") !== false) {
            // Handle "million" suffix and parse as float
            if (preg_match('/Shares of Stock Outstanding:\s+([0-9\.]+)\s*million/', $line, $matches)) {
                $data['shares_outstanding'] = (float)$matches[1];
            }
        }
        if (strpos($line, "Current Stock Price:") !== false) sscanf($line, "Current Stock Price: %f", $data['current_stock_price']);
    }
    return $data;
}

// Add other common helper functions here if needed
