<?php
/**
 * Helper functions for PHP LLM Backend
 */

// Function to load text data from file
function loadTextData($filePath) {
    if (!file_exists($filePath)) {
        throw new Exception("File not found: $filePath");
    }
    
    $content = file_get_contents($filePath);
    if ($content === false) {
        throw new Exception("Could not read file: $filePath");
    }
    
    return $content;
}

// Function to save model data to file
function saveModelData($data, $filePath) {
    $result = file_put_contents($filePath, json_encode($data));
    if ($result === false) {
        throw new Exception("Could not write to file: $filePath");
    }
    
    return true;
}

// Function to load model data from file
function loadModelData($filePath) {
    if (!file_exists($filePath)) {
        throw new Exception("Model file not found: $filePath");
    }
    
    $content = file_get_contents($filePath);
    if ($content === false) {
        throw new Exception("Could not read model file: $filePath");
    }
    
    $data = json_decode($content, true);
    if ($data === null) {
        throw new Exception("Could not decode model file: $filePath");
    }
    
    return $data;
}

// Function to tokenize text
function tokenize($text) {
    // Simple tokenization by splitting on whitespace and common punctuation
    $tokens = preg_split('/\s+|([,.!?;:])/', strtolower($text), -1, PREG_SPLIT_DELIM_CAPTURE | PREG_SPLIT_NO_EMPTY);
    
    // Remove empty tokens and return
    return array_values(array_filter(array_map('trim', $tokens), function($token) {
        return $token !== '';
    }));
}

// Function to convert tokens to IDs
function tokensToIds($tokens, $vocab) {
    $ids = [];
    foreach ($tokens as $token) {
        if (isset($vocab[$token])) {
            $ids[] = $vocab[$token];
        } else {
            $ids[] = $vocab['<unk>'] ?? 0; // Unknown token ID
        }
    }
    return $ids;
}

// Function to convert IDs to tokens
function idsToTokens($ids, $vocab) {
    $reverseVocab = array_flip($vocab);
    $tokens = [];
    foreach ($ids as $id) {
        if (isset($reverseVocab[$id])) {
            $tokens[] = $reverseVocab[$id];
        } else {
            $tokens[] = '<unk>';
        }
    }
    return $tokens;
}

// Function to create vocabulary from text
function createVocabulary($text) {
    $tokens = tokenize($text);
    $uniqueTokens = array_unique($tokens);
    
    // Create vocab with special tokens
    $vocab = [
        '<pad>' => 0,
        '<unk>' => 1,
        '<sos>' => 2,  // Start of sequence
        '<eos>' => 3,  // End of sequence
    ];
    
    // Add unique tokens to vocab
    $nextId = 4;
    foreach ($uniqueTokens as $token) {
        if (!isset($vocab[$token])) {
            $vocab[$token] = $nextId++;
        }
    }
    
    return $vocab;
}

// Function to normalize text
function normalizeText($text) {
    // Convert to lowercase and normalize whitespace
    return preg_replace('/\s+/', ' ', strtolower(trim($text)));
}

// Function to log messages
function logMessage($message, $level = 'INFO') {
    $timestamp = date('Y-m-d H:i:s');
    $logEntry = "[$timestamp] [$level] $message" . PHP_EOL;
    file_put_contents(__DIR__ . '/../logs/app.log', $logEntry, FILE_APPEND | LOCK_EX);
}

// Function to send JSON response
function sendJsonResponse($data, $statusCode = 200) {
    http_response_code($statusCode);
    header('Content-Type: application/json');
    echo json_encode($data);
    exit();
}

// Function to validate request method
function validateRequestMethod($requiredMethod) {
    if ($_SERVER['REQUEST_METHOD'] !== $requiredMethod) {
        sendJsonResponse(['error' => "Method not allowed, expected $requiredMethod"], 405);
    }
}

// Function to get request body as JSON
function getJsonFromBody() {
    $json = file_get_contents('php://input');
    $data = json_decode($json, true);
    
    if (json_last_error() !== JSON_ERROR_NONE) {
        sendJsonResponse(['error' => 'Invalid JSON in request body'], 400);
    }
    
    return $data;
}

// Function to sample from logits with temperature
function sampleFromLogits($logits, $temperature = 1.0, $topK = 0, $topP = 0) {
    // Apply temperature scaling
    $scaledLogits = array_map(function($logit) use ($temperature) {
        return $logit / $temperature;
    }, $logits);
    
    // Apply softmax to get probabilities
    $maxLogit = max($scaledLogits);
    $expLogits = array_map(function($logit) use ($maxLogit) {
        return exp($logit - $maxLogit);
    }, $scaledLogits);
    
    $sumExpLogits = array_sum($expLogits);
    $probs = array_map(function($exp) use ($sumExpLogits) {
        return $exp / $sumExpLogits;
    }, $expLogits);
    
    // Apply top-k filtering if specified
    if ($topK > 0 && $topK < count($probs)) {
        // Get indices and sort by probability
        arsort($probs, SORT_NUMERIC);
        $topKIndices = array_slice(array_keys($probs), 0, $topK, true);
        
        // Zero out probabilities not in top-k
        $filteredProbs = array_fill_keys(array_keys($probs), 0.0);
        foreach ($topKIndices as $idx) {
            $filteredProbs[$idx] = $probs[$idx];
        }
        
        // Renormalize
        $sumFiltered = array_sum($filteredProbs);
        if ($sumFiltered > 0) {
            $probs = array_map(function($prob) use ($sumFiltered) {
                return $prob / $sumFiltered;
            }, $filteredProbs);
        }
    }
    
    // Apply top-p (nucleus) sampling if specified
    if ($topP > 0 && $topP < 1.0) {
        // Sort probabilities in descending order with indices
        arsort($probs, SORT_NUMERIC);
        $cumulativeProb = 0.0;
        $filteredProbs = [];
        
        foreach ($probs as $idx => $prob) {
            $cumulativeProb += $prob;
            $filteredProbs[$idx] = $prob;
            
            if ($cumulativeProb >= $topP) {
                break;
            }
        }
        
        // Renormalize
        $sumFiltered = array_sum($filteredProbs);
        if ($sumFiltered > 0) {
            foreach ($filteredProbs as $idx => $prob) {
                $probs[$idx] = $prob / $sumFiltered;
            }
        } else {
            $probs = array_fill_keys(array_keys($probs), 1.0 / count($probs));
        }
    }
    
    // Sample from the final probability distribution
    $rand = mt_rand() / mt_getrandmax();
    $cumulative = 0.0;
    
    foreach ($probs as $idx => $prob) {
        $cumulative += $prob;
        if ($rand < $cumulative) {
            return $idx;
        }
    }
    
    // Fallback to last index if no match
    return array_key_last($probs);
}