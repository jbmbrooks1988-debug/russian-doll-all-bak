// demo-counter.js - Demo Counter App for PyOS-TPM Web
// Uses innerHTML pattern for window content

function createDemoCounterApp() {
    const counterContent = `
        <div class="counter-container" style="padding: 20px; text-align: center; font-family: 'Segoe UI', sans-serif;">
            <h2 style="color: #2c3e50; margin-bottom: 5px;">Demo Counter</h2>
            <p style="color: #7f8c8d; font-size: 13px; margin-top: 0;">A TPM demonstration app</p>
            
            <div class="counter-display" style="
                font-size: 56px;
                color: #3498db;
                margin: 30px 0;
                font-weight: bold;
                font-family: 'Courier New', monospace;
                background: linear-gradient(to bottom, #ecf0f1, #bdc3c7);
                padding: 20px;
                border-radius: 10px;
                box-shadow: inset 0 2px 5px rgba(0,0,0,0.1);
            ">
                <span style="font-size: 24px; color: #7f8c8d;">Count: </span>
                <span id="count-value">0</span>
            </div>
            
            <div class="counter-controls" style="display: flex; gap: 10px; justify-content: center; margin-bottom: 20px;">
                <button id="btn-decrement" style="
                    padding: 12px 24px;
                    font-size: 24px;
                    background: linear-gradient(to bottom, #e74c3c, #c0392b);
                    color: white;
                    border: none;
                    border-radius: 8px;
                    cursor: pointer;
                    transition: all 0.2s;
                    box-shadow: 0 3px 6px rgba(231, 76, 60, 0.3);
                " onmouseover="this.style.transform='translateY(-2px)'" onmouseout="this.style.transform='translateY(0)'">−</button>
                
                <button id="btn-reset" style="
                    padding: 12px 24px;
                    font-size: 14px;
                    background: linear-gradient(to bottom, #95a5a6, #7f8c8d);
                    color: white;
                    border: none;
                    border-radius: 8px;
                    cursor: pointer;
                    transition: all 0.2s;
                    box-shadow: 0 3px 6px rgba(149, 165, 166, 0.3);
                " onmouseover="this.style.transform='translateY(-2px)'" onmouseout="this.style.transform='translateY(0)'">Reset</button>
                
                <button id="btn-increment" style="
                    padding: 12px 24px;
                    font-size: 24px;
                    background: linear-gradient(to bottom, #27ae60, #1e8449);
                    color: white;
                    border: none;
                    border-radius: 8px;
                    cursor: pointer;
                    transition: all 0.2s;
                    box-shadow: 0 3px 6px rgba(39, 174, 96, 0.3);
                " onmouseover="this.style.transform='translateY(-2px)'" onmouseout="this.style.transform='translateY(0)'">+</button>
            </div>
            
            <div class="tpm-status" style="
                display: flex;
                justify-content: space-between;
                padding: 10px 15px;
                background: #f5f6fa;
                border-radius: 5px;
                font-size: 11px;
                color: #7f8c8d;
                margin-bottom: 15px;
            ">
                <span>📊 Piece: counter</span>
                <span>📝 Ledger: active</span>
                <span>💾 History: <span id="history-count">0</span> entries</span>
            </div>
            
            <div class="counter-history" style="
                padding: 10px;
                background: #1a1a2e;
                border-radius: 5px;
                font-size: 11px;
                text-align: left;
                max-height: 120px;
                overflow-y: auto;
                font-family: 'Courier New', monospace;
                color: #00ff00;
            ">
                <div style="color: #00ffff; margin-bottom: 8px; font-weight: bold;">📜 TPM Event Log:</div>
                <div id="history-list"></div>
            </div>
        </div>
        
        <script>
            (function() {
                let localCount = 0;
                const countEl = document.getElementById('count-value');
                const historyEl = document.getElementById('history-list');
                const historyCountEl = document.getElementById('history-count');
                let historyEntries = 0;
                
                function updateDisplay() {
                    countEl.textContent = localCount;
                }
                
                function addToHistory(action, value) {
                    const entry = document.createElement('div');
                    const time = new Date().toLocaleTimeString();
                    entry.innerHTML = '<span style="color: #888;">[' + time + ']</span> ' + action + ': <span style="color: #00ffff;">' + value + '</span>';
                    entry.style.padding = '3px 0';
                    entry.style.borderBottom = '1px solid #333';
                    historyEl.insertBefore(entry, historyEl.firstChild);
                    historyEntries++;
                    historyCountEl.textContent = historyEntries;
                    
                    // Keep only last 20 entries in UI
                    while (historyEl.children.length > 20) {
                        historyEl.removeChild(historyEl.lastChild);
                    }
                }
                
                function tpmUpdate(action, value) {
                    // Update via Piece Manager
                    if (typeof pieceManager !== 'undefined') {
                        pieceManager.updatePieceState('counter', { count: value });
                        pieceManager.fireEvent('counter', action, { newValue: value });
                    }
                    
                    // Log to master ledger
                    if (typeof masterLedger !== 'undefined') {
                        masterLedger.log('StateChange', 'Counter ' + action + ': ' + value, 'counter_plugin');
                    }
                }
                
                // Button event listeners
                document.getElementById('btn-increment').addEventListener('click', () => {
                    localCount++;
                    updateDisplay();
                    addToHistory('Increment', localCount);
                    tpmUpdate('increment', localCount);
                });
                
                document.getElementById('btn-decrement').addEventListener('click', () => {
                    localCount--;
                    updateDisplay();
                    addToHistory('Decrement', localCount);
                    tpmUpdate('decrement', localCount);
                });
                
                document.getElementById('btn-reset').addEventListener('click', () => {
                    localCount = 0;
                    updateDisplay();
                    addToHistory('Reset', localCount);
                    tpmUpdate('reset', localCount);
                });
                
                // Load initial state from piece manager
                if (typeof pieceManager !== 'undefined') {
                    const savedState = pieceManager.getPieceState('counter', 'count');
                    if (savedState !== null) {
                        localCount = savedState;
                        updateDisplay();
                        addToHistory('Loaded', localCount);
                    }
                }
                
                // Initial log entry
                addToHistory('Init', localCount);
            })();
        <\/script>
    `;
    
    windowManager.createWindow('Demo Counter', counterContent, 450, 500, {
        className: 'counter-window'
    });
    
    masterLedger.log('StateChange', 'counter app opened', 'desktop');
}

// Register in the app opener function
if (typeof openApp !== 'undefined') {
    // Already registered via desktop.js
}
