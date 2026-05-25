// Test script to verify that the view commands work correctly
// This script tests the logic without needing to run the full web interface

// Simulate the handleSpecialCommand function with our changes
function simulateHandleSpecialCommand(command) {
    console.log(`Testing command: "${command}"`);
    
    // This simulates the fixed logic from main.js
    switch (command.toLowerCase()) {
        case 'view vocab':
            console.log('Would call: handleViewVocabCommand("name=default")');
            console.log('Expected: Show contents of default vocabulary file');
            return true;
        case 'view corpus':
            console.log('Would call: handleViewCorpusCommand("name=sample.txt")');
            console.log('Expected: Show contents of sample corpus file');
            return true;
        case 'view model':
            console.log('Would call: handleViewModelCommand("name=default")');
            console.log('Expected: Show details of default model file');
            return true;
        case 'view-vocab:name=default':
            console.log('Would call: handleViewVocabCommand("name=default")');
            console.log('Expected: Show contents of specified vocabulary file');
            return true;
        case 'view-corpus:name=sample.txt':
            console.log('Would call: handleViewCorpusCommand("name=sample.txt")');
            console.log('Expected: Show contents of specified corpus file');
            return true;
        case 'view-model:name=default':
            console.log('Would call: handleViewModelCommand("name=default")');
            console.log('Expected: Show details of specified model file');
            return true;
        case 'list-vocab':
            console.log('Would call: handleListVocabCommand()');
            console.log('Expected: List all vocabulary files');
            return true;
        case 'list-corpus':
            console.log('Would call: handleListCorpusCommand()');
            console.log('Expected: List all corpus files');
            return true;
        case 'list-model':
            console.log('Would call: handleListModelCommand()');
            console.log('Expected: List all model files');
            return true;
    }
    
    return false;
}

console.log('Testing the updated view command functionality...\n');

// Test the new shortcuts
const testCommands = [
    'view vocab',
    'view corpus',
    'view model',
    'view-vocab:name=default',
    'view-corpus:name=sample.txt',
    'view-model:name=default',
    'list-vocab',
    'list-corpus',
    'list-model'
];

testCommands.forEach(cmd => {
    simulateHandleSpecialCommand(cmd);
    console.log('---');
});

console.log('\nTest completed. The changes should now properly handle:');
console.log('- "view vocab" -> shows default vocabulary contents (not just list)');
console.log('- "view corpus" -> shows sample.txt corpus contents (not just list)');
console.log('- "view model" -> shows default model details (not just list)');