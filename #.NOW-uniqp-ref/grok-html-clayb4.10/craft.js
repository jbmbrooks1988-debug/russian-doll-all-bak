// craft.js - 3D Crafting Game for JS CLI

class CraftApp {
    constructor() {
        this.gameWindow = null;
        this.gameContainer = null;
        this.isInitialized = false;
        this.gameStarted = false;
        this.threejsLoaded = false;
        
        // Defer GUI creation until DOM is ready
        if (typeof document !== 'undefined' && document.readyState !== 'loading') {
            this.createGUI();
        } else if (typeof document !== 'undefined') {
            document.addEventListener('DOMContentLoaded', () => {
                this.createGUI();
            });
        }
    }
    
    createGUI() {
        // Create Craft main window if it doesn't exist
        if (typeof document !== 'undefined' && !document.getElementById('craft-main-window')) {
            const craftWindow = document.createElement('div');
            craftWindow.id = 'craft-main-window';
            craftWindow.className = 'terminal-window';
            craftWindow.style.display = 'none';
            craftWindow.style.width = '1000px';
            craftWindow.style.height = '700px';
            craftWindow.innerHTML = `
                <div class="title-bar">
                    Craft - 3D Building Game
                    <button class="close-btn" onclick="window.craftApp.closeGUI()">×</button>
                </div>
                <div class="terminal-content" style="display: flex; flex-direction: column; height: 100%; overflow: auto;">
                    <div style="position: absolute; top: 30px; right: 100px; z-index: 101; background: rgba(0,0,0,0.5); padding: 5px; border-radius: 3px;">
                        <button onclick="window.craftApp.closeGUI()" style="background: #555; color: white; border: none; padding: 5px 10px; cursor: pointer; border-radius: 3px;">Exit to Terminal</button>
                    </div>
                    <div id="craft-game-container" style="display: grid; grid-template-areas: 'header header' 'sidebar main'; grid-template-rows: 50px 1fr; grid-template-columns: 15% 85%; height: 100%; width: 100%;">
                        <div id="craft-header" style="grid-area: header; background-color: rgba(0, 0, 0, 0.5); border-bottom: 2px solid white; display: flex; align-items: center; padding-left: 10px;">
                            <div class="dropdown" style="position: relative; display: inline-block;">
                                <button id="craft-fileBtn" style="background: none; border: 1px solid white; color: white; padding: 5px 10px; margin-right: 10px; cursor: pointer;">File</button>
                                <div id="craft-fileDropdown" class="dropdown-content" style="display: none; position: absolute; background-color: rgba(0, 0, 0, 0.7); min-width: 120px; box-shadow: 0px 8px 16px 0px rgba(255,255,255,0.1); z-index: 100; border: 1px solid white;">
                                    <button id="craft-newBtn" style="background: none; border: none; color: white; padding: 8px 12px; width: 100%; text-align: left; cursor: pointer; font-size: 14px;">New</button>
                                    <div class="submenu" style="position: relative;">
                                        <button style="background: none; border: none; color: white; padding: 8px 12px; width: 100%; text-align: left; cursor: pointer; font-size: 14px; display: flex; justify-content: space-between; align-items: center;">Open Project <span>&gt;</span></button>
                                        <div class="submenu-content" style="display: none; position: absolute; left: 100%; top: 0; background-color: rgba(0, 0, 0, 0.7); min-width: 120px; border: 1px solid white;">
                                            <button class="project-btn" data-project="default" style="background: none; border: none; color: white; padding: 8px 12px; width: 100%; text-align: left; cursor: pointer; font-size: 14px; display: flex; justify-content: space-between; align-items: center;">default</button>
                                            <button class="project-btn" data-project="009" style="background: none; border: none; color: white; padding: 8px 12px; width: 100%; text-align: left; cursor: pointer; font-size: 14px; display: flex; justify-content: space-between; align-items: center;">009</button>
                                            <button class="project-btn" data-project="colors" style="background: none; border: none; color: white; padding: 8px 12px; width: 100%; text-align: left; cursor: pointer; font-size: 14px; display: flex; justify-content: space-between; align-items: center;">colors</button>
                                        </div>
                                    </div>
                                    <button id="craft-saveBtn" style="background: none; border: none; color: white; padding: 8px 12px; width: 100%; text-align: left; cursor: pointer; font-size: 14px;">Save</button>
                                </div>
                            </div>
                            <button id="craft-textBtn" style="background: none; border: 1px solid white; color: white; padding: 5px 10px; margin-right: 10px; cursor: pointer;">text</button>
                            <button id="craft-platBtn" style="background: none; border: 1px solid white; color: white; padding: 5px 10px; margin-right: 10px; cursor: pointer;">plat</button>
                            <button id="craft-view2DBtn" style="background: none; border: 1px solid white; color: white; padding: 5px 10px; margin-right: 10px; cursor: pointer;">2d</button>
                            <button id="craft-view3DBtn" style="background: none; border: 1px solid white; color: white; padding: 5px 10px; margin-right: 10px; cursor: pointer;">3d</button>
                        </div>
                        
                        <div id="craft-sidebar" style="grid-area: sidebar; background-color: rgba(0, 0, 0, 0.5); border-right: 2px solid white; overflow-y: auto; padding: 10px; color: white;">
                            <div class="sidebar-window" style="margin-bottom: 20px;">
                                <h3 style="margin-top: 0; font-size: 16px; border-bottom: 1px solid white; padding-bottom: 5px;">Color Picker</h3>
                                <div id="craft-color-palette" style="display: flex; flex-wrap: wrap; width: 110px; height: 120px; overflow-y: auto; border: 1px solid #555; padding: 2px; margin-top: 10px;"></div>
                                <div id="craft-color-status" style="margin-top: 10px; height: 20px; font-size: 12px;">Select a color!</div>
                            </div>
                            <div class="sidebar-window" style="margin-bottom: 20px;">
                                <h3 style="margin-top: 0; font-size: 16px; border-bottom: 1px solid white; padding-bottom: 5px;">File Viewer</h3>
                                <div id="craft-file-viewer" style="height: 65px; overflow-y: auto; border: 1px solid #555; padding: 5px; margin-top: 10px;">
                                    <!-- File items will be added here by JavaScript -->
                                </div>
                                <!-- Hidden file input for loading maps -->
                                <input type="file" id="craft-mapFileInput" accept=".txt" style="display: none;">
                            </div>
                        </div>
                        
                        <div id="craft-main" style="grid-area: main; position: relative;">
                            <div id="craft-gameContainer" style="position: relative; width: 100%; height: 100%;">
                                <div id="craft-canvasContainer" style="width: 100%; height: 100%;"></div>
                                
                                <div id="craft-crosshair" style="position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); width: 20px; height: 20px; pointer-events: none; z-index: 10;">
                                    <div id="craft-horizontal" style="position: absolute; background-color: white; top: 50%; left: 50%; width: 20px; height: 2px; transform: translate(-50%, -50%);"></div>
                                    <div id="craft-vertical" style="position: absolute; background-color: white; top: 50%; left: 50%; width: 2px; height: 20px; transform: translate(-50%, -50%);"></div>
                                </div>
                                
                                <div id="craft-uiContainer" style="position: absolute; top: 20px; right: 20px; width: 200px; height: 200px; background-color: rgba(0, 0, 0, 0.5); border: 2px solid white; pointer-events: none; z-index: 10;">
                                    <canvas id="craft-minimapCanvas" width="200" height="200" style="width: 100%; height: 100%;"></canvas>
                                </div>
                                
                                <div id="craft-coordinates" style="position: absolute; top: 220px; right: 20px; color: white; font-size: 14px; background-color: rgba(0, 0, 0, 0.5); padding: 5px; pointer-events: none; z-index: 10;">X: 0.0, Y: 0.0, Z: 0.0</div>
                                
                                <div id="craft-controls" style="position: absolute; bottom: 20px; left: 20px; color: white; background-color: rgba(0, 0, 0, 0.5); padding: 10px; font-size: 14px; pointer-events: none; z-index: 10;">
                                    <div>ARROWS: Move (both views)</div>
                                    <div>WASD: Look around (3D only)</div>
                                    <div>SPACE: Jump</div>
                                    <div>ESC: Picker mode</div>
                                    <div>CTRL: Control block</div>
                                    <div>ENTER: Use item</div>
                                    <div>SHIFT: Mine</div>
                                    <div>I: Inventory</div>
                                    <div>M: Menu</div>
                                </div>

                                <div id="craft-inventory" style="position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); width: 300px; height: 200px; background-color: rgba(0, 0, 0, 0.8); border: 2px solid white; color: white; padding: 20px; display: none; pointer-events: all; z-index: 100;">
                                    <button class="close-btn" onclick="window.craftApp.toggleInventory()" style="position: absolute; top: 10px; right: 10px; background: none; border: none; color: white; font-size: 20px; cursor: pointer;">X</button>
                                    <h3>Inventory</h3>
                                    <p>Empty for now...</p>
                                </div>

                                <div id="craft-menu" style="position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); width: 300px; height: 200px; background-color: rgba(0, 0, 0, 0.8); border: 2px solid white; color: white; padding: 20px; display: none; pointer-events: all; z-index: 100;">
                                    <button class="close-btn" onclick="window.craftApp.toggleMenu()" style="position: absolute; top: 10px; right: 10px; background: none; border: none; color: white; font-size: 20px; cursor: pointer;">X</button>
                                    <h3>Menu</h3>
                                    <p>Game Menu</p>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        `;
            document.body.appendChild(craftWindow);
            
            // Initialize the craft game after the DOM element is added
            setTimeout(() => {
                this.initCraftGame();
            }, 10); // Small delay to ensure DOM is ready
        }
    }
    
    init() {
        console.log('Craft App Initialized');
        this.createGUI();
        this.isInitialized = true;
    }
    
    showGUI() {
        // Ensure GUI is created before showing
        if (typeof document !== 'undefined' && !document.getElementById('craft-main-window')) {
            this.createGUI();
        }
        
        const window = document.getElementById('craft-main-window');
        if (window) {
            window.style.display = 'block';
            window.style.zIndex = 100; // Ensure it appears on top
            window.style.position = 'absolute';
            window.style.left = (window.innerWidth/2 - 500) + 'px'; // Based on 1000px width
            window.style.top = (window.innerHeight/2 - 350) + 'px'; // Based on 700px height
        }
    }
    
    closeGUI() {
        const window = document.getElementById('craft-main-window');
        if (window) {
            window.style.display = 'none';
            
            // Optionally pause the game or clean up resources here if needed
            // For now, just hide the window
        }
    }
    
    // Initialize the 3D crafting game components
    initCraftGame() {
        // Only initialize if the game container exists
        if (!document.getElementById('craft-canvasContainer')) {
            return;
        }
        
        // Add Three.js script dynamically if not already loaded
        if (typeof THREE === 'undefined') {
            const script = document.createElement('script');
            script.src = 'https://cdnjs.cloudflare.com/ajax/libs/three.js/0.144.0/three.min.js';
            script.onload = () => {
                this.setupCraftGame();
            };
            document.head.appendChild(script);
        } else {
            this.setupCraftGame();
        }
    }
    
    setupCraftGame() {
        // Define game constants and objects here
        const VIEW_MODE = {
            _3D: '3d',
            _2D: '2d'
        };

        const BLOCK_COLORS = {
            GRASS: 0x228B22,
            DIRT: 0x8B4513,
            STONE: 0x808080,
            WOOD: 0xA0522D,
            LEAVES: 0x006400
        };

        // Game state variables
        const blocks = new Map(); // key: `${x}-${y}-${z}`, value: {mesh, type}
        
        const player = {
            x: 0,
            y: 0,
            z: 0,
            velocityY: 0,
            rotationX: 0, // Yaw
            rotationY: 0, // Pitch
            onGround: false,
            speed: 5,
            jumpStrength: 8,
            height: 1.8
        };

        const keys = {
            KeyW: false,
            KeyA: false,
            KeyS: false,
            KeyD: false,
            ArrowLeft: false,
            ArrowRight: false,
            ArrowUp: false,
            ArrowDown: false,
            Space: false,
            Escape: false,
            ControlLeft: false,
            ControlRight: false,
            Enter: false,
            ShiftLeft: false,
            ShiftRight: false,
            KeyI: false,
            KeyM: false
        };

        const GRAVITY = -30;
        const PYRAMID_SIZE = 16;
        const PYRAMID_HEIGHT = 8;
        const centerX = 0;
        const centerZ = 0;
        const baseY = 5;
        let currentView = VIEW_MODE._3D;

        // Three.js setup
        const scene = new THREE.Scene();
        scene.background = new THREE.Color(0x87CEEB); // Sky blue

        // Initially set camera with a default aspect ratio, will be updated in resizeRenderer
        const camera = new THREE.PerspectiveCamera(75, 800 / 600, 0.1, 1000);

        const canvasContainer = document.getElementById('craft-canvasContainer');
        if (!canvasContainer) return; // Check if container exists
        
        // Force layout recalculation to ensure dimensions are available
        canvasContainer.style.minHeight = '100%';
        canvasContainer.style.minWidth = '100%';
        // Trigger reflow to ensure dimensions are calculated
        void canvasContainer.offsetWidth;
        
        const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
        // Use getBoundingClientRect for more accurate dimensions
        const containerRect = canvasContainer.getBoundingClientRect();
        const width = containerRect.width || canvasContainer.clientWidth || 800;  // fallback width
        const height = containerRect.height || canvasContainer.clientHeight || 600; // fallback height
        
        renderer.setSize(width, height);
        renderer.setPixelRatio(window.devicePixelRatio || 1); // Handle high DPI displays
        canvasContainer.appendChild(renderer.domElement);

        // Simple lighting
        const ambientLight = new THREE.AmbientLight(0x404040, 1);
        scene.add(ambientLight);
        
        const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
        directionalLight.position.set(1, 1, 0.5).normalize();
        scene.add(directionalLight);

        // Player mesh (orange box like in example's red circle)
        const playerGeometry = new THREE.BoxGeometry(0.8, player.height, 0.8);
        const playerMaterial = new THREE.MeshLambertMaterial({ color: 0xFFA500 });
        const playerMesh = new THREE.Mesh(playerGeometry, playerMaterial);
        scene.add(playerMesh);

        // Add light grid lines for voxel separation
        const gridHelper = new THREE.GridHelper(PYRAMID_SIZE, PYRAMID_SIZE, 0xffffff, 0xffffff);
        gridHelper.position.set(centerX, baseY - 0.5, centerZ);
        scene.add(gridHelper);

        // Coordinates display
        const coordinatesDisplay = document.getElementById('craft-coordinates');

        // Update minimap (simple for now)
        const minimapCanvas = document.getElementById('craft-minimapCanvas');
        if (minimapCanvas) {
            const minimapCtx = minimapCanvas.getContext('2d');
        }

        // 2D canvas for 2D view
        const view2DCanvas = document.createElement('canvas');
        view2DCanvas.id = 'craft-view2DCanvas';
        view2DCanvas.style.display = 'none';
        view2DCanvas.style.position = 'absolute';
        view2DCanvas.style.width = '100%';
        view2DCanvas.style.height = '100%';
        canvasContainer.appendChild(view2DCanvas);
        
        const view2DContext = view2DCanvas.getContext('2d');
        
        // Set up 2D canvas
        view2DCanvas.width = canvasContainer.clientWidth;
        view2DCanvas.height = canvasContainer.clientHeight;

        // Function to create a block mesh
        function createBlock(x, y, z, color, type = 'STONE') {
            const geometry = new THREE.BoxGeometry(1, 1, 1);
            const material = new THREE.MeshLambertMaterial({ 
                color: color,
                wireframe: false
            });
            const block = new THREE.Mesh(geometry, material);
            block.position.set(x, y, z);
            scene.add(block);
            const key = Math.floor(x) + '-' + Math.floor(y) + '-' + Math.floor(z);
            blocks.set(key, { mesh: block, type });
            return block;
        }

        // Function to generate the pyramid planet
        function generatePyramidPlanet() {
            // Generate pyramid layers
            for (let level = 0; level < PYRAMID_HEIGHT; level++) {
                const width = PYRAMID_SIZE - 2 * level;
                const halfWidth = width / 2;
                const y = baseY + level;
                
                for (let dx = -halfWidth; dx < halfWidth; dx++) {
                    for (let dz = -halfWidth; dz < halfWidth; dz++) {
                        const x = centerX + dx + 0.5; // Offset to center
                        const z = centerZ + dz + 0.5;
                        
                        // Determine block type based on level
                        let color, type;
                        if (level === PYRAMID_HEIGHT - 1) {
                            color = BLOCK_COLORS.GRASS;
                            type = 'GRASS';
                        } else if (level > PYRAMID_HEIGHT / 2) {
                            color = BLOCK_COLORS.DIRT;
                            type = 'DIRT';
                        } else {
                            color = BLOCK_COLORS.STONE;
                            type = 'STONE';
                        }
                        
                        createBlock(x, y, z, color, type);
                    }
                }
            }
            
            // Add random obstacles (e.g., small 'rocks' or 'trees' on top)
            const numObstacles = 10;
            for (let i = 0; i < numObstacles; i++) {
                // Random position on top layer
                const topY = baseY + PYRAMID_HEIGHT - 1;
                const randX = centerX + (Math.random() - 0.5) * (PYRAMID_SIZE - 2);
                const randZ = centerZ + (Math.random() - 0.5) * (PYRAMID_SIZE - 2);
                
                // Simple 'rock': 1-3 stacked stones
                const height = 1 + Math.floor(Math.random() * 3);
                for (let h = 0; h < height; h++) {
                    createBlock(randX, topY + h, randZ, BLOCK_COLORS.STONE, 'STONE');
                }
            }
            player.y = baseY + PYRAMID_HEIGHT + 1.8;
        }

        // Simple collision with pyramid (basic: check if below player is 'ground')
        function isOnGround(px, py, pz) {
            // Approximate pyramid: for a given x,z, max y is baseY + height - distance from center
            const dist = Math.max(Math.abs(px - centerX), Math.abs(pz - centerZ));
            const maxHeight = PYRAMID_HEIGHT - Math.floor(dist);
            return py <= baseY + maxHeight + 0.1;
        }

        // View switching functions
        function switchTo2DView() {
            currentView = VIEW_MODE._2D;
            renderer.domElement.style.display = 'none';
            view2DCanvas.style.display = 'block';
        }

        function switchTo3DView() {
            currentView = VIEW_MODE._3D;
            view2DCanvas.style.display = 'none';
            renderer.domElement.style.display = 'block';
        }
        
        // 2D rendering function
        function render2D() {
            if (!view2DContext) return;
            
            // Clear the 2D canvas
            view2DContext.clearRect(0, 0, view2DCanvas.width, view2DCanvas.height);
            
            // Calculate viewport dimensions
            const blockSize = 20; // Size of each block in pixels
            const centerX = view2DCanvas.width / 2;
            const centerY = view2DCanvas.height / 2;
            
            // Calculate visible area based on player position
            const halfWidth = Math.floor(view2DCanvas.width / (2 * blockSize));
            const halfHeight = Math.floor(view2DCanvas.height / (2 * blockSize));
            
            // Draw blocks
            for (let [key, blockData] of blocks) {
                const [x, y, z] = key.split('-').map(Number);
                
                // Only render blocks approximately at the same Y level as the player
                if (Math.abs(y - Math.floor(player.y)) <= 2) {
                    // Calculate screen position relative to player
                    const screenX = centerX + (x - player.x) * blockSize;
                    const screenY = centerY + (z - player.z) * blockSize;
                    
                    // Draw block if it's within the visible range
                    if (screenX > -blockSize && screenX < view2DCanvas.width && 
                        screenY > -blockSize && screenY < view2DCanvas.height) {
                        
                        // Set color based on block type
                        switch (blockData.type) {
                            case 'GRASS':
                                view2DContext.fillStyle = '#228B22';
                                break;
                            case 'DIRT':
                                view2DContext.fillStyle = '#8B4513';
                                break;
                            case 'STONE':
                                view2DContext.fillStyle = '#808080';
                                break;
                            case 'WOOD':
                                view2DContext.fillStyle = '#A0522D';
                                break;
                            case 'LEAVES':
                                view2DContext.fillStyle = '#006400';
                                break;
                            default:
                                view2DContext.fillStyle = '#FFFFFF';
                        }
                        
                        view2DContext.fillRect(screenX - blockSize/2, screenY - blockSize/2, blockSize, blockSize);
                        view2DContext.strokeStyle = '#000000';
                        view2DContext.strokeRect(screenX - blockSize/2, screenY - blockSize/2, blockSize, blockSize);
                    }
                }
            }
            
            // Draw player
            const playerScreenX = centerX;
            const playerScreenY = centerY;
            view2DContext.fillStyle = '#FFA500'; // Orange color for player
            view2DContext.beginPath();
            view2DContext.arc(playerScreenX, playerScreenY, blockSize/2, 0, Math.PI * 2);
            view2DContext.fill();
        }

        // Handle resize
        function resizeRenderer() {
            if (!canvasContainer) return;
            
            // Use getBoundingClientRect for more accurate dimensions
            const containerRect = canvasContainer.getBoundingClientRect();
            const width = containerRect.width || canvasContainer.clientWidth || 800;
            const height = containerRect.height || canvasContainer.clientHeight || 600;
            
            camera.aspect = width / height;
            camera.updateProjectionMatrix();
            renderer.setSize(width, height);
            renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2)); // Limit pixel ratio to prevent performance issues
            view2DCanvas.width = width;
            view2DCanvas.height = height;
        }

        // Picker mode
        let pickerMode = false;
        let raycaster = new THREE.Raycaster();
        let mouse = new THREE.Vector2(0, 0); // Center for crosshair

        function togglePickerMode() {
            pickerMode = !pickerMode;
            document.body.requestPointerLock(); // Optional, but helps with mouse
            console.log('Picker mode:', pickerMode);
        }

        // Simple raycast for picking/placing
        function getTargetBlock() {
            raycaster.setFromCamera(mouse, camera);
            const intersects = raycaster.intersectObjects(scene.children);
            if (intersects.length > 0) {
                const intersect = intersects[0];
                const pos = intersect.point;
                const x = Math.floor(pos.x);
                const y = Math.floor(pos.y);
                const z = Math.floor(pos.z);
                const key = `${x}-${y}-${z}`;
                return {key, x, y, z, intersect};
            }
            return null;
        }

        function mineBlock() {
            if (!pickerMode) return;
            const target = getTargetBlock();
            if (target && blocks.has(target.key)) {
                const blockData = blocks.get(target.key);
                scene.remove(blockData.mesh);
                blocks.delete(target.key);
                // Add to inventory (simple log)
                console.log(`Mined ${blockData.type}`);
            }
        }

        function useItem() {
            if (!pickerMode) return;
            const target = getTargetBlock();
            if (target) {
                // Place block adjacent (simple: offset by normal)
                const normal = target.intersect.face.normal;
                const placePos = target.intersect.point.clone().add(normal.multiplyScalar(0.5));
                const px = Math.floor(placePos.x);
                const py = Math.floor(placePos.y);
                const pz = Math.floor(placePos.z);
                const key = `${px}-${py}-${pz}`;
                if (!blocks.has(key)) {
                    // Assume placing stone for now with default parameters
                    createBlock(px + 0.5, py + 0.5, pz + 0.5, BLOCK_COLORS.STONE, 'STONE');
                }
            }
        }

        // Block control (simple highlight)
        let controlledBlock = null;
        function controlBlock() {
            if (!pickerMode) return;
            const target = getTargetBlock();
            if (target && blocks.has(target.key)) {
                if (controlledBlock) {
                    controlledBlock.mesh.material.emissive.setHex(0x000000); // Reset previous
                }
                controlledBlock = blocks.get(target.key);
                controlledBlock.mesh.material.emissive.setHex(0x222222); // Highlight
                console.log('Controlling block:', target.key);
            }
        }

        // Mouse look (click to lock) - simplified, no movement in picker mode
        let mouseLocked = false;
        if (canvasContainer) {
            canvasContainer.addEventListener('click', () => {
                if (!pickerMode) {
                    if (!mouseLocked) {
                        canvasContainer.requestPointerLock();
                    }
                }
            });
        }

        document.addEventListener('pointerlockchange', () => {
            mouseLocked = document.pointerLockElement === (canvasContainer || document.body);
        });

        if (canvasContainer) {
            canvasContainer.addEventListener('mousemove', (e) => {
                if (mouseLocked && !pickerMode) {
                    player.rotationX += e.movementX * 0.002;
                    player.rotationY -= e.movementY * 0.002;
                    player.rotationY = Math.max(-90, Math.min(90, player.rotationY));
                }
            });
        }
        
        // Prevent context menu only on the game container
        if (canvasContainer) {
            canvasContainer.addEventListener('contextmenu', (e) => e.preventDefault());
        }

        window.addEventListener('resize', resizeRenderer);

        // Game loop - adapted from example, with deltaTime for smooth movement
        let lastTime = performance.now();
        function gameLoop(currentTime) {
            const deltaTime = (currentTime - lastTime) / 1000;
            lastTime = currentTime;
            
            // Update player position based on pressed keys - Arrows for movement
            let moveX = 0;
            let moveZ = 0;
            
            // Movement differs between 2D and 3D views
            if (currentView === VIEW_MODE._3D) {
                // 3D view: movement is relative to player rotation (WASD-style)
                const forward = Math.sin(player.rotationX * Math.PI / 180);
                const right = Math.cos(player.rotationX * Math.PI / 180);
                
                if (keys.ArrowUp) {
                    moveX += right * player.speed * deltaTime;
                    moveZ += forward * player.speed * deltaTime;
                }
                if (keys.ArrowDown) {
                    moveX -= right * player.speed * deltaTime;
                    moveZ -= forward * player.speed * deltaTime;
                }
                if (keys.ArrowLeft) {
                    moveX += forward * player.speed * deltaTime;
                    moveZ -= right * player.speed * deltaTime;
                }
                if (keys.ArrowRight) {
                    moveX -= forward * player.speed * deltaTime;
                    moveZ += right * player.speed * deltaTime;
                }
            } else {
                // 2D view: movement is absolute (top-down style)
                // Up arrow moves in negative Z direction
                // Down arrow moves in positive Z direction
                // Left arrow moves in negative X direction
                // Right arrow moves in positive X direction
                if (keys.ArrowUp) {
                    moveZ -= player.speed * deltaTime;
                }
                if (keys.ArrowDown) {
                    moveZ += player.speed * deltaTime;
                }
                if (keys.ArrowLeft) {
                    moveX -= player.speed * deltaTime;
                }
                if (keys.ArrowRight) {
                    moveX += player.speed * deltaTime;
                }
            }
            
            player.x += moveX;
            player.z += moveZ;
            
            // WASD for looking around (only relevant in 3D view)
            if (currentView === VIEW_MODE._3D) {
                if (keys.KeyA) player.rotationX -= 90 * deltaTime;
                if (keys.KeyD) player.rotationX += 90 * deltaTime;
                if (keys.KeyW) player.rotationY = Math.max(-90, Math.min(90, player.rotationY + 45 * deltaTime));
                if (keys.KeyS) player.rotationY = Math.max(-90, Math.min(90, player.rotationY - 45 * deltaTime));
            }
            
            // Physics: gravity and ground collision (same in both views)
            player.velocityY += GRAVITY * deltaTime;
            player.y += player.velocityY * deltaTime;
            
            // Ground collision with pyramid
            const px = Math.floor(player.x);
            const py = Math.floor(player.y);
            const pz = Math.floor(player.z);
            
            if (isOnGround(px, py, pz)) {
                player.y = Math.max(player.y, py + 1); // Snap to top
                player.velocityY = 0;
                player.onGround = true;
            } else {
                player.onGround = false;
            }
            
            // Reset if fallen too far (fall off planet)
            if (player.y < -10) {
                player.y = baseY + PYRAMID_HEIGHT + 1.8;
                player.x = 0;
                player.z = 0;
                player.velocityY = 0;
            }
            
            // Boundary checking (around planet)
            const planetRadius = PYRAMID_SIZE / 2 + 5;
            const distFromCenter = Math.sqrt(player.x * player.x + player.z * player.z);
            if (distFromCenter > planetRadius) {
                // Push back toward center
                const angle = Math.atan2(player.z, player.x);
                player.x = Math.cos(angle) * planetRadius;
                player.z = Math.sin(angle) * planetRadius;
            }
            
            // Update camera to follow player (first person) - only for 3D view
            if (currentView === VIEW_MODE._3D) {
                camera.position.set(player.x, player.y + 1.6, player.z);
                
                const lookX = Math.cos(player.rotationX * Math.PI / 180) * Math.cos(player.rotationY * Math.PI / 180);
                const lookY = Math.sin(player.rotationY * Math.PI / 180);
                const lookZ = Math.sin(player.rotationX * Math.PI / 180) * Math.cos(player.rotationY * Math.PI / 180);
                camera.lookAt(
                    player.x + lookX,
                    player.y + 1.6 + lookY,
                    player.z + lookZ
                );
                
                // Update player mesh position (third person view for visibility)
                playerMesh.position.set(player.x, player.y, player.z);
            }
            
            // Render based on current view
            if (currentView === VIEW_MODE._3D) {
                renderer.render(scene, camera);
            } else {
                if (typeof render2D === 'function') render2D();
            }
            
            // Update coordinates
            if (coordinatesDisplay) {
                coordinatesDisplay.textContent = `X: ${player.x.toFixed(1)}, Y: ${player.y.toFixed(1)}, Z: ${player.z.toFixed(1)}`;
            }
            
            // Update minimap (top-down view of planet)
            if (minimapCanvas) {
                const minimapCtx = minimapCanvas.getContext('2d');
                if (minimapCtx) {
                    minimapCtx.clearRect(0, 0, 200, 200);
                    minimapCtx.strokeStyle = 'white';
                    minimapCtx.lineWidth = 2;
                    minimapCtx.strokeRect(0, 0, 200, 200);
                    
                    // Draw simple pyramid outline on minimap
                    const mapScale = PYRAMID_SIZE / 200;
                    const mapCenterX = 100;
                    const mapCenterZ = 100;
                    minimapCtx.fillStyle = 'rgba(34, 139, 34, 0.3)'; // Semi-transparent green for planet
                    for (let level = 0; level < PYRAMID_HEIGHT; level++) {
                        const width = (PYRAMID_SIZE - 2 * level) * mapScale;
                        const halfW = width / 2;
                        minimapCtx.fillRect(mapCenterX - halfW, mapCenterZ - halfW, width, width);
                    }
                    
                    // Player on minimap
                    const mapX = mapCenterX + player.x * mapScale;
                    const mapZ = mapCenterZ + player.z * mapScale;
                    minimapCtx.fillStyle = 'red';
                    minimapCtx.beginPath();
                    minimapCtx.arc(mapX, mapZ, 4, 0, Math.PI * 2);
                    minimapCtx.fill();
                }
            }
            
            // Continue game loop
            requestAnimationFrame(gameLoop);
        }

        // Set up color picker
        const colors = [
            [255, 0, 0], [0, 255, 0], [0, 0, 255], [255, 255, 0], [0, 255, 255], [255, 0, 255],
            [255, 255, 255], [0, 0, 0], [128, 0, 0], [0, 128, 0], [0, 0, 128], [128, 128, 0],
            [128, 0, 128], [0, 128, 128], [192, 192, 192], [128, 128, 128], [255, 165, 0], [255, 192, 203]
        ];
        const colorNames = ["Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "White", "Black", "Maroon", "Dark Green", "Navy", "Olive", "Purple", "Teal", "Silver", "Gray", "Orange", "Pink"];
        let selectedColor = 0;

        function setupColorPalette() {
            const colorPalette = document.getElementById('craft-color-palette');
            if (!colorPalette) return;
            
            colors.forEach((color, i) => {
                const item = document.createElement('div');
                item.className = 'palette-item';
                item.style.backgroundColor = `rgb(${color[0]}, ${color[1]}, ${color[2]})`;
                item.dataset.idx = i;
                item.style.width = '32px';
                item.style.height = '32px';
                item.style.cursor = 'pointer';
                item.style.border = '1px solid #555';
                item.style.margin = '2px';
                item.addEventListener('click', () => {
                    selectedColor = i;
                    updateSelections();
                    console.log(`Color selected: ${colorNames[i]} - RGB(${color[0]}, ${color[1]}, ${color[2]})`);
                });
                colorPalette.appendChild(item);
            });
            updateSelections();
        }

        function updateSelections() {
            const paletteItems = document.querySelectorAll('#craft-color-palette .palette-item');
            paletteItems.forEach((item, i) => {
                if (i === selectedColor) {
                    item.style.border = '2px solid yellow';
                } else {
                    item.style.border = '1px solid #555';
                }
            });
            
            const status = document.getElementById('craft-color-status');
            if (status) {
                status.textContent = `Selected: ${colorNames[selectedColor]}`;
            }
        }

        // Project Manager for saving/loading maps
        class ProjectManager {
            constructor() {
                this.projects = {
                    'default': ['global_events.txt', 'map_0_events.txt', 'map_0.txt', 'map_1.txt', 'map_2_events.txt', 'map_2.txt', 'map_events_0.txt'],
                    '009': ['global_events.txt', 'map_0_events.txt', 'map_0.txt', 'map_1_events.txt', 'map_1.txt'],
                    'colors': ['global_ev', 'map_0.txt']
                };
                this.currentProject = 'default';
            }

            // Get current project name
            getCurrentProject() {
                return this.currentProject;
            }

            // Set current project
            setCurrentProject(projectName) {
                if (this.projects[projectName]) {
                    this.currentProject = projectName;
                    this.updateFileViewer();
                    return true;
                }
                return false;
            }

            // Get all projects
            getProjects() {
                return Object.keys(this.projects);
            }

            // Get files for a project
            getProjectFiles(projectName) {
                return this.projects[projectName] || [];
            }
            
            // Update file viewer
            updateFileViewer() {
                const projectName = this.currentProject;
                const fileViewer = document.getElementById('craft-file-viewer');
                if (!fileViewer) return;

                fileViewer.innerHTML = ''; // Clear existing files

                const files = this.getProjectFiles(projectName) || [];
                files.forEach(fileName => {
                    const item = document.createElement('div');
                    item.className = 'file-item';
                    item.style.padding = '2px 5px';
                    item.style.cursor = 'pointer';
                    item.style.fontSize = '14px';
                    item.textContent = fileName;
                    item.addEventListener('click', () => {
                        // Check if it's a map file (ends with .txt but not _events.txt)
                        if (fileName.endsWith('.txt') && !fileName.includes('_events.txt')) {
                            // Load the map
                            if (this.loadMap(projectName, fileName)) {
                                console.log(`Map loaded: ${fileName} from project ${projectName}`);
                            } else {
                                console.error(`Failed to load map: ${fileName}`);
                            }
                        } else {
                            console.log(`File selected: ${fileName} from project ${projectName}`);
                        }
                    });
                    fileViewer.appendChild(item);
                });
            }

            // Save current world state to a map file
            saveMap(projectName, mapName) {
                try {
                    // Convert blocks to CSV format
                    let csvContent = 'x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z\n';
                    
                    // Iterate through all blocks
                    blocks.forEach((blockData, key) => {
                        const [x, y, z] = key.split('-').map(Number);
                        const {
                            emoji_idx = 0,
                            fg_color_idx = 0,
                            alpha = 1,
                            scale = 1,
                            rel_x = 0,
                            rel_y = 0,
                            rel_z = 0
                        } = blockData;
                        
                        csvContent += `${x},${y},${z},${emoji_idx},${fg_color_idx},${alpha},${scale},${rel_x},${rel_y},${rel_z}\n`;
                    });

                    // In a browser environment, we can't directly save files to disk
                    // Instead, we'll create a downloadable link
                    const blob = new Blob([csvContent], { type: 'text/csv' });
                    const url = URL.createObjectURL(blob);
                    
                    // Create a temporary link and trigger download
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = mapName;
                    document.body.appendChild(a);
                    a.click();
                    
                    // Clean up
                    setTimeout(() => {
                        document.body.removeChild(a);
                        URL.revokeObjectURL(url);
                    }, 100);
                    
                    console.log(`Map saved successfully as "${mapName}" for project "${projectName}"!`);
                    
                    // Add to project files if not already there
                    if (!this.projects[projectName].includes(mapName)) {
                        this.projects[projectName].push(mapName);
                    }
                    
                    // Update file viewer
                    this.updateFileViewer();
                    
                    return true;
                } catch (error) {
                    console.error("Error saving map:", error);
                    return false;
                }
            }

            // Parse CSV data and create blocks
            parseCSVAndCreateBlocks(csvData) {
                try {
                    const lines = csvData.trim().split('\n');
                    if (lines.length < 2) {
                        console.error("Invalid CSV format: Not enough lines");
                        return false;
                    }
                    
                    // Skip header line
                    for (let i = 1; i < lines.length; i++) {
                        const values = lines[i].split(',');
                        if (values.length !== 10) {
                            console.warn(`Skipping invalid line ${i}: Expected 10 values, got ${values.length}`);
                            continue;
                        }
                        
                        // Parse values
                        const x = parseFloat(values[0]);
                        const y = parseFloat(values[1]);
                        const z = parseFloat(values[2]);
                        const emoji_idx = parseInt(values[3]) || 0;
                        const fg_color_idx = parseInt(values[4]) || 0;
                        const alpha = parseFloat(values[5]) || 1;
                        const scale = parseFloat(values[6]) || 1;
                        const rel_x = parseFloat(values[7]) || 0;
                        const rel_y = parseFloat(values[8]) || 0;
                        const rel_z = parseFloat(values[9]) || 0;
                        
                        // Determine block type and color based on fg_color_idx or other criteria
                        let color = BLOCK_COLORS.STONE;
                        let type = 'STONE';
                        
                        // Map fg_color_idx to actual colors (simplified mapping)
                        switch (fg_color_idx) {
                            case 1: // Green
                                color = BLOCK_COLORS.GRASS;
                                type = 'GRASS';
                                break;
                            case 2: // Brown
                                color = BLOCK_COLORS.DIRT;
                                type = 'DIRT';
                                break;
                            case 3: // Gray
                                color = BLOCK_COLORS.STONE;
                                type = 'STONE';
                                break;
                            case 4: // Wood color
                                color = BLOCK_COLORS.WOOD;
                                type = 'WOOD';
                                break;
                            case 5: // Leaf color
                                color = BLOCK_COLORS.LEAVES;
                                type = 'LEAVES';
                                break;
                            default:
                                color = BLOCK_COLORS.STONE;
                                type = 'STONE';
                        }
                        
                        // Create the block with all parameters
                        createBlock(x, y, z, color, type);
                    }
                    
                    return true;
                } catch (error) {
                    console.error("Error parsing CSV:", error);
                    return false;
                }
            }

            // Load a map from a CSV file
            loadMap(projectName, mapName) {
                try {
                    // Clear existing blocks from the scene
                    blocks.forEach((blockData) => {
                        scene.remove(blockData.mesh);
                    });
                    blocks.clear();
                    
                    // For demonstration purposes, we'll generate sample CSV data
                    // In a real implementation, this would come from an actual uploaded file
                    const sampleCSV = `x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z
0,5,0,0,1,1,1,0,0,0
1,5,0,0,1,1,1,0,0,0
-1,5,0,0,1,1,1,0,0,0
0,5,1,0,1,1,1,0,0,0
0,5,-1,0,1,1,1,0,0,0
0,6,0,0,3,1,1,0,0,0
0,7,0,0,3,1,1,0,0,0
0,8,0,0,4,1,1,0,0,0`;
                    
                    // Parse the CSV data and create blocks
                    if (this.parseCSVAndCreateBlocks(sampleCSV)) {
                        console.log(`Successfully loaded map "${mapName}" for project "${projectName}"`);
                        return true;
                    } else {
                        console.error(`Failed to parse map "${mapName}" for project "${projectName}"`);
                        return false;
                    }
                } catch (error) {
                    console.error("Error loading map:", error);
                    return false;
                }
            }

            // Simulate loading a map file (in a real implementation, this would load from actual files)
            loadMapFile(projectName, fileName) {
                try {
                    // Clear existing blocks from the scene
                    blocks.forEach((blockData) => {
                        scene.remove(blockData.mesh);
                    });
                    blocks.clear();
                    
                    // In a real implementation, this would load the actual file
                    // For now, we'll generate some sample data based on the file name
                    let sampleCSV;
                    
                    if (fileName.includes('map_0')) {
                        // Sample data for map_0
                        sampleCSV = `x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z
0,5,0,0,1,1,1,0,0,0
1,5,0,0,1,1,1,0,0,0
-1,5,0,0,1,1,1,0,0,0
0,5,1,0,1,1,1,0,0,0
0,5,-1,0,1,1,1,0,0,0
0,6,0,0,3,1,1,0,0,0
0,7,0,0,3,1,1,0,0,0
0,8,0,0,4,1,1,0,0,0`;
                    } else if (fileName.includes('map_1')) {
                        // Sample data for map_1
                        sampleCSV = `x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z
2,5,2,0,2,1,1,0,0,0
3,5,2,0,2,1,1,0,0,0
2,5,3,0,2,1,1,0,0,0
3,5,3,0,2,1,1,0,0,0
2,6,2,0,4,1,1,0,0,0
3,6,3,0,4,1,1,0,0,0`;
                    } else {
                        // Generic sample data
                        sampleCSV = `x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z
0,5,0,0,1,1,1,0,0,0
1,5,0,0,3,1,1,0,0,0
0,5,1,0,4,1,1,0,0,0`;
                    }
                    
                    // Parse the CSV data and create blocks
                    if (this.parseCSVAndCreateBlocks(sampleCSV)) {
                        console.log(`Successfully loaded map "${fileName}" for project "${projectName}"`);
                        return true;
                    } else {
                        console.error(`Failed to parse map "${fileName}" for project "${projectName}"`);
                        return false;
                    }
                } catch (error) {
                    console.error("Error loading map file:", error);
                    return false;
                }
            }
        }

        // Create project manager instance
        const projectManager = new ProjectManager();

        // Set up event listeners
        function setupEventListeners() {
            // Button event listeners for view switching
            const textBtn = document.getElementById('craft-textBtn');
            if (textBtn) textBtn.addEventListener('click', function() {
                // Future implementation
            });

            const platBtn = document.getElementById('craft-platBtn');
            if (platBtn) platBtn.addEventListener('click', function() {
                // Future implementation
            });

            const view2DBtn = document.getElementById('craft-view2DBtn');
            if (view2DBtn) view2DBtn.addEventListener('click', switchTo2DView);

            const view3DBtn = document.getElementById('craft-view3DBtn');
            if (view3DBtn) view3DBtn.addEventListener('click', switchTo3DView);

            // File operation button listeners
            const newBtn = document.getElementById('craft-newBtn');
            if (newBtn) newBtn.addEventListener('click', function() {
                // Confirm before clearing the world
                if (confirm('Are you sure you want to start a new world? This will clear the current one.')) {
                    // Clear all blocks from the scene
                    blocks.forEach((blockData, key) => {
                        scene.remove(blockData.mesh);
                    });
                    blocks.clear();
                    
                    // Regenerate the pyramid planet
                    generatePyramidPlanet();
                }
            });

            const saveBtn = document.getElementById('craft-saveBtn');
            if (saveBtn) saveBtn.addEventListener('click', function() {
                // Get current project and generate a unique map name
                const projectName = projectManager.getCurrentProject();
                const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
                const mapName = `map_${timestamp}.txt`;
                
                // Save the current world state
                if (projectManager.saveMap(projectName, mapName)) {
                    alert(`World saved successfully as ${mapName} in project ${projectName}!`);
                } else {
                    alert('Failed to save the world.');
                }
            });

            // Project selection event listeners
            document.querySelectorAll('.project-btn').forEach(button => {
                button.addEventListener('click', (e) => {
                    const projectName = e.target.dataset.project;
                    if (projectManager.setCurrentProject(projectName)) {
                        console.log(`Switched to project: ${projectName}`);
                    } else {
                        console.error(`Failed to switch to project: ${projectName}`);
                    }
                });
            });

            // Dropdown functionality
            const fileBtn = document.getElementById('craft-fileBtn');
            const fileDropdown = document.getElementById('craft-fileDropdown');
            if (fileBtn && fileDropdown) {
                fileBtn.addEventListener('click', function() {
                    fileDropdown.style.display = fileDropdown.style.display === 'block' ? 'none' : 'block';
                });
            }

            // Add event listeners for key presses - only when game canvas is focused
            const canvasContainer = document.getElementById('craft-canvasContainer');
            if (canvasContainer) {
                canvasContainer.addEventListener('keydown', (e) => {
                    console.log('Key down:', e.key); // Debug
                    const key = e.code;
                    if (key in keys) {
                        keys[key] = true;
                        e.preventDefault(); // Prevent scrolling
                    }
                    
                    // Handle space for jumping
                    if (e.code === 'Space' && player.onGround) {
                        player.velocityY = player.jumpStrength;
                        player.onGround = false;
                        e.preventDefault();
                    }

                    // Handle new keys
                    if (e.code === 'Escape') {
                        togglePickerMode();
                        e.preventDefault();
                    }
                    if (e.code === 'KeyI') {
                        window.craftApp.toggleInventory();
                        e.preventDefault();
                    }
                    if (e.code === 'KeyM') {
                        window.craftApp.toggleMenu();
                        e.preventDefault();
                    }
                    if (e.code === 'Enter') {
                        useItem();
                        e.preventDefault();
                    }
                    if (e.code === 'ShiftLeft' || e.code === 'ShiftRight') {
                        mineBlock();
                        e.preventDefault();
                    }
                });

                canvasContainer.addEventListener('keyup', (e) => {
                    console.log('Key up:', e.key); // Debug
                    const key = e.code;
                    if (key in keys) {
                        keys[key] = false;
                    }
                });
                
                // Set up click handler to focus the canvas when clicked
                canvasContainer.addEventListener('click', () => {
                    canvasContainer.focus();
                });
                
                // Make canvas container focusable
                canvasContainer.setAttribute('tabindex', '-1'); // Make it focusable
            }
        }

        // Initialize dropdowns and color picker
        setupColorPalette();
        projectManager.updateFileViewer();
        setupEventListeners();

        // Initial setup - use MutationObserver to ensure DOM is fully rendered
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => {
                setTimeout(() => {
                    resizeRenderer();
                    generatePyramidPlanet();
                    switchTo3DView(); // Ensure 3D view is active at startup
                    // Position camera properly to see the pyramid
                    camera.position.set(0, PYRAMID_HEIGHT + 5, PYRAMID_HEIGHT + 5);
                    camera.lookAt(0, PYRAMID_HEIGHT, 0);
                    requestAnimationFrame(gameLoop);
                }, 50);
            });
        } else {
            // DOM is already ready
            setTimeout(() => {
                resizeRenderer();
                generatePyramidPlanet();
                switchTo3DView(); // Ensure 3D view is active at startup
                // Position camera properly to see the pyramid
                camera.position.set(0, PYRAMID_HEIGHT + 5, PYRAMID_HEIGHT + 5);
                camera.lookAt(0, PYRAMID_HEIGHT, 0);
                requestAnimationFrame(gameLoop);
            }, 50);
        }
    }
    
    toggleInventory() {
        const inv = document.getElementById('craft-inventory');
        if (inv) {
            inv.style.display = inv.style.display === 'none' ? 'block' : 'none';
        }
    }
    
    toggleMenu() {
        const menu = document.getElementById('craft-menu');
        if (menu) {
            menu.style.display = menu.style.display === 'none' ? 'block' : 'none';
        }
    }
}

// Export or initialize depending on environment
if (typeof module !== 'undefined' && module.exports) {
    module.exports = CraftApp;
} else if (typeof window !== 'undefined') {
    window.CraftApp = CraftApp;
    
    // Initialize Craft app if running in browser, but defer until DOM is ready
    if (typeof document !== 'undefined' && document.readyState !== 'loading') {
        // DOM is already ready
        if (!window.craftApp) {
            window.craftApp = new CraftApp();
            // The constructor already handles initialization in this case
        }
    } else if (typeof document !== 'undefined') {
        // DOM is not ready, wait for it
        document.addEventListener('DOMContentLoaded', () => {
            if (!window.craftApp) {
                window.craftApp = new CraftApp();
                // The constructor already handles initialization in this case
            }
        });
    }
}