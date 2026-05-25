// Test the parameter parsing logic for vocab command
function testVocabCommandParsing(vocabCommand) {
    // Parse vocabulary parameters from command
    // Expected format: source=path/to/file.txt,vocabName=name
    const params = {};
    
    if (vocabCommand) {
        const pairs = vocabCommand.split(',');
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

// Test cases
console.log("Testing vocab command parameter parsing...\n");

// Test case 1: Standard format
const test1 = "source=corpora/sample.txt,vocabName=default";
const result1 = testVocabCommandParsing(test1);
console.log("Test 1 - Standard format:");
console.log("Input:", test1);
console.log("Output:", result1);
console.log("Expected: { source: 'corpora/sample.txt', vocabName: 'default' }");
console.log("Match:", result1.source === 'corpora/sample.txt' && result1.vocabName === 'default');
console.log("");

// Test case 2: Reversed order
const test2 = "vocabName=test,source=corpora/other.txt";
const result2 = testVocabCommandParsing(test2);
console.log("Test 2 - Reversed order:");
console.log("Input:", test2);
console.log("Output:", result2);
console.log("Expected: { vocabName: 'test', source: 'corpora/other.txt' }");
console.log("Match:", result2.source === 'corpora/other.txt' && result2.vocabName === 'test');
console.log("");

// Test case 3: Extra spaces
const test3 = "source=  corpora/spaced.txt  ,  vocabName = spaced_test ";
const result3 = testVocabCommandParsing(test3);
console.log("Test 3 - With spaces:");
console.log("Input:", test3);
console.log("Output:", result3);
console.log("Expected: { source: 'corpora/spaced.txt', vocabName: 'spaced_test' }");
console.log("Match:", result3.source === 'corpora/spaced.txt' && result3.vocabName === 'spaced_test');
console.log("");

// Test case 4: Additional parameter
const test4 = "source=corpora/extra.txt,vocabName=extra,other=param";
const result4 = testVocabCommandParsing(test4);
console.log("Test 4 - With additional parameter:");
console.log("Input:", test4);
console.log("Output:", result4);
console.log("Expected: { source: 'corpora/extra.txt', vocabName: 'extra', other: 'param' }");
console.log("Match:", result4.source === 'corpora/extra.txt' && result4.vocabName === 'extra' && result4.other === 'param');
console.log("");

console.log("All tests completed!");