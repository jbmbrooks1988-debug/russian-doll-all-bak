// Main game file
class TacticalGame {
    constructor() {
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
        
        // Make this instance globally accessible so other code can interact with it
        window.tacticalGame = this;
        
        this.init();
    }

    init() {
        // Create scene
        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x333333);

        // Create camera
        this.camera = new THREE.PerspectiveCamera(
            75, 
            window.innerWidth / window.innerHeight, 
            0.1, 
            1000
        );
        // Adjust camera position for larger board (centered on 4,0,4 which is center of 8x8 grid)
        this.camera.position.set(8, 12, 10);
        this.camera.lookAt(4, 0, 4);
        
        // Camera rotation variables
        this.cameraAngle = 0; // For rotating around the board

        // Create renderer
        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        this.renderer.setSize(window.innerWidth, window.innerHeight);
        this.renderer.shadowMap.enabled = true;
        document.getElementById('gameContainer').appendChild(this.renderer.domElement);

        // Add lighting
        const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
        this.scene.add(ambientLight);

        const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
        directionalLight.position.set(10, 20, 15);
        directionalLight.castShadow = true;
        this.scene.add(directionalLight);

        // Create the game board
        this.createBoard();
        // this.createVerticalGrid(); // Removed as per user request

        // Create selector
        this.createSelector();

        // Handle window resize
        window.addEventListener('resize', () => this.onWindowResize());

        // Setup keyboard controls
        this.setupKeyboardControls();

        // Add mouse controls
        document.getElementById('gameContainer').addEventListener('mousedown', this.onMouseClick.bind(this), false);

        // Start animation loop
        this.animate();
    }

    onMouseClick(event) {
        event.preventDefault();

        const raycaster = new THREE.Raycaster();
        const mouse = new THREE.Vector2();

        mouse.x = (event.clientX / window.innerWidth) * 2 - 1;
        mouse.y = -(event.clientY / window.innerHeight) * 2 + 1;

        raycaster.setFromCamera(mouse, this.camera);

        const intersects = raycaster.intersectObjects(this.pieces);

        if (intersects.length > 0) {
            const clickedPiece = intersects[0].object;

            // Move selector to the clicked piece's position
            this.selector.position.x = clickedPiece.position.x;
            this.selector.position.y = clickedPiece.position.y - 0.4;
            this.selector.position.z = clickedPiece.position.z;

            this.saveSelectorState();
            this.checkForPieceAtSelector();
        }
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
        fetch('get_pieces.php')
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
                });
            })
            .catch(error => console.error('Error loading pieces:', error));
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

        // Check for a selected piece position from the text menu (localStorage)
        const selectedPiecePos = localStorage.getItem('selectedPiecePosition');
        if (selectedPiecePos) {
            try {
                const pos = JSON.parse(selectedPiecePos);
                // Clear the stored position so it doesn't persist
                localStorage.removeItem('selectedPiecePosition');
                
                // Position the selector on the specified piece immediately
                const spacing = 1.2;
                this.selector.position.set(parseInt(pos.x) * spacing, parseInt(pos.y) * 1.0 + 0.1, parseInt(pos.z) * spacing);
                
                // Attempt to select the piece at that position
                this.attemptSelectPieceAtPosition(pos.x, pos.y, pos.z);
                
                console.log(`Positioned selector at: (${pos.x}, ${pos.y}, ${pos.z}) from text menu selection`);
                return; // Skip loading from server since we have a specific position from the text menu
            } catch (e) {
                console.error('Error parsing selected piece position from localStorage:', e);
            }
        }

        // If no specific position from text menu, load selector's last position from server
        fetch('get_selector.php')
            .then(response => response.json())
            .then(pos => {
                const spacing = 1.2;
                this.selector.position.set(parseInt(pos.x) * spacing, parseInt(pos.y) * 1.0 + 0.1, parseInt(pos.z) * spacing);
            })
            .catch(error => {
                console.error('Error loading selector position:', error);
                this.selector.position.set(0, 0.1, 0); // Default position on error
            });
    }

    // Attempt to select a piece at the given position
    attemptSelectPieceAtPosition(x, y, z) {
        const spacing = 1.2;
        const levelHeight = 1.0;
        
        // Look for a piece at the specified coordinates
        for (const piece of this.pieces) {
            if (piece.userData.x === x && piece.userData.y === y && piece.userData.z === z) {
                // Select this piece for movement (similar to pressing Ctrl when selector is on a piece)
                this.deselectCurrentPiece();
                this.selectedPiece = piece;
                
                // Highlight selected piece by making it slightly larger
                piece.scale.set(1.1, 1.1, 1.1);
                
                // Switch to piece movement mode
                this.currentMode = 'piece';
                
                // Change the ground color under the selected piece to white
                for (const cube of this.cubes) {
                    if (cube.userData.isOriginalGround && 
                        Math.round(cube.position.x / spacing) === x && 
                        Math.round(cube.position.z / spacing) === z) {
                        cube.material.color.set(0xffffff); // Change to white
                        break;
                    }
                }
                
                console.log(`Selected piece at (${x}, ${y}, ${z}) from text menu selection`);
                break;
            }
        }
    }

    setupKeyboardControls() {
        // Track the control mode: 'selector' or 'piece'
        this.currentMode = 'selector'; // Start in selector mode
        
        // Track keys for camera movement
        this.keys = {};
        
        document.addEventListener('keydown', (event) => {
            this.keys[event.key.toLowerCase()] = true;
            
            // Toggle between selector and piece movement mode with Ctrl key
            if (event.key === 'Control') {
                event.preventDefault(); // Prevent browser's default Ctrl behavior
                
                if (this.currentMode === 'selector') {
                    // Try to select a piece at the current selector position
                    this.checkForPieceAtSelector();
                    
                    if (this.selectedPiece) {
                        // Switch to piece movement mode
                        this.currentMode = 'piece';
                        console.log("Switched to piece movement mode");
                        this.followPiece(); // Change ground color to white and attach selector to piece
                    }

                } else if (this.currentMode === 'piece') {
                    // Switch back to selector mode
                    this.currentMode = 'selector';
                    this.deselectCurrentPiece(); // Deselect and restore ground color
                    console.log("Switched back to selector mode");
                }
                return;
            }

            // Handle movement based on current mode
            if (this.currentMode === 'selector') {
                // Move selector when in selector mode
                switch(event.key) {
                    case 'ArrowUp':
                        this.moveSelector(0, -1, 0);
                        break;
                    case 'ArrowDown':
                        this.moveSelector(0, 1, 0);
                        break;
                    case 'ArrowLeft':
                        this.moveSelector(-1, 0, 0);
                        break;
                    case 'ArrowRight':
                        this.moveSelector(1, 0, 0);
                        break;
                    case 'z':
                        this.moveSelector(0, 0, 1); // Up
                        break;
                    case 'x':
                        this.moveSelector(0, 0, -1); // Down
                        break;
                }
            } else if (this.currentMode === 'piece' && this.selectedPiece) {
                // Move selected piece when in piece movement mode
                switch(event.key) {
                    case 'ArrowUp':
                        this.moveSelectedPiece(0, 0, -1);
                        break;
                    case 'ArrowDown':
                        this.moveSelectedPiece(0, 0, 1);
                        break;
                    case 'ArrowLeft':
                        this.moveSelectedPiece(-1, 0, 0);
                        break;
                    case 'ArrowRight':
                        this.moveSelectedPiece(1, 0, 0);
                        break;
                    case 'z':
                        this.moveSelectedPiece(0, 1, 0); // Up
                        break;
                    case 'x':
                        this.moveSelectedPiece(0, -1, 0); // Down
                        break;
                }
            }
        });
        
        document.addEventListener('keyup', (event) => {
            this.keys[event.key.toLowerCase()] = false;
        });
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
        const centerZ = lookAtZ;  // Fixed: was "CenterZ"
        
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

            // Check if there's a piece under the selector to potentially select
            this.checkForPieceAtSelector();
            
            // Save the selector's new position
            this.saveSelectorState();
        }
    }

    saveSelectorState() {
        const spacing = 1.2;
        const levelHeight = 1.0;
        const selectorPos = {
            x: Math.round(this.selector.position.x / spacing),
            y: Math.round((this.selector.position.y - 0.1) / levelHeight),
            z: Math.round(this.selector.position.z / spacing)
        };

        fetch('save_selector.php', {
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

    moveSelectedPiece(deltaX, deltaY, deltaZ) {
        if (!this.selectedPiece) return;

        const spacing = 1.2;
        const levelHeight = 1.0;
        const newX = this.selectedPiece.position.x + (deltaX * spacing);
        const newY = this.selectedPiece.position.y + (deltaY * levelHeight);
        const newZ = this.selectedPiece.position.z + (deltaZ * spacing);

        // Check if the new position is valid (within bounds and not occupied)
        if (this.isValidMove(newX, newY, newZ)) {
            // Get the old position before moving
            const oldX = this.selectedPiece.userData.x;
            const oldZ = this.selectedPiece.userData.z;
            
            // Restore the ground color at the old position
            this.restoreGroundColorAt(oldX, oldZ);
            
            // Update piece position
            this.selectedPiece.position.x = newX;
            this.selectedPiece.position.y = newY;
            this.selectedPiece.position.z = newZ;
            
            // Update the piece's stored position
            this.selectedPiece.userData.x = Math.round(newX / spacing);
            this.selectedPiece.userData.y = Math.round((newY - 0.5) / levelHeight);
            this.selectedPiece.userData.z = Math.round(newZ / spacing);
            
            // Update the selector to follow the piece
            this.selector.position.x = newX;
            this.selector.position.y = newY;
            this.selector.position.z = newZ;
            
            // Change the ground color under the new position to white
            for (const cube of this.cubes) {
                if (cube.userData.isOriginalGround && 
                    Math.round(cube.position.x / spacing) === this.selectedPiece.userData.x && 
                    Math.round(cube.position.z / spacing) === this.selectedPiece.userData.z) {
                    cube.material.color.set(0xffffff); // Change to white
                    break;
                }
            }

            // Save the new board state to the server
            this.saveBoardState();
        }
    }

    saveBoardState() {
        const boardState = this.pieces.map(p => ({
            type: p.userData.type,
            x: p.userData.x,
            y: p.userData.y,
            z: p.userData.z
        }));

        fetch('save_pieces.php', {
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
                // Select the piece if we haven't already selected it
                if (this.selectedPiece !== piece) {
                    this.deselectCurrentPiece();
                    this.selectedPiece = piece;
                    // Highlight selected piece by making it slightly larger
                    piece.scale.set(1.1, 1.1, 1.1);
                    console.log(`Selected ${piece.userData.type} at (${selectorX}, ${selectorY}, ${selectorZ})`);
                }
                return; // Found a piece, exit the function
            }
        }
        
        // If no piece is found at the selector position, we should only deselect 
        // if we're currently in selector mode (not in piece move mode)
        if (this.currentMode === 'selector') {
            this.deselectCurrentPiece();
        }
    }
    
    // Make the selector follow the selected piece and change the ground color underneath to white
    followPiece() {
        if (this.selectedPiece) {
            // Move the selector to follow the piece
            this.selector.position.x = this.selectedPiece.position.x;
            this.selector.position.y = this.selectedPiece.position.y - 0.4;
            this.selector.position.z = this.selectedPiece.position.z;
            
            // Find the ground cube under the selected piece and change its color to white
            const spacing = 1.2;
            this.selectedPiece.userData.x = Math.round(this.selectedPiece.position.x / spacing);
            this.selectedPiece.userData.z = Math.round(this.selectedPiece.position.z / spacing);
            
            for (const cube of this.cubes) {
                if (cube.userData.isOriginalGround && 
                    Math.round(cube.position.x / spacing) === this.selectedPiece.userData.x && 
                    Math.round(cube.position.z / spacing) === this.selectedPiece.userData.z) {
                    cube.material.color.set(0xffffff); // Change to white
                    break;
                }
            }
        }
    }
    
    // Detach the selector from the piece and position it where the piece currently is
    detachSelector() {
        if (this.selectedPiece) {
            // Position selector at the piece's current location
            this.selector.position.x = this.selectedPiece.position.x;
            this.selector.position.y = this.selectedPiece.position.y - 0.4;
            this.selector.position.z = this.selectedPiece.position.z;
            
            // Don't change the ground color when detaching, since the piece is still there
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

    deselectCurrentPiece() {
        if (this.selectedPiece) {
            this.selectedPiece.scale.set(1, 1, 1);
            
            // Restore the ground color under the currently selected piece only
            if (this.selectedPiece.userData && this.selectedPiece.userData.x !== undefined && this.selectedPiece.userData.z !== undefined) {
                this.restoreGroundColorAt(this.selectedPiece.userData.x, this.selectedPiece.userData.z);
            }
            
            this.selectedPiece = null;
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

    onWindowResize() {
        this.camera.aspect = window.innerWidth / window.innerHeight;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(window.innerWidth, window.innerHeight);
    }

    animate() {
        requestAnimationFrame(() => this.animate());
        
        // Update camera position based on keyboard input
        this.updateCamera();
        
        // Rotate selector slightly for visual effect when in selector mode
        if (this.selector) {
            if (this.currentMode === 'selector') {
                this.selector.rotation.y += 0.01;
            } else if (this.currentMode === 'piece' && this.selectedPiece) {
                // When in piece movement mode, make sure selector follows the piece
                this.selector.position.x = this.selectedPiece.position.x;
                this.selector.position.z = this.selectedPiece.position.z;
            }
        }
        
        this.renderer.render(this.scene, this.camera);
    }
}

// Start the game when the page loads
window.addEventListener('load', () => {
    new TacticalGame();
});

// Check for selected piece position from the text-based menu when the page loads
window.addEventListener('load', () => {
    // Wait a short time for the game to initialize, then check for selected piece
    setTimeout(() => {
        const selectedPiecePos = localStorage.getItem('selectedPiecePosition');
        if (selectedPiecePos) {
            try {
                const pos = JSON.parse(selectedPiecePos);
                
                // Clear the stored position so it doesn't persist for future visits
                localStorage.removeItem('selectedPiecePosition');
                
                // Find the global TacticalGame instance and position the selector
                // This assumes the TacticalGame instance is available globally or can be accessed
                console.log(`Attempting to position selector at: (${pos.x}, ${pos.y}, ${pos.z})`);
                
                // The positioning will be handled by the TacticalGame class once we've modified it
            } catch (e) {
                console.error('Error parsing selected piece position:', e);
            }
        }
    }, 1000); // Wait 1 second for the game to initialize
});
