// cube.js - 3D Cube Demo for JS CLI - Refactored to extend AppBase

class CubeApp extends AppBase {
    constructor(jobId = null) {
        super(); // Initialize the base class
        this.jobId = jobId; // Store the job ID if provided
        
        // Store reference to this instance so ProcessManager can interact with it
        if (!window.appInstances) {
            window.appInstances = {};
        }
        window.appInstances[this.instanceId] = this;
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
            craftWindow.className = 'app-window terminal-window';  // Use shared CSS class
            craftWindow.style.display = 'none';
            
            // Set initial size to be responsive to browser window size
            const maxWidth = Math.min(this.getDefaultWidth(), window.innerWidth - 100);
            const maxHeight = Math.min(this.getDefaultHeight(), window.innerHeight - 100);
            craftWindow.style.width = maxWidth + 'px';
            craftWindow.style.height = maxHeight + 'px';
            
            craftWindow.innerHTML = `
                <div class="title-bar">
                    Cube - 3D Demo
                    <div class="window-buttons">
                        <button class="minimize-btn" onclick="window.appInstances['${this.instanceId}'].minimizeWindow()">-</button>
                        <button class="fullscreen-btn" onclick="window.appInstances['${this.instanceId}'].toggleFullscreen()">□</button>
                        <button class="close-btn" onclick="window.appInstances['${this.instanceId}'].closeGUI()">×</button>
                    </div>
                </div>
                <div class="terminal-content">
                    <div id="${this.instanceId}-canvasContainer" class="canvas-container" style="width: 100%; height: 100%;"></div>
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
        const camera = new THREE.PerspectiveCamera(75, 1, 0.1, 1000); // Aspect ratio will be updated on resize
        const canvasContainer = getEl('canvasContainer');
        if (!canvasContainer) return;

        const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
        
        // Initially set size based on container
        const width = canvasContainer.clientWidth || 400;
        const height = canvasContainer.clientHeight || 300;
        renderer.setSize(width, height);
        renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2)); // Limit pixel ratio
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

        // Handle resize
        const resizeRenderer = () => {
            if (!canvasContainer) return;
            
            // Use getBoundingClientRect for more accurate dimensions
            const containerRect = canvasContainer.getBoundingClientRect();
            const width = containerRect.width || canvasContainer.clientWidth || 400;
            const height = containerRect.height || canvasContainer.clientHeight || 300;
            
            camera.aspect = width / height;
            camera.updateProjectionMatrix();
            renderer.setSize(width, height);
            renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
        };

        // Set initial size and add resize listener
        resizeRenderer();
        window.addEventListener('resize', resizeRenderer);

        const animate = () => {
            requestAnimationFrame(animate);
            cube.rotation.x += 0.01;
            cube.rotation.y += 0.01;
            renderer.render(scene, camera);
        };

        animate();
    }
    
    // Override getTitle to provide a proper title for the minimized tab
    getTitle() {
        return 'Cube - 3D Demo';
    }
    
    // AppBase provides the minimizeWindow implementation
    
    toggleFullscreen() {
        if (this.gameWindow) {
            if (!this.gameWindow.classList.contains('fullscreen')) {
                // Store original dimensions and position before going fullscreen
                this.originalWidth = this.gameWindow.style.width;
                this.originalHeight = this.gameWindow.style.height;
                this.originalLeft = this.gameWindow.style.left;
                this.originalTop = this.gameWindow.style.top;
                
                // Apply fullscreen styles
                this.gameWindow.style.width = '100%';
                this.gameWindow.style.height = 'calc(100vh - 40px)'; // Subtract toolbar height
                this.gameWindow.style.top = '0px';
                this.gameWindow.style.left = '0px';
                this.gameWindow.classList.add('fullscreen');
            } else {
                // Restore original dimensions and position
                this.gameWindow.style.width = this.originalWidth || '800px';
                this.gameWindow.style.height = this.originalHeight || '600px';
                this.gameWindow.style.top = this.originalTop || '50px';
                this.gameWindow.style.left = this.originalLeft || '50px';
                this.gameWindow.classList.remove('fullscreen');
            }
        }
    }
}

if (typeof window !== 'undefined') {
    window.CubeApp = CubeApp;
}