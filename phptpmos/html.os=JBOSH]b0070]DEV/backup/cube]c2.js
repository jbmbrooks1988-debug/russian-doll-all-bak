// cube.js - 3D Cube Demo for JS CLI - Refactored to extend AppBase

class CubeApp extends AppBase {
    constructor() {
        super(); // Initialize the base class
    }
    
    getDefaultWidth() {
        return 400;  // Cube app specific width
    }
    
    getDefaultHeight() {
        return 300;  // Cube app specific height
    }
    
    createGUI() {
        // Check if GUI element already exists in DOM for this instance
        let existingWindow = document.getElementById(`${this.instanceId}-main-window`);
        
        if (typeof document !== 'undefined' && !existingWindow) {
            // Create new window if it doesn't exist
            const craftWindow = document.createElement('div');
            craftWindow.id = `${this.instanceId}-main-window`;
            craftWindow.className = 'terminal-window';
            craftWindow.style.display = 'none';
            
            // Set initial size to be responsive to browser window size
            const maxWidth = Math.min(this.getDefaultWidth(), window.innerWidth - 100);
            const maxHeight = Math.min(this.getDefaultHeight(), window.innerHeight - 100);
            craftWindow.style.width = maxWidth + 'px';
            craftWindow.style.height = maxHeight + 'px';
            
            craftWindow.innerHTML = `
                <div class="title-bar">
                    Cube - 3D Demo
                    <button class="close-btn" onclick="window.appInstances['${this.instanceId}'].closeGUI()">×</button>
                </div>
                <div class="terminal-content" style="display: flex; flex-direction: column; height: 100%; overflow: auto;">
                    <div id="${this.instanceId}-main" style="position: relative; width: 100%; height: 100%;">
                        <div id="${this.instanceId}-canvasContainer" style="width: 100%; height: 100%;"></div>
                    </div>
                </div>
            `;
            document.body.appendChild(craftWindow);
            this.gameWindow = craftWindow; // Store reference to the window
            
            // Initialize the cube game after the DOM element is added
            setTimeout(() => {
                this.initCraftGame();
            }, 10); // Small delay to ensure DOM is ready
        } else if (existingWindow) {
            // If the element already exists in the DOM, store reference to it
            this.gameWindow = existingWindow;
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
    window.CubeApp = CubeApp;
}