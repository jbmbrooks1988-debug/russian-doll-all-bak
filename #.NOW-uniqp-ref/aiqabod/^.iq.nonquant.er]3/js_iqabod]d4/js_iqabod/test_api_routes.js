const http = require('http');

// Test script to verify API routes work with the PHP built-in server
const HOST = 'localhost';
// We'll test on a common port, but this should match what your start_server.sh uses
const PORT = process.env.PORT || 8080;

console.log(`Testing API routes on http://${HOST}:${PORT}`);

// Test endpoints
const endpoints = [
    { method: 'GET', path: '/backend/status', description: 'Status endpoint' },
    { method: 'GET', path: '/backend/config', description: 'Config endpoint' },
    { method: 'POST', path: '/backend/vocab', description: 'Vocab endpoint (POST)', data: JSON.stringify({ action: 'list' }) },
    { method: 'GET', path: '/backend/vocab', description: 'Vocab endpoint (GET)' }
];

let testsCompleted = 0;
const totalTests = endpoints.length;

function testEndpoint(endpoint) {
    const options = {
        hostname: HOST,
        port: PORT,
        path: endpoint.path,
        method: endpoint.method,
        headers: {
            'Content-Type': 'application/json',
        }
    };

    const req = http.request(options, (res) => {
        let data = '';
        
        res.on('data', (chunk) => {
            data += chunk;
        });
        
        res.on('end', () => {
            console.log(`\n${endpoint.description}:`);
            console.log(`Status: ${res.statusCode}`);
            console.log(`Response: ${data.substring(0, 200)}${data.length > 200 ? '...' : ''}`);
            
            testsCompleted++;
            if (testsCompleted === totalTests) {
                console.log('\nAll API route tests completed.');
            }
        });
    });
    
    req.on('error', (e) => {
        console.error(`Error testing ${endpoint.path}:`, e.message);
        testsCompleted++;
        if (testsCompleted === totalTests) {
            console.log('\nAll API route tests completed.');
        }
    });
    
    if (endpoint.data) {
        req.write(endpoint.data);
    }
    
    req.end();
}

// Start tests
endpoints.forEach(testEndpoint);