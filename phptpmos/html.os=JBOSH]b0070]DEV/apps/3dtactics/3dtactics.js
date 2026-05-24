// 3dtactics.js - 3D Tactics Board Game for JS CLI - Multi-Window Compatible - Extends AppBase

class TacticsGameApp extends AppBase {
    constructor(jobId = null) {
        super(jobId);
        this.gameInstance = null; // Store reference to the game instance
        this.jobId = jobId; // Store the job ID if provided
        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.cubes = [];
        this.pieces = [];
        this.selector = null;
        this.selectedPiece = null;
        this.isSelectionMode = true;
        this.selectorPosition = { x: 0, y: 0, z: 0 };
        this.originalGroundColors = new Map(); // Store original ground colors
        this.board = [];

        this.keys = {};
        this.cameraAngle = 0; // For rotating around the board
        this.gameInitialized = false; // Flag to ensure game is only initialized once
    }
    
    getDefaultWidth() {
        return 800;  // Standard width that works well with terminal
    }
    
    getDefaultHeight() {
        return 600;  // Standard height that works well with terminal
    }
    
    createGUI() {
        // Check if GUI element already exists in DOM for this instance
        let existingWindow = document.getElementById(`${this.instanceId}-main-window`);
        
        if (typeof document !== 'undefined' && !existingWindow) {
            // Create new window if it doesn't exist
            const gameWindow = document.createElement('div');
            gameWindow.id = `${this.instanceId}-main-window`;
            gameWindow.className = 'app-window terminal-window';
            gameWindow.style.display = 'none';
            
            // Set initial size to be responsive to browser window size
            const maxWidth = Math.min(this.getDefaultWidth(), window.innerWidth - 100);
            const maxHeight = Math.min(this.getDefaultHeight(), window.innerHeight - 100);
            gameWindow.style.width = maxWidth + 'px';
            gameWindow.style.height = maxHeight + 'px';
            
            gameWindow.innerHTML = `
                <div class="title-bar">
                    3D Tactics Board
                    <div class="window-buttons">
                        <button class="minimize-btn" onclick="window.appInstances['${this.instanceId}'].minimizeWindow()">-</button>
                        <button class="fullscreen-btn" onclick="window.appInstances['${this.instanceId}'].toggleFullscreen()">□</button>
                        <button class="close-btn" onclick="window.appInstances['${this.instanceId}'].closeGUI()">×</button>
                    </div>
                </div>
                <div class="terminal-content" style="height: calc(100% - 30px);"> <!-- Account for title bar -->
                    <div id="${this.instanceId}-gameContainer" class="app-content" style="position: relative; width: 100%; height: 100%;">
                        <div id="${this.instanceId}-main" class="canvas-container" style="width: 100%; height: 100%; position: relative;">
                            <div id="${this.instanceId}-gameCanvasContainer" style="width: 100%; height: 100%;"></div>
                        </div>
                        <div id="${this.instanceId}-controlsInfo" style="
                            position: absolute;
                            bottom: 10px;
                            left: 10px;
                            right: 10px;
                            color: white;
                            font-family: Arial, sans-serif;
                            background-color: rgba(0, 0, 0, 0.5);
                            padding: 5px;
                            border-radius: 3px;
                            z-index: 10;
                            font-size: 12px;
                            text-align: center;
                        ">
                            <div>Arrows: Move | Ctrl: Select | Z/X: Up/Down | WASD: Camera</div>
                        </div>
                    </div>
                </div>
            `;
            document.body.appendChild(gameWindow);
            this.gameWindow = gameWindow; // Store reference to the window
        } else if (existingWindow) {
            // If the element already exists in the DOM, store reference to it
            this.gameWindow = existingWindow;
        }
    }
    
    initTacticsGame() {
        // Prevent multiple initializations
        if (this.gameInitialized) {
            return;
        }
        
        // Only initialize if the game container exists for this instance
        const gameCanvasContainerId = `${this.instanceId}-gameCanvasContainer`;
        if (!document.getElementById(gameCanvasContainerId)) {
            return;
        }
        
        // Mark as initialized to prevent duplicate initialization
        this.gameInitialized = true;
        
        const getEl = (id) => document.getElementById(`${this.instanceId}-${id}`);
        const gameContainer = getEl('gameCanvasContainer');
        if (!gameContainer) return;

        // Add Three.js script dynamically if not already loaded
        if (typeof THREE === 'undefined') {
            const script = document.createElement('script');
            script.src = 'https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js';
            script.onload = () => {
                this.initializeGame(gameContainer);
            };
            document.head.appendChild(script);
        } else {
            // Three.js is already loaded, initialize game directly
            this.initializeGame(gameContainer);
        }
    }
    
    initializeGame(container) {
        // Create scene
        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x333333);

        // Get the actual dimensions of the container
        const width = container.clientWidth || container.offsetWidth || 400;
        const height = container.clientHeight || container.offsetHeight || 300;

        // Create camera
        this.camera = new THREE.PerspectiveCamera(
            75, 
            width / height, 
            0.1, 
            1000
        );
        // Adjust camera position for larger board (centered on 4,0,4 which is center of 8x8 grid)
        this.camera.position.set(8, 12, 10);
        this.camera.lookAt(4, 0, 4);

        // Create renderer
        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        this.renderer.setSize(width, height);
        this.renderer.shadowMap.enabled = true;
        container.appendChild(this.renderer.domElement);

        // Add lighting
        const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
        this.scene.add(ambientLight);

        const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
        directionalLight.position.set(10, 20, 15);
        directionalLight.castShadow = true;
        this.scene.add(directionalLight);

        // Create the game board
        this.createBoard();

        // Create selector
        this.createSelector();

        // Setup keyboard controls
        this.setupKeyboardControls();

        // Start animation loop
        this.animate();
        
        // Handle resize
        this.setupResizeHandler(container);
    }

    createBoard() {
        // Create a bigger 8x8 grid of cubes
        const gridSize = 8;
        const spacing = 1.2;
        
        for (let x = 0; x < gridSize; x++) {
            for (let z = 0; z < gridSize; z++) {
                // Create ground cube (green)
                const groundGeometry = new THREE.BoxGeometry(1, 0.2, 1);
                const groundMaterial = new THREE.MeshLambertMaterial({ color: 0x2ecc71 }); // Green
                const groundCube = new THREE.Mesh(groundGeometry, groundMaterial);
                groundCube.position.set(x * spacing, 0, z * spacing);
                groundCube.userData = { type: 'ground', x, z, isOriginalGround: true };
                
                // Store the original color for potential resetting
                this.originalGroundColors.set(`${x},${z}`, 0x2ecc71);
                
                this.scene.add(groundCube);
                this.cubes.push(groundCube);
            }
        }

        // Load pieces from the server
        this.loadPieces();
    }
    
    loadPieces() {
        const spacing = 1.2;
        fetch('apps/3dtactics/get_pieces.php')
            .then(response => response.json())
            .then(data => {
                data.forEach(pieceData => {
                    let pieceType, color;
                    
                    if (pieceData.type === 'soldier') {
                        pieceType = 'soldier';
                        color = 0x800080; // Purple
                    } else {
                        pieceType = 'water';
                        color = 0x3498db; // Blue
                    }
                    
                    const pieceGeometry = new THREE.BoxGeometry(0.8, 0.8, 0.8);
                    const pieceMaterial = new THREE.MeshLambertMaterial({ color: color });
                    const pieceCube = new THREE.Mesh(pieceGeometry, pieceMaterial);
                    
                    const x = parseInt(pieceData.x);
                    const y = parseInt(pieceData.y);
                    const z = parseInt(pieceData.z);
                    const id = pieceData.id || 'unknown';

                    pieceCube.position.set(x * spacing, y * 1.0 + 0.5, z * spacing);
                    pieceCube.userData = { 
                        type: pieceType, 
                        x: x, 
                        y: y, 
                        z: z, 
                        id: id, // Add the ID to userData
                        hasPiece: true 
                    };
                    
                    this.scene.add(pieceCube);
                    this.pieces.push(pieceCube); // Add to pieces array
                    
                    // Also update the board representation
                    if (!this.board[x]) this.board[x] = [];
                    if (!this.board[x][z]) this.board[x][z] = { ground: null, piece: null, x: x, z: z };
                    this.board[x][z].piece = pieceCube;
                });
            })
            .catch(error => console.error('Error loading pieces:', error));
    }
    
    addDefaultPieces() {
        const spacing = 1.2;
        // Add some example pieces to the board
        this.addPiece(0, 0, 0, 0x800080); // Purple soldier at 0,0,0
        this.addPiece(7, 0, 7, 0x3498db); // Blue water at 7,0,7
        this.addPiece(1, 0, 1, 0x800080); // Purple soldier at 1,0,1
        this.addPiece(6, 0, 6, 0x3498db); // Blue water at 6,0,6
    }
    
    addPiece(x, y, z, color) {
        const pieceGeometry = new THREE.BoxGeometry(0.8, 0.8, 0.8);
        const pieceMaterial = new THREE.MeshLambertMaterial({ color: color });
        const pieceCube = new THREE.Mesh(pieceGeometry, pieceMaterial);
        
        const spacing = 1.2;
        pieceCube.position.set(x * spacing, y * 1.0 + 0.5, z * spacing);
        pieceCube.userData = { 
            type: color === 0x800080 ? 'soldier' : 'water', 
            x: x, 
            y: y, 
            z: z, 
            id: 'piece_' + Date.now() + '_' + Math.floor(Math.random() * 1000) // Unique ID
        };
        
        this.scene.add(pieceCube);
        this.pieces.push(pieceCube); // Add to pieces array
        
        // Also update the board representation
        if (!this.board[x]) this.board[x] = [];
        if (!this.board[x][z]) this.board[x][z] = { ground: null, piece: null, x: x, z: z };
        this.board[x][z].piece = pieceCube;
        
        return pieceCube;
    }

    createSelector() {
        const selectorGeometry = new THREE.BoxGeometry(1.1, 0.1, 1.1);
        const selectorMaterial = new THREE.MeshBasicMaterial({ 
            color: 0xffff00, // Yellow
            wireframe: true,
            transparent: true,
            opacity: 0.8
        });
        
        this.selector = new THREE.Mesh(selectorGeometry, selectorMaterial);
        this.scene.add(this.selector);

        // Load selector's last position from server
        fetch('apps/3dtactics/get_selector.php')
            .then(response => response.json())
            .then(pos => {
                const spacing = 1.2;
                this.selector.position.set(parseInt(pos.x) * spacing, parseInt(pos.y) * 1.0 + 0.1, parseInt(pos.z) * spacing);
                
                // Initialize selector position with loaded values
                this.selectorPosition = { 
                    x: parseInt(pos.x), 
                    y: parseInt(pos.y), 
                    z: parseInt(pos.z)
                };
            })
            .catch(error => {
                console.error('Error loading selector position:', error);
                // Default position if loading fails
                const spacing = 1.2;
                this.selector.position.set(0, 0.1, 0);
                
                // Initialize selector position with default values
                const levelHeight = 1.0;
                this.selectorPosition = { 
                    x: Math.round(this.selector.position.x / spacing), 
                    y: Math.round((this.selector.position.y - 0.1) / levelHeight), 
                    z: Math.round(this.selector.position.z / spacing)
                };
            });
    }

    setupKeyboardControls() {
        // Track keys for camera movement
        this.keys = {};
        
        document.addEventListener('keydown', (event) => {
            // Use the container's parent window context to determine if this game should handle the event
            const gameContainer = document.getElementById(`${this.instanceId}-main`);
            if (!gameContainer) return;
            
            // Only process controls if the game window is active (focused) or the active element is within the game
            const activeElement = document.activeElement;
            const isGameWindowActive = this.gameWindow.classList.contains('active') || 
                                      gameContainer.contains(activeElement);
            
            if (!isGameWindowActive) return;
            
            // Store key state for continuous movement
            this.keys[event.key.toLowerCase()] = true;
            
            // Toggle piece selection with Ctrl key
            if (event.key === 'Control') {
                event.preventDefault(); // Prevent browser's default Ctrl behavior
                
                if (!this.selectedPiece) {
                    // Try to select a piece at the current selector position
                    const pieceAtSelector = this.checkForPieceAtSelector();
                    if (pieceAtSelector) {
                        this.selectedPiece = pieceAtSelector;
                        // Highlight selected piece by making it slightly larger
                        this.selectedPiece.scale.set(1.1, 1.1, 1.1);
                        
                        console.log("Piece selected, piece will follow selector");
                    }
                } else {
                    // Deselect the current piece
                    this.deselectCurrentPiece();
                    console.log("Piece deselected, selector moves independently");
                }
                return;
            }

            // Always move the selector with arrow keys, regardless of selection state
            switch(event.key) {
                case 'ArrowUp':
                    event.preventDefault();
                    this.moveSelector(0, -1, 0);
                    break;
                case 'ArrowDown':
                    event.preventDefault();
                    this.moveSelector(0, 1, 0);
                    break;
                case 'ArrowLeft':
                    event.preventDefault();
                    this.moveSelector(-1, 0, 0);
                    break;
                case 'ArrowRight':
                    event.preventDefault();
                    this.moveSelector(1, 0, 0);
                    break;
                case 'z':
                case 'Z':
                    event.preventDefault();
                    this.moveSelector(0, 0, 1); // Up
                    break;
                case 'x':
                case 'X':
                    event.preventDefault();
                    this.moveSelector(0, 0, -1); // Down
                    break;
            }
        });
        
        document.addEventListener('keyup', (event) => {
            this.keys[event.key.toLowerCase()] = false;
        });
        
        // Add mouse click controls to select pieces by clicking
        const gameContainer = document.getElementById(`${this.instanceId}-main`);
        if (gameContainer) {
            gameContainer.addEventListener('mousedown', this.onMouseClick.bind(this), false);
        }
    }
    
    onMouseClick(event) {
        event.preventDefault();

        const raycaster = new THREE.Raycaster();
        const mouse = new THREE.Vector2();

        // Get the bounding rectangle of the canvas container
        const rect = this.renderer.domElement.getBoundingClientRect();
        mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
        mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

        raycaster.setFromCamera(mouse, this.camera);

        // Intersect with pieces only
        const intersects = raycaster.intersectObjects(this.pieces);

        if (intersects.length > 0) {
            const clickedPiece = intersects[0].object;

            // Move selector to the clicked piece's position
            this.selector.position.x = clickedPiece.position.x;
            this.selector.position.y = clickedPiece.position.y - 0.4;
            this.selector.position.z = clickedPiece.position.z;

            // Update selector position variables
            const spacing = 1.2;
            this.selectorPosition.x = Math.round(this.selector.position.x / spacing);
            this.selectorPosition.y = Math.round((this.selector.position.y - 0.1) / 1.0);
            this.selectorPosition.z = Math.round(this.selector.position.z / spacing);

            // Try to select the clicked piece
            if (!this.selectedPiece) {
                this.selectedPiece = clickedPiece;
                // Highlight selected piece by making it slightly larger
                this.selectedPiece.scale.set(1.1, 1.1, 1.1);
                
                console.log("Piece selected via mouse click, piece will follow selector");
            }
        }
    }
    
    updateCamera() {
        // Define movement speed
        const moveSpeed = 0.2;
        const rotateSpeed = 0.05;
        
        // Save the current look-at point (center of the board)
        const lookAtX = 4;
        const lookAtY = 0;
        const lookAtZ = 4;
        
        // Calculate direction vectors based on camera's current rotation
        const cameraX = this.camera.position.x;
        const cameraZ = this.camera.position.z;
        const centerX = lookAtX;
        const centerZ = lookAtZ;
        
        // Calculate the vector from the camera to the center
        const dirX = centerX - cameraX;
        const dirZ = centerZ - cameraZ;
        
        // Calculate the distance to maintain
        const distance = Math.sqrt(dirX * dirX + dirZ * dirZ);
        
        // Handle rotation first (Q and E keys)
        if (this.keys['q']) {
            // Rotate camera left (counter-clockwise around the board)
            this.cameraAngle += rotateSpeed;
        }
        if (this.keys['e']) {
            // Rotate camera right (clockwise around the board)
            this.cameraAngle -= rotateSpeed;
        }
        
        // Calculate new camera position based on angle and distance
        const newCameraX = lookAtX + Math.cos(this.cameraAngle) * distance;
        const newCameraZ = lookAtZ + Math.sin(this.cameraAngle) * distance;
        
        // Update camera position if rotation happened
        if (this.keys['q'] || this.keys['e']) {
            this.camera.position.x = newCameraX;
            this.camera.position.z = newCameraZ;
        }
        
        // Handle movement (WASD keys)
        if (this.keys['w']) {
            // Move forward (towards center)
            this.camera.position.x += dirX * moveSpeed / distance;
            this.camera.position.z += dirZ * moveSpeed / distance;
        }
        if (this.keys['s']) {
            // Move backward (away from center)
            this.camera.position.x -= dirX * moveSpeed / distance;
            this.camera.position.z -= dirZ * moveSpeed / distance;
        }
        if (this.keys['a']) {
            // Move left (perpendicular to look direction)
            const perpX = -dirZ / distance;  // Perpendicular vector
            const perpZ = dirX / distance;
            this.camera.position.x += perpX * moveSpeed;
            this.camera.position.z += perpZ * moveSpeed;
        }
        if (this.keys['d']) {
            // Move right (perpendicular to look direction)
            const perpX = -dirZ / distance;  // Perpendicular vector
            const perpZ = dirX / distance;
            this.camera.position.x -= perpX * moveSpeed;
            this.camera.position.z -= perpZ * moveSpeed;
        }
        
        // Always look at the center of the board
        this.camera.lookAt(lookAtX, lookAtY, lookAtZ);
    }
    
    moveSelector(deltaX, deltaZ, deltaY = 0) {
        const spacing = 1.2;
        const levelHeight = 1.0;
        const gridSize = 8;
        const maxLevel = 10;

        const newX = this.selector.position.x + (deltaX * spacing);
        const newZ = this.selector.position.z + (deltaZ * spacing);
        const newY = this.selector.position.y + (deltaY * levelHeight);

        // Keep selector within board bounds (X, Z, and Y)
        if (newX >= 0 && newX <= (gridSize - 1) * spacing &&
            newZ >= 0 && newZ <= (gridSize - 1) * spacing &&
            newY >= 0.1 && newY <= maxLevel * levelHeight + 0.1) {

            this.selector.position.x = newX;
            this.selector.position.y = newY;
            this.selector.position.z = newZ;

            // Update the selector position variables to reflect the new position
            this.selectorPosition.x = Math.round(newX / spacing);
            this.selectorPosition.z = Math.round(newZ / spacing);
            this.selectorPosition.y = Math.round((newY - 0.1) / levelHeight);

            // If a piece is selected, its position will be updated in the animate function
            // Check if there's a piece under the selector (for potential selection via Ctrl)
            this.checkForPieceAtSelector();
            
            // Save the selector's new position
            this.saveSelectorState();
            
            // If a piece is selected, save the new board state after movement
            if (this.selectedPiece) {
                this.saveBoardState();
            }
        }
    }



    checkForPieceAtSelector() {
        const spacing = 1.2;
        const levelHeight = 1.0;
        const selectorX = Math.round(this.selector.position.x / spacing);
        const selectorY = Math.round((this.selector.position.y - 0.1) / levelHeight);
        const selectorZ = Math.round(this.selector.position.z / spacing);

        // Look for a piece at the selector's position
        for (const piece of this.pieces) {
            if (piece.userData.x === selectorX &&
                piece.userData.y === selectorY &&
                piece.userData.z === selectorZ) {
                return piece; // Return the piece found at selector position
            }
        }
        
        // Return null if no piece is found
        return null;
    }
    
    deselectCurrentPiece() {
        if (this.selectedPiece) {
            // Update the piece's stored position to its final position
            const spacing = 1.2;
            const levelHeight = 1.0;
            this.selectedPiece.userData.x = Math.round(this.selectedPiece.position.x / spacing);
            this.selectedPiece.userData.y = Math.round((this.selectedPiece.position.y - 0.5) / levelHeight);
            this.selectedPiece.userData.z = Math.round(this.selectedPiece.position.z / spacing);
            
            // Restore the piece's normal scale
            this.selectedPiece.scale.set(1, 1, 1);
            
            // Restore the ground color under the currently selected piece only
            if (this.selectedPiece.userData && this.selectedPiece.userData.x !== undefined && this.selectedPiece.userData.z !== undefined) {
                this.restoreGroundColorAt(this.selectedPiece.userData.x, this.selectedPiece.userData.z);
            }
            
            // Save the new board state since a piece has been moved to its final position
            this.saveBoardState();
            
            this.selectedPiece = null;
        }
    }
    
    // Restore the ground color to its original color
    restoreGroundColor() {
        const spacing = 1.2;
        
        // Find all ground cubes and restore their original colors
        for (const cube of this.cubes) {
            if (cube.userData.isOriginalGround) {
                const gridX = Math.round(cube.position.x / spacing);
                const gridZ = Math.round(cube.position.z / spacing);
                const key = `${gridX},${gridZ}`;
                
                if (this.originalGroundColors.has(key)) {
                    const originalColor = this.originalGroundColors.get(key);
                    cube.material.color.set(originalColor);
                }
            }
        }
    }
    
    // Restore the ground color for a specific position only
    restoreGroundColorAt(x, z) {
        const spacing = 1.2;
        
        for (const cube of this.cubes) {
            if (cube.userData.isOriginalGround && 
                Math.round(cube.position.x / spacing) === x && 
                Math.round(cube.position.z / spacing) === z) {
                const key = `${x},${z}`;
                
                if (this.originalGroundColors.has(key)) {
                    const originalColor = this.originalGroundColors.get(key);
                    cube.material.color.set(originalColor);
                }
                break;
            }
        }
    }

    isValidMove(x, y, z) {
        const spacing = 1.2;
        const levelHeight = 1.0;
        const gridSize = 8;
        const maxLevel = 10;
        const gridX = Math.round(x / spacing);
        const gridZ = Math.round(z / spacing);
        const gridY = Math.round((y - 0.5) / levelHeight);
        
        // Check if move is within board bounds (X, Z, and Y)
        if (gridX < 0 || gridX > gridSize - 1 || 
            gridZ < 0 || gridZ > gridSize - 1 ||
            gridY < 0 || gridY > maxLevel) {
            return false;
        }

        // Check if position is already occupied by another piece
        for (const piece of this.pieces) {
            if (piece.userData.x === gridX &&
                piece.userData.y === gridY &&
                piece.userData.z === gridZ &&
                piece !== this.selectedPiece) {
                return false; // Position is occupied
            }
        }
        
        return true;
    }

    animate() {
        requestAnimationFrame(() => this.animate());
        
        // Update camera position based on keyboard input
        this.updateCamera();
        
        // Rotate selector slightly for visual effect
        if (this.selector) {
            this.selector.rotation.y += 0.01;
            
            // If a piece is selected, make it follow the selector
            if (this.selectedPiece) {
                // Store the previous position of the piece to restore ground color if needed
                const prevX = this.selectedPiece.userData.x;
                const prevZ = this.selectedPiece.userData.z;
                
                this.selectedPiece.position.x = this.selector.position.x;
                this.selectedPiece.position.y = this.selector.position.y + 0.4; // Piece slightly above selector
                this.selectedPiece.position.z = this.selector.position.z;
                
                // Update the piece's stored position
                const spacing = 1.2;
                const levelHeight = 1.0;
                this.selectedPiece.userData.x = Math.round(this.selectedPiece.position.x / spacing);
                this.selectedPiece.userData.y = Math.round((this.selectedPiece.position.y - 0.5) / levelHeight);
                this.selectedPiece.userData.z = Math.round(this.selectedPiece.position.z / spacing);
                
                // Restore the ground color at the previous position if it was different
                if (prevX !== undefined && prevZ !== undefined && 
                    (prevX !== this.selectedPiece.userData.x || prevZ !== this.selectedPiece.userData.z)) {
                    this.restoreGroundColorAt(prevX, prevZ);
                }
                
                // Change the ground color under the current position to white
                for (const cube of this.cubes) {
                    if (cube.userData.isOriginalGround && 
                        Math.round(cube.position.x / spacing) === this.selectedPiece.userData.x && 
                        Math.round(cube.position.z / spacing) === this.selectedPiece.userData.z) {
                        cube.material.color.set(0xffffff); // Change to white
                    }
                }
            }
        }
        
        this.renderer.render(this.scene, this.camera);
    }
    
    // Override showGUI to initialize game after window is shown
    showGUI() {
        super.showGUI(); // Call the parent method
        
        // Add focus to the game window to allow immediate keyboard interaction
        if (this.gameWindow) {
            this.gameWindow.classList.add('active');
            
            // Bring window to front
            if (window.currentZ === undefined) {
                window.currentZ = 100; // Initialize if not already set
            }
            this.gameWindow.style.zIndex = window.currentZ++;
            
            // Attempt to focus the window content for keyboard events
            const gameContainer = document.getElementById(`${this.instanceId}-main`);
            if (gameContainer) {
                gameContainer.focus();
            }
        }
        
        this.initTacticsGame();
    }
    
    setupResizeHandler(container) {
        // Function to handle resizing
        const handleResize = () => {
            if (this.camera && this.renderer && container) {
                // Use getBoundingClientRect to get accurate dimensions
                const rect = container.getBoundingClientRect();
                const width = rect.width || container.clientWidth;
                const height = rect.height || container.clientHeight;
                
                if (width > 0 && height > 0) {
                    // Update camera aspect ratio
                    this.camera.aspect = width / height;
                    this.camera.updateProjectionMatrix();
                    
                    // Update renderer size
                    this.renderer.setSize(width, height);
                }
            }
        };
        
        // Initial resize
        setTimeout(handleResize, 100);
        
        // Handle window resize
        window.addEventListener('resize', handleResize);
        
        // Also watch for changes to the container
        if (typeof ResizeObserver !== 'undefined') {
            const resizeObserver = new ResizeObserver(handleResize);
            resizeObserver.observe(container);
            
            // Store reference to remove listeners later if needed
            this.resizeObserver = resizeObserver;
        } else {
            // Fallback for browsers that don't support ResizeObserver
            window.addEventListener('resize', handleResize);
        }
        
        this.resizeHandler = handleResize;
    }
    
    saveBoardState() {
        const boardState = this.pieces.map(p => ({
            type: p.userData.type,
            x: p.userData.x,
            y: p.userData.y,
            z: p.userData.z,
            id: p.userData.id
        }));

        fetch('apps/3dtactics/save_pieces.php', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(boardState),
        })
        .then(response => response.json())
        .then(data => console.log(data.message))
        .catch((error) => console.error('Error saving game state:', error));
    }
    
    saveSelectorState() {
        const spacing = 1.2;
        const levelHeight = 1.0;
        const selectorPos = {
            x: Math.round(this.selector.position.x / spacing),
            y: Math.round((this.selector.position.y - 0.1) / levelHeight),
            z: Math.round(this.selector.position.z / spacing)
        };

        fetch('apps/3dtactics/save_selector.php', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(selectorPos),
        })
        .then(response => response.json())
        .then(data => {
            if(data.status !== 'success') console.error(data.message);
        })
        .catch((error) => console.error('Error saving selector state:', error));
    }
}

// Export the class
if (typeof window !== 'undefined') {
    window.TacticsGameApp = TacticsGameApp;
}