// wallet.js - Wallet App for JS CLI

class WalletApp {
    constructor() {
        this.balance = 0;
        this.transactions = [];
    }
    
    init() {
        console.log('Wallet App Initialized');
    }
    
    checkBalance() {
        console.log(`Current balance: ${this.balance}`);
    }
    
    addFunds(amount) {
        this.balance += amount;
        this.transactions.push({type: 'credit', amount: amount, timestamp: Date.now()});
        console.log(`Added ${amount} to wallet. New balance: ${this.balance}`);
    }
    
    makePayment(amount) {
        if (this.balance >= amount) {
            this.balance -= amount;
            this.transactions.push({type: 'debit', amount: amount, timestamp: Date.now()});
            console.log(`Paid ${amount}. Remaining balance: ${this.balance}`);
        } else {
            console.log('Insufficient funds');
        }
    }
}

// Export or initialize depending on environment
if (typeof module !== 'undefined' && module.exports) {
    module.exports = WalletApp;
} else if (typeof window !== 'undefined') {
    window.WalletApp = WalletApp;
}