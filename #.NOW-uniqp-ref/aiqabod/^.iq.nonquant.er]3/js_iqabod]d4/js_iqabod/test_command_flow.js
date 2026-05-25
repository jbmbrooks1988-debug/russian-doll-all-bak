// Test to simulate the exact command flow from user input to parameter parsing
function simulateCommandFlow(userInput) {
    console.log("User input:", userInput);
    
    // This simulates the handleSpecialCommand function
    if (userInput.toLowerCase().startsWith('vocab:')) {
        const vocabCommand = userInput.substring(5).trim(); // Remove 'vocab:' prefix
        console.log("After removing vocab: prefix:", vocabCommand);
        
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

// Test the exact scenario from the task
console.log("=== Testing the problematic scenario ===");
const result = simulateCommandFlow("vocab:source=corpora/sample.txt,vocabName=default");

console.log("\n=== Expected backend parameters ===");
console.log("params.source should be 'corpora/sample.txt':", result && result.source === 'corpora/sample.txt');
console.log("params.vocabName should be 'default':", result && result.vocabName === 'default');
console.log("Both parameters present:", result && result.source && result.vocabName);

// Test some edge cases that might have caused the original issue
console.log("\n=== Testing edge cases ===");

// Test with extra spaces
console.log("\n--- Edge case: Extra spaces ---");
simulateCommandFlow("vocab:  source  =  corpora/sample.txt  ,  vocabName  =  default  ");

// Test with wrong formatting
console.log("\n--- Edge case: Missing equals ---");
simulateCommandFlow("vocab:source, vocabName=default");

// Test with mixed case
console.log("\n--- Edge case: Mixed case ---");
simulateCommandFlow("vocab:SOURCE=corpora/sample.txt,VOCABNAME=default");

// Test empty values
console.log("\n--- Edge case: Empty values ---");
simulateCommandFlow("vocab:source=,vocabName=default");