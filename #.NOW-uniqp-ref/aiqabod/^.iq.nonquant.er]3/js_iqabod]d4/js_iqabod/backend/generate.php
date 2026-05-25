<?php
/**
 * Generation functionality for PHP LLM Backend
 */
require_once __DIR__ . '/config.php';
require_once __DIR__ . '/helpers.php';

class LLMGenerator {
    private $vocab;
    private $modelData;
    private $reverseVocab;
    
    public function __construct() {
        $this->vocab = [];
        $this->modelData = [];
        $this->reverseVocab = [];
    }
    
    /**
     * Load model for generation
     */
    public function loadModel($modelName = 'default') {
        $modelPath = MODEL_SAVE_PATH . $modelName . '.json';
        $modelData = loadModelData($modelPath);
        
        $this->vocab = $modelData['vocab'];
        $this->modelData = $modelData['model'];
        $this->reverseVocab = array_flip($this->vocab);
        
        logMessage("Generation model loaded from: $modelPath");
    }
    
    /**
     * Generate text from a prompt
     */
    public function generate($prompt, $maxTokens = GENERATE_MAX_TOKENS, $temperature = GENERATE_TEMPERATURE, $topK = GENERATE_TOP_K, $topP = GENERATE_TOP_P) {
        logMessage("Generating text with prompt: '$prompt'");
        
        // Tokenize the prompt
        $promptTokens = tokenize($prompt);
        $promptIds = tokensToIds($promptTokens, $this->vocab);
        
        // Initialize the generation sequence with prompt tokens
        $sequence = $promptIds;
        
        // Generate tokens one by one
        for ($i = 0; $i < $maxTokens; $i++) {
            // Prepare input sequence (truncate if too long)
            $inputSequence = array_slice($sequence, -CONTEXT_LENGTH);
            
            // Pad sequence if too short
            while (count($inputSequence) < CONTEXT_LENGTH) {
                array_unshift($inputSequence, $this->vocab['<pad>'] ?? 0);
            }
            
            // Get the next token prediction
            $nextTokenId = $this->predictNextToken($inputSequence, $temperature, $topK, $topP);
            
            // Add token to sequence
            $sequence[] = $nextTokenId;
            
            // Stop generation if end-of-sequence token is generated
            if ($nextTokenId === ($this->vocab['<eos>'] ?? 3)) {
                break;
            }
        }
        
        // Convert generated token IDs back to text
        $generatedIds = array_slice($sequence, count($promptIds));
        $generatedTokens = idsToTokens($generatedIds, $this->vocab);
        
        // Join tokens into text
        $result = '';
        foreach ($generatedTokens as $token) {
            if ($token === '<eos>') continue; // Skip end-of-sequence tokens
            
            // Add space before punctuation or special tokens, otherwise just append
            if (in_array($token, ['.', '!', '?', ',', ';', ':'])) {
                $result = rtrim($result) . $token . ' ';
            } else {
                $result .= $token . ' ';
            }
        }
        
        $result = trim($result);
        logMessage("Generated text: '$result'");
        
        return $result;
    }
    
    /**
     * Predict the next token given an input sequence
     */
    private function predictNextToken($inputSequence, $temperature = GENERATE_TEMPERATURE, $topK = GENERATE_TOP_K, $topP = GENERATE_TOP_P) {
        // In a real implementation, this would perform a forward pass through the model
        // to generate logits for the next token, then sample based on those logits
        
        // For this simulation, we'll create a simple probability distribution based on
        // the last token in the sequence and sample from it
        
        // Get the ID of the last token
        $lastTokenId = end($inputSequence);
        
        // Create a simple simulation of logits
        $logits = [];
        for ($i = 0; $i < min(MODEL_VOCAB_SIZE, 100); $i++) { // Limit for performance
            // Base probability
            $logits[$i] = 0.1;
            
            // Boost probability of tokens that frequently follow the last token (simulated)
            if ($i === $lastTokenId) {
                $logits[$i] = 0.5; // Higher probability to continue the same pattern
            } elseif ($i === (($lastTokenId + 1) % MODEL_VOCAB_SIZE)) {
                $logits[$i] = 0.3; // Secondary choice
            }
        }
        
        // Add some randomness
        foreach ($logits as $id => $logit) {
            $logits[$id] += (mt_rand() / mt_getrandmax() - 0.5) * 0.1;
        }
        
        // Normalize logits to probabilities and sample
        $tokenId = sampleFromLogits($logits, $temperature, $topK, $topP);
        
        return $tokenId;
    }
    
    /**
     * Get model info
     */
    public function getModelInfo() {
        return [
            'vocab_size' => count($this->vocab),
            'context_length' => CONTEXT_LENGTH,
            'temperature' => GENERATE_TEMPERATURE,
            'top_k' => GENERATE_TOP_K,
            'top_p' => GENERATE_TOP_P,
            'max_tokens' => GENERATE_MAX_TOKENS
        ];
    }
}

// Example usage when running the script directly
if (basename(__FILE__) === basename($_SERVER['SCRIPT_NAME'])) {
    try {
        // Create generator instance
        $generator = new LLMGenerator();
        
        // Load model
        $generator->loadModel();
        
        // Generate text from a prompt
        $prompt = "The future of artificial intelligence";
        $result = $generator->generate($prompt, 50, GENERATE_TEMPERATURE, GENERATE_TOP_K, GENERATE_TOP_P);
        
        echo "Prompt: $prompt\n";
        echo "Generated: $result\n";
    } catch (Exception $e) {
        echo "Error during generation: " . $e->getMessage() . "\n";
        logMessage("Generation error: " . $e->getMessage(), 'ERROR');
    }
}