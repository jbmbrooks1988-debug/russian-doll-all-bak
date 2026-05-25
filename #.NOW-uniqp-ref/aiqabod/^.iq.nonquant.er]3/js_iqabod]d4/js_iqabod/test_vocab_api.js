// Test script for vocab command API
import fetch from 'node-fetch';

async function testVocabCommand() {
    try {
        console.log('Testing vocab command API with fixed parameter parsing...');

        // Test parameters that match the expected format
        const params = {
            source: 'corpora/sample.txt',
            vocabName: 'default'
        };

        console.log('Sending parameters:', params);

        const response = await fetch('http://localhost:8080/backend/vocab', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(params)
        });

        const result = await response.json();
        console.log('Response status:', response.status);
        console.log('Response data:', result);

        if (result.status === 'success') {
            console.log('✅ Vocab command API test PASSED - Vocabulary created successfully!');
        } else {
            console.log('❌ Vocab command API test FAILED -', result.message);
        }
    } catch (error) {
        console.error('❌ Error during API test:', error.message);
    }
}

testVocabCommand();