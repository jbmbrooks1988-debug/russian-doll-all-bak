// API communication layer for PHP LLM Backend
class ApiClient {
    constructor(baseURL = '/backend') {
        this.baseURL = baseURL;
    }

    // Make an API request
    async request(endpoint, method = 'GET', data = null) {
        const config = {
            method,
            headers: {
                'Content-Type': 'application/json',
            },
        };

        if (data) {
            config.body = JSON.stringify(data);
        }

        try {
            const response = await fetch(`${this.baseURL}/${endpoint}`, config);
            const result = await response.json();

            if (!response.ok) {
                throw new Error(result.message || `API error: ${response.status}`);
            }

            return result;
        } catch (error) {
            console.error(`API request failed to ${endpoint}:`, error);
            throw error;
        }
    }

    // Train the model
    async train(params = {}) {
        return this.request('train', 'POST', params);
    }

    // Generate text
    async generate(params = {}) {
        return this.request('generate', 'POST', params);
    }

    // Get model status
    async getStatus() {
        return this.request('status', 'GET');
    }

    // Get model configuration
    async getConfig() {
        return this.request('config', 'GET');
    }

    // Update model configuration
    async updateConfig(params = {}) {
        return this.request('config', 'POST', params);
    }

    // Create vocabulary
    async createVocab(params = {}) {
        return this.request('vocab', 'POST', params);
    }

    // Get vocabulary information
    async getVocab(vocabName = 'default') {
        return this.request(`vocab?name=${vocabName}`, 'GET');
    }

    // Get corpus file content
    async getCorpusFile(fileName) {
        return this.request(`corpus?name=${fileName}`, 'GET');
    }

    // Get vocabulary information in detail
    async getVocabInfo(vocabName = 'default') {
        return this.request(`vocab-info?name=${vocabName}`, 'GET');
    }

    // Get model information
    async getModelInfo(modelName = 'default') {
        return this.request(`model-info?name=${modelName}`, 'GET');
    }

    // List corpus files
    async listCorpusFiles() {
        return this.request('list-corpus', 'GET');
    }

    // List vocabulary files
    async listVocabFiles() {
        return this.request('list-vocab', 'GET');
    }

    // List model files
    async listModelFiles() {
        return this.request('list-model', 'GET');
    }
}