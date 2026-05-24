// craft.js - 3D Crafting Game for JS CLI - Refactored for Instantiation

class CraftApp {
    constructor() {
        this.gameWindow = null;
        this.isInitialized = false;
        
        // Unique ID for this instance
        this.instanceId = 'craft_instance_' + Date.now() + Math.floor(Math.random() * 1000);

        // Register instance in a global map
        if (!window.appInstances) {
            window.appInstances = {};
        }
        window.appInstances[this.instanceId] = this;
    }

    init() {
        // This is called when the user runs the 'craft' command.
        // By this time, the DOM is guaranteed to be ready.
        this.createGUI();
        this.showGUI();
        // The game logic itself is started from createGUI -> setTimeout -> initCraftGame
    }
    
    createGUI() {
        const craftWindow = document.createElement('div');
        craftWindow.id = `${this.instanceId}-main-window`;
        craftWindow.className = 'terminal-window';
        craftWindow.style.display = 'none';
        craftWindow.style.width = '1000px';
        craftWindow.style.height = '700px';
        
        craftWindow.innerHTML = `
            <div class="title-bar">
                Craft - 3D Building Game
                <button class="close-btn" onclick="window.appInstances['${this.instanceId}'].closeGUI()">×</button>
            </div>
            <div class="terminal-content" style="display: flex; flex-direction: column; height: 100%; overflow: auto;">
                <div style="position: absolute; top: 30px; right: 100px; z-index: 101; background: rgba(0,0,0,0.5); padding: 5px; border-radius: 3px;">
                    <button onclick="window.appInstances['${this.instanceId}'].closeGUI()" style="background: #555; color: white; border: none; padding: 5px 10px; cursor: pointer; border-radius: 3px;">Exit to Terminal</button>
                </div>
                <div id="${this.instanceId}-game-container" style="display: grid; grid-template-areas: 'header header' 'sidebar main'; grid-template-rows: 50px 1fr; grid-template-columns: 15% 85%; height: 100%; width: 100%;">
                    <div id="${this.instanceId}-header" style="grid-area: header; background-color: rgba(0, 0, 0, 0.5); border-bottom: 2px solid white; display: flex; align-items: center; padding-left: 10px;">
                        <button id="${this.instanceId}-view2DBtn" style="background: none; border: 1px solid white; color: white; padding: 5px 10px; margin-right: 10px; cursor: pointer;">2d</button>
                        <button id="${this.instanceId}-view3DBtn" style="background: none; border: 1px solid white; color: white; padding: 5px 10px; margin-right: 10px; cursor: pointer;">3d</button>
                    </div>
                    <div id="${this.instanceId}-sidebar" style="grid-area: sidebar; background-color: rgba(0, 0, 0, 0.5); border-right: 2px solid white; overflow-y: auto; padding: 10px; color: white;">
                        <div class="sidebar-window" style="margin-bottom: 20px;">
                            <h3 style="margin-top: 0; font-size: 16px; border-bottom: 1px solid white; padding-bottom: 5px;">Color Picker</h3>
                            <div id="${this.instanceId}-color-palette" style="display: flex; flex-wrap: wrap; width: 110px; height: 120px; overflow-y: auto; border: 1px solid #555; padding: 2px; margin-top: 10px;"></div>
                            <div id="${this.instanceId}-color-status" style="margin-top: 10px; height: 20px; font-size: 12px;">Select a color!</div>
                        </div>
                    </div>
                    <div id="${this.instanceId}-main" style="grid-area: main; position: relative;">
                        <div id="${this.instanceId}-canvasContainer" style="width: 100%; height: 100%;"></div>
                        <div id="${this.instanceId}-coordinates" style="position: absolute; top: 10px; right: 10px; color: white; font-size: 14px; background-color: rgba(0, 0, 0, 0.5); padding: 5px; pointer-events: none; z-index: 10;">X: 0.0, Y: 0.0, Z: 0.0</div>
                    </div>
                </div>
            </div>
        `;
        document.body.appendChild(craftWindow);
        this.gameWindow = craftWindow;
        
        setTimeout(() => {
            this.initCraftGame();
        }, 10);
    }
    
    showGUI() {
        if (this.gameWindow) {
            this.gameWindow.style.display = 'block';
            this.gameWindow.style.zIndex = 100;
            this.gameWindow.style.position = 'absolute';
            this.gameWindow.style.left = (window.innerWidth / 2 - 500) + 'px';
            this.gameWindow.style.top = (window.innerHeight / 2 - 350) + 'px';
        }
    }
    
    closeGUI() {
        if (this.gameWindow) {
            this.gameWindow.remove();
        }
        if (window.appInstances && window.appInstances[this.instanceId]) {
            delete window.appInstances[this.instanceId];
        }
    }
    
    initCraftGame() {
        const canvasContainerId = `${this.instanceId}-canvasContainer`;
        if (!document.getElementById(canvasContainerId)) {
            return;
        }
        
        if (typeof THREE === 'undefined' && !window.threejsLoaded) {
            window.threejsLoaded = true;
            const script = document.createElement('script');
            script.src = 'https://cdnjs.cloudflare.com/ajax/libs/three.js/0.144.0/three.min.js';
            script.onload = () => { this.setupCraftGame(); };
            document.head.appendChild(script);
        } else if (typeof THREE !== 'undefined') {
            this.setupCraftGame();
        } else {
            const poll = setInterval(() => {
                if (typeof THREE !== 'undefined') {
                    clearInterval(poll);
                    this.setupCraftGame();
                }
            }, 100);
        }
    }
    
    setupCraftGame() {
        const getEl = (id) => document.getElementById(`${this.instanceId}-${id}`);

        const scene = new THREE.Scene();
        scene.background = new THREE.Color(0x87CEEB);
        const camera = new THREE.PerspectiveCamera(75, 800 / 600, 0.1, 1000);
        const canvasContainer = getEl('canvasContainer');
        if (!canvasContainer) return;

        const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
        renderer.setSize(canvasContainer.clientWidth, canvasContainer.clientHeight);
        renderer.setPixelRatio(window.devicePixelRatio);
        canvasContainer.appendChild(renderer.domElement);

        const ambientLight = new THREE.AmbientLight(0x404040, 1.5);
        scene.add(ambientLight);
        const directionalLight = new THREE.DirectionalLight(0xffffff, 1);
        directionalLight.position.set(1, 1, 0.5).normalize();
        scene.add(directionalLight);

        // This is just a placeholder, the real content will be generated
        const geometry = new THREE.BoxGeometry();
        const material = new THREE.MeshStandardMaterial({ color: 0x00ff00 });
        const cube = new THREE.Mesh(geometry, material);
        scene.add(cube);

        camera.position.z = 5;

        const animate = () => {
            requestAnimationFrame(animate);
            cube.rotation.x += 0.01;
            cube.rotation.y += 0.01;
            renderer.render(scene, camera);
        };

        animate();
    }
}

if (typeof window !== 'undefined') {
    window.CraftApp = CraftApp;
}