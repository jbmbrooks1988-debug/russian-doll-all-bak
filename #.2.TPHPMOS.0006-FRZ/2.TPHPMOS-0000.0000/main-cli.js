console.log("TPMOS Terminal Initialized");

// Global state
let windowCount = 0;
let activeWindow = null;
let isDragging = false;
let isResizing = false;
let offset = { x: 0, y: 0 };

// Menu State
const menuData = {
    items: ["Project Loader", "App Store (Fondu)", "Status", "GL-OS", "Process Monitor"],
    selectedIndex: 0
};

// Function to create new windows dynamically
window.createWindow = (title, type) => {
    windowCount++;
    const winId = `win${windowCount}`;
    
    const win = document.createElement('div');
    win.className = 'window';
    win.id = winId;
    win.style.top = (50 + (windowCount * 20)) + 'px';
    win.style.left = (50 + (windowCount * 20)) + 'px';
    win.style.width = '450px';
    win.style.height = '400px';
    
    let contentHtml = '';
    if (type === 'terminal') {
        contentHtml = `
            <div class="terminal-output" id="output${windowCount}"></div>
            <div id="nav-prompt${windowCount}" class="nav-prompt">Nav > _</div>
        `;
    } else {
        contentHtml = `<canvas id="canvas${windowCount}"></canvas>`;
    }

    win.innerHTML = `
        <div class="titlebar">
            <span>${title} (${type})</span>
            <div>
                <button onclick="minimize('${winId}')">_</button>
                <button onclick="closeWin('${winId}')">X</button>
            </div>
        </div>
        <div class="content">
            ${contentHtml}
        </div>
        <div class="resize-handle"></div>
    `;
    
    document.getElementById('desktop').appendChild(win);
    
    if (type === 'terminal') {
        renderMenu(windowCount);
    }
};

// Render menu to terminal
function renderMenu(winId) {
    const output = document.getElementById(`output${winId}`);
    let html = `CHTPM+OS Main Menu<br><br>`;
    menuData.items.forEach((item, index) => {
        const indicator = (index === menuData.selectedIndex) ? '[>]' : '[ ]';
        html += `${indicator} ${index + 1}. [${item}]<br>`;
    });
    output.innerHTML = html;
}

// Handle terminal input
window.handleInput = (e, winId) => {
    if (e.key === 'ArrowUp') {
        menuData.selectedIndex = Math.max(0, menuData.selectedIndex - 1);
        renderMenu(winId);
    } else if (e.key === 'ArrowDown') {
        menuData.selectedIndex = Math.min(menuData.items.length - 1, menuData.selectedIndex + 1);
        renderMenu(winId);
    }
    // ... Enter key handling ...
};

// Keyboard listener for terminal navigation
document.addEventListener('keydown', (e) => {
    if (!activeWindow || !activeWindow.querySelector('.terminal-output')) return;
    const winId = activeWindow.id.replace('win', '');
    window.handleInput(e, winId);
});

// Event Delegation for Dragging/Resizing (works on dynamic windows)
document.addEventListener('mousedown', (e) => {
    const win = e.target.closest('.window');
    if (!win) return;

    activeWindow = win;
    // Bring to front
    win.style.zIndex = ++windowCount + 10;
    
    if (e.target.closest('.titlebar')) {
        isDragging = true;
        offset = { x: e.clientX - win.offsetLeft, y: e.clientY - win.offsetTop };
    } else if (e.target.classList.contains('resize-handle')) {
        isResizing = true;
    }
});

document.addEventListener('mousemove', (e) => {
    if (!activeWindow) return;

    if (isDragging) {
        activeWindow.style.left = (e.clientX - offset.x) + 'px';
        activeWindow.style.top = (e.clientY - offset.y) + 'px';
    } else if (isResizing) {
        activeWindow.style.width = (e.clientX - activeWindow.offsetLeft) + 'px';
        activeWindow.style.height = (e.clientY - activeWindow.offsetTop) + 'px';
    }
});

document.addEventListener('mouseup', () => {
    isDragging = false;
    isResizing = false;
    activeWindow = null;
});

window.minimize = (id) => {
    const win = document.getElementById(id);
    const content = win.querySelector('.content');
    content.style.display = (content.style.display === 'none') ? 'block' : 'none';
};

window.closeWin = (id) => {
    document.getElementById(id).remove();
};

// Auto-launch default terminal
window.onload = () => {
    window.createWindow('Terminal', 'terminal');
};
