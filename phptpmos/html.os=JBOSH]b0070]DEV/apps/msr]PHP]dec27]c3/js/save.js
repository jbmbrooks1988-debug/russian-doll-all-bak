// js/save.js
// Save/load functionality for Mars Streetrace Raider

// Save game to server
function saveGameToServer() {
    const saveData = {
        player: GameState.player,
        gameTime: GameState.gameTime,
        corporations: GameState.corporations,
        market: GameState.market,
        settings: GameState.settings
    };

    // Try to save to server via PHP
    try {
        fetch('php/save_game.php', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(saveData)
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                console.log('Game saved to server successfully');
                addToOutput('GAME SAVED TO SERVER');
            } else {
                console.error('Error saving game:', data.message);
                addToOutput('ERROR SAVING GAME: ' + data.message);
                // Fallback to local save
                localStorage.setItem('msrSave', JSON.stringify(saveData));
                addToOutput('GAME SAVED LOCALLY');
            }
        })
        .catch(error => {
            console.error('Connection error:', error);
            addToOutput('CONNECTION ERROR: ' + error.message);
            // Fallback to local save
            localStorage.setItem('msrSave', JSON.stringify(saveData));
            addToOutput('GAME SAVED LOCALLY');
        });
    } catch (e) {
        console.error('Save error:', e);
        addToOutput('SAVE ERROR: ' + e.message);
    }
}

// Load game from server
function loadGameFromServer(playerName) {
    const url = playerName ? `php/load_game.php?player=${encodeURIComponent(playerName)}` : 'php/load_game.php';
    
    fetch(url)
        .then(response => response.json())
        .then(data => {
            if (data.success === false) {
                console.error('Error loading game:', data.message);
                addToOutput('ERROR LOADING GAME: ' + data.message);
                
                // Try to load from local storage as fallback
                loadGameFromLocalStorage();
            } else {
                // Load the game data
                GameState.player = data.player;
                GameState.gameTime = data.gameTime;
                GameState.corporations = data.corporations;
                GameState.market = data.market;
                GameState.settings = data.settings;
                
                showScreen('game-screen');
                updateGameDisplay();
                addToOutput('GAME LOADED FROM SERVER');
            }
        })
        .catch(error => {
            console.error('Connection error:', error);
            addToOutput('CONNECTION ERROR: ' + error.message);
            
            // Try to load from local storage as fallback
            loadGameFromLocalStorage();
        });
}

// Save game to local storage
function saveGameToLocalStorage() {
    const saveData = {
        player: GameState.player,
        gameTime: GameState.gameTime,
        corporations: GameState.corporations,
        market: GameState.market,
        settings: GameState.settings
    };
    
    try {
        localStorage.setItem('msrSave', JSON.stringify(saveData));
        console.log('Game saved to local storage');
        addToOutput('GAME SAVED LOCALLY');
    } catch (e) {
        console.error('Error saving to local storage:', e);
        addToOutput('ERROR SAVING LOCALLY: ' + e.message);
    }
}

// Load game from local storage
function loadGameFromLocalStorage() {
    const saveData = localStorage.getItem('msrSave');
    if (saveData) {
        try {
            const data = JSON.parse(saveData);
            GameState.player = data.player;
            GameState.gameTime = data.gameTime;
            GameState.corporations = data.corporations;
            GameState.market = data.market;
            GameState.settings = data.settings;
            
            showScreen('game-screen');
            updateGameDisplay();
            addToOutput('GAME LOADED FROM LOCAL STORAGE');
        } catch (e) {
            console.error('Error loading from local storage:', e);
            addToOutput('ERROR LOADING FROM LOCAL STORAGE: ' + e.message);
        }
    } else {
        addToOutput('NO LOCAL SAVE FOUND');
    }
}

// Export save file for download
function exportSaveFile() {
    const saveData = {
        player: GameState.player,
        gameTime: GameState.gameTime,
        corporations: GameState.corporations,
        market: GameState.market,
        settings: GameState.settings
    };
    
    const dataStr = JSON.stringify(saveData, null, 2);
    const dataUri = 'data:application/json;charset=utf-8,'+ encodeURIComponent(dataStr);
    
    const exportFileDefaultName = 'msr_save_' + GameState.player.name + '_' + new Date().toISOString().slice(0, 10) + '.json';
    
    const linkElement = document.createElement('a');
    linkElement.setAttribute('href', dataUri);
    linkElement.setAttribute('download', exportFileDefaultName);
    linkElement.click();
    
    addToOutput('SAVE FILE EXPORTED');
}

// Import save file from user selection
function importSaveFile(file) {
    const reader = new FileReader();
    reader.onload = function(e) {
        try {
            const saveData = JSON.parse(e.target.result);
            GameState.player = saveData.player;
            GameState.gameTime = saveData.gameTime;
            GameState.corporations = saveData.corporations;
            GameState.market = saveData.market;
            GameState.settings = saveData.settings;
            
            showScreen('game-screen');
            updateGameDisplay();
            addToOutput('GAME LOADED FROM IMPORTED FILE');
        } catch (err) {
            console.error('Error loading imported save:', err);
            addToOutput('ERROR LOADING IMPORTED FILE: ' + err.message);
        }
    };
    reader.readAsText(file);
}

// Clear temporary save files on server
function clearTempSaves() {
    fetch('php/clear_temp.php')
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                console.log(`Cleared ${data.deleted_count} temporary files`);
                addToOutput(`CLEARED ${data.deleted_count} TEMPORARY FILES`);
            } else {
                console.error('Error clearing temp files:', data.message);
                addToOutput('ERROR CLEARING TEMP FILES: ' + data.message);
            }
        })
        .catch(error => {
            console.error('Connection error:', error);
            addToOutput('CONNECTION ERROR: ' + error.message);
        });
}