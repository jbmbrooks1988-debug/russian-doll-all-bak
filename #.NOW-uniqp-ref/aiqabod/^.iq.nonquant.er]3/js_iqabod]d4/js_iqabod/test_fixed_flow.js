// Test the corrected command flow based on the fixed main.js
function simulateCorrectedCommandFlow(userInput) {
    console.log("User input:", userInput);
    
    // This simulates the CORRECTED handleSpecialCommand function 
    if (userInput.toLowerCase().startsWith('vocab:')) {
        const vocabCommand = userInput.substring(6).trim(); // Remove 'vocab:' prefix (6 characters, not 5)
        console.log("After removing vocab: prefix (6 chars):", vocabCommand);
        
        // This simulates the handleVocabCommand function
        const params = {};
        
        if (vocabCommand) {
            const pairs = vocabCommand.split(',');
            console.log("Pairs after split by comma:", pairs);
            
            for (const pair of pairs) {
                const [key, value] = pair.split('=');
                console.log(`Processing pair: "${pair}", key="${key}", value="${value}"`);
                
                if (key && value) {
                    const trimmedKey = key.trim();
                    const trimmedValue = value.trim();
                    console.log(`Trimmed: key="${trimmedKey}", value="${trimmedValue}"`);
                    
                    // Map parameters to expected backend format
                    switch(trimmedKey.toLowerCase()) {
                        case 'source':
                            params.source = trimmedValue;
                            console.log(`Set params.source to "${trimmedValue}"`);
                            break;
                        case 'vocabname':
                            params.vocabName = trimmedValue;
                            console.log(`Set params.vocabName to "${trimmedValue}"`);
                            break;
                        default:
                            params[trimmedKey] = trimmedValue;
                            console.log(`Set params["${trimmedKey}"] to "${trimmedValue}"`);
                            break;
                    }
                } else {
                    console.log(`Skipping pair because key="${key}", value="${value}"`);
                }
            }
        }
        
        console.log("Final params:", params);
        return params;
    }
    
    return null;
}

// Test the exact scenario from the task after the fix
console.log("=== Testing the FIXED problematic scenario ===");
const result = simulateCorrectedCommandFlow("vocab:source=corpora/sample.txt,vocabName=default");

console.log("\n=== Expected backend parameters ===");
console.log("params.source should be 'corpora/sample.txt':", result && result.source === 'corpora/sample.txt');
console.log("params.vocabName should be 'default':", result && result.vocabName === 'default');
console.log("Both parameters present:", result && result.source && result.vocabName);
console.log("Is source missing (the error condition):", !result || !result.source);