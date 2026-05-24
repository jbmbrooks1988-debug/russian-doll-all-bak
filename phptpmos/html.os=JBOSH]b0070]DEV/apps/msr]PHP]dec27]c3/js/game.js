// js/game.js
// Main game logic for Mars Streetrace Raider

// Global Game State
const GameState = {
    player: {
        name: '',
        cash: 10000.0,
        portfolio: [] // {ticker, shares, avgPrice}
    },
    gameTime: {
        year: 2025,
        month: 1,
        day: 1,
        turn: 1
    },
    corporations: [],
    market: {
        primeRate: 10.0,
        bloodIndex: 100.0,
        clonePrice: 50.0
    },
    settings: {
        difficulty: 'normal',
        gameLength: 10
    },
    news: [
        'Market opens on Mars. Blood futures stable.',
        'Redplanet Labs announces new clone variant.',
        'Clown gang activity reported in Sector 7.',
        'Solar Empire imposes new tariffs on imports.',
        'Terraforming project shows promising results.',
        'Water scarcity affects multiple settlements.',
        'New mining operation discovered rare elements.',
        'Political tensions rise between Mars colonies.'
    ]
};

// Game initialization
function initGame() {
    console.log('Mars Streetrace Raider initialized');
    loadDefaultCorporations();
}

// Generate default corporations for the Mars market
function loadDefaultCorporations() {
    if (GameState.corporations.length === 0) {
        generateDefaultCorporations();
    }
}

function generateDefaultCorporations() {
    // Mars-themed corporations based on the brainstorm document
    GameState.corporations = [
        {
            ticker: 'RPL',
            name: 'Redplanet Labs',
            industry: 'BIOTECH',
            price: 50.25,
            priceChange: 0,
            week52High: 55.00,
            week52Low: 45.00
        },
        {
            ticker: 'CMF',
            name: 'Clown Mafia Front',
            industry: 'ENTERTAINMENT',
            price: 12.50,
            priceChange: 0,
            week52High: 15.00,
            week52Low: 8.00
        },
        {
            ticker: 'MST',
            name: 'Mars Streetrace Inc',
            industry: 'TRANSPORTATION',
            price: 75.30,
            priceChange: 0,
            week52High: 80.00,
            week52Low: 65.00
        },
        {
            ticker: 'BFS',
            name: 'Blood Futures Systems',
            industry: 'COMMODITIES',
            price: 35.75,
            priceChange: 0,
            week52High: 40.00,
            week52Low: 30.00
        },
        {
            ticker: 'DNC',
            name: 'Dune Buggy Convoy',
            industry: 'LOGISTICS',
            price: 28.90,
            priceChange: 0,
            week52High: 32.00,
            week52Low: 25.00
        },
        {
            ticker: 'SCE',
            name: 'Solar Empire Corp',
            industry: 'ENERGY',
            price: 95.40,
            priceChange: 0,
            week52High: 100.00,
            week52Low: 85.00
        },
        {
            ticker: 'TMC',
            name: 'Terraforming Materials',
            industry: 'CONSTRUCTION',
            price: 42.10,
            priceChange: 0,
            week52High: 46.00,
            week52Low: 38.00
        },
        {
            ticker: 'CLN',
            name: 'Clone Genetics',
            industry: 'BIOTECH',
            price: 68.25,
            priceChange: 0,
            week52High: 72.00,
            week52Low: 60.00
        }
    ];
}

// Calculate portfolio value
function calculatePortfolioValue() {
    let value = 0;
    GameState.player.portfolio.forEach(holding => {
        const corp = GameState.corporations.find(c => c.ticker === holding.ticker);
        if (corp) {
            value += holding.shares * corp.price;
        }
    });
    return value;
}

// Calculate net worth
function calculateNetWorth() {
    return GameState.player.cash + calculatePortfolioValue();
}

// Update market prices based on difficulty and random factors
function updateMarketPrices() {
    GameState.corporations.forEach(corp => {
        // Determine price change based on difficulty
        let volatility = 0.02; // 2% base volatility
        
        if (GameState.settings.difficulty === 'hard') {
            volatility = 0.05; // 5% for hard
        } else if (GameState.settings.difficulty === 'nightmare') {
            volatility = 0.1; // 10% for nightmare
        } else if (GameState.settings.difficulty === 'easy') {
            volatility = 0.01; // 1% for easy
        }
        
        // Calculate random change (-volatility to +volatility)
        const changePercent = (Math.random() * 2 - 1) * volatility;
        const oldPrice = corp.price;
        corp.price = Math.max(0.01, corp.price * (1 + changePercent));
        
        // Calculate price change for display
        corp.priceChange = ((corp.price - oldPrice) / oldPrice) * 100;
        
        // Update 52-week high/low
        if (corp.price > corp.week52High) {
            corp.week52High = corp.price;
        } else if (corp.price < corp.week52Low) {
            corp.week52Low = corp.price;
        }
    });
}

// Advance game time by one turn
function advanceGameTime() {
    GameState.gameTime.day++;
    
    // Advance month and year if needed
    if (GameState.gameTime.day > 30) {
        GameState.gameTime.day = 1;
        GameState.gameTime.month++;
        
        if (GameState.gameTime.month > 12) {
            GameState.gameTime.month = 1;
            GameState.gameTime.year++;
        }
    }
    
    GameState.gameTime.turn++;
    
    // Update market prices
    updateMarketPrices();
    
    // Update news
    const newsIndex = GameState.gameTime.turn % GameState.news.length;
    GameState.currentNews = GameState.news[newsIndex];
    
    // Update market data
    updateMarketData();
}

// Update market parameters
function updateMarketData() {
    // Randomly adjust market parameters based on game state
    const changeFactor = (Math.random() * 0.2) - 0.1; // -10% to +10% change
    GameState.market.primeRate = Math.max(1.0, GameState.market.primeRate * (1 + changeFactor));
    
    // Blood index and clone price also fluctuate
    const bloodChange = (Math.random() * 4) - 2; // -2 to +2 change
    GameState.market.bloodIndex = Math.max(50, GameState.market.bloodIndex + bloodChange);
    
    const cloneChange = (Math.random() * 2) - 1; // -1 to +1 change
    GameState.market.clonePrice = Math.max(10, GameState.market.clonePrice + cloneChange);
}

// Check win/lose conditions
function checkGameConditions() {
    // Check if game should end based on game length
    if (GameState.gameTime.year >= 2025 + GameState.settings.gameLength) {
        return { gameOver: true, reason: 'reached_max_length' };
    }
    
    // Check if player is bankrupt
    if (calculateNetWorth() < 1) {
        return { gameOver: true, reason: 'bankrupt' };
    }
    
    // Check if player reached win condition (e.g., net worth goal based on difficulty)
    const winGoal = GameState.settings.difficulty === 'easy' ? 50000 : 
                   GameState.settings.difficulty === 'hard' ? 200000 : 
                   GameState.settings.difficulty === 'nightmare' ? 500000 : 100000;
                   
    if (calculateNetWorth() >= winGoal) {
        return { gameOver: true, reason: 'reached_goal' };
    }
    
    return { gameOver: false };
}