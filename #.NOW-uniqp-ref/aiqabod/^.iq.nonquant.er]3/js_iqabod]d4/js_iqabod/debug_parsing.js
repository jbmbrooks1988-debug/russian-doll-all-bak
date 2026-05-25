// Debug script to test the exact parsing algorithm from main.js
function testHandleVocabCommandParsing(command) {
    // Copy exact logic from main.js handleVocabCommand function
    const params = {};
    
    if (command) {
        const pairs = command.split(',');
        for (const pair of pairs) {
            const [key, value] = pair.split('=');
            if (key && value) {
                const trimmedKey = key.trim();
                const trimmedValue = value.trim();
                
                // Map parameters to expected backend format
                switch(trimmedKey.toLowerCase()) {
                    case 'source':
                        params.source = trimmedValue;
                        break;
                    case 'vocabname':
                        params.vocabName = trimmedValue;
                        break;
                    default:
                        params[trimmedKey] = trimmedValue;
                        break;
                }
            }
        }
    }
    
    return params;
}

// Test the exact scenario from the problem description
const testCommand = "source=corpora/sample.txt,vocabName=default";
const result = testHandleVocabCommandParsing(testCommand);

console.log("Testing command:", `vocab:${testCommand}`);
console.log("Parsed params:", result);
console.log("Source param:", result.source);
console.log("VocabName param:", result.vocabName);
console.log("Source is empty:", !result.source);
console.log("VocabName is empty:", !result.vocabName);

// Expected by backend
console.log("\nExpected by backend:");
console.log("params.source should be 'corpora/sample.txt':", result.source === 'corpora/sample.txt');
console.log("params.vocabName should be 'default':", result.vocabName === 'default');