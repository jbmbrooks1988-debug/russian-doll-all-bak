// Add button event listeners for view switching
document.getElementById('textBtn').addEventListener('click', function() { // text button
    // Future implementation
});

document.getElementById('platBtn').addEventListener('click', function() { // plat button
    // Future implementation
});

document.getElementById('view2DBtn').addEventListener('click', function() { // 2d button
    switchTo2DView();
});

document.getElementById('view3DBtn').addEventListener('click', function() { // 3d button
    switchTo3DView();
});

// Add event listeners for project selection
document.querySelectorAll('.project-btn').forEach(button => {
    button.addEventListener('click', (e) => {
        const projectName = e.target.dataset.project;
        if (projectManager.setCurrentProject(projectName)) {
            updateFileViewer(projectName);
            console.log(`Switched to project: ${projectName}`);
        } else {
            console.error(`Failed to switch to project: ${projectName}`);
        }
    });
});

// Update the file viewer update function to handle map loading
function updateFileViewer(projectName) {
    const fileViewer = document.getElementById('file-viewer');
    fileViewer.innerHTML = ''; // Clear existing files

    const files = projectManager.getProjectFiles(projectName) || [];
    files.forEach(fileName => {
        const item = document.createElement('div');
        item.className = 'file-item';
        item.textContent = fileName;
        item.addEventListener('click', () => {
            // Check if it's a map file (ends with .txt but not _events.txt)
            if (fileName.endsWith('.txt') && !fileName.includes('_events.txt')) {
                // Load the map
                if (projectManager.loadMap(projectName, fileName)) {
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

// File operation button listeners
document.getElementById('newBtn').addEventListener('click', function() {
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



document.getElementById('saveBtn').addEventListener('click', function() {
    // Get current project and generate a unique map name
    const projectName = projectManager.getCurrentProject();
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const mapName = `map_${timestamp}.txt`;
    
    // Save the current world state
    if (projectManager.saveMap(projectName, mapName)) {
        alert(`World saved successfully as ${mapName} in project ${projectName}!`);
        
        // Update the file viewer to show the new map
        updateFileViewer(projectName);
    } else {
        alert('Failed to save the world.');
    }
});

// Event listeners for key presses
window.addEventListener('keydown', (e) => {
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
        toggleInventory();
        e.preventDefault();
    }
    if (e.code === 'KeyM') {
        toggleMenu();
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

window.addEventListener('keyup', (e) => {
    console.log('Key up:', e.key); // Debug
    const key = e.code;
    if (key in keys) {
        keys[key] = false;
    }
});

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
            createBlock(px + 0.5, py + 0.5, pz + 0.5, BLOCK_COLORS.STONE, 'STONE', 0, 0, 1, 1, 0, 0, 0);
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
document.addEventListener('click', () => {
    if (!pickerMode) {
        if (!mouseLocked) {
            document.body.requestPointerLock();
        }
    }
});

document.addEventListener('pointerlockchange', () => {
    mouseLocked = document.pointerLockElement === document.body;
});

document.addEventListener('mousemove', (e) => {
    if (mouseLocked && !pickerMode) {
        player.rotationX += e.movementX * 0.002;
        player.rotationY -= e.movementY * 0.002;
        player.rotationY = Math.max(-90, Math.min(90, player.rotationY));
    }
});

// Prevent context menu
document.addEventListener('contextmenu', (e) => e.preventDefault());

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
    
    // CTRL for block control
    if (keys.ControlLeft || keys.ControlRight) {
        controlBlock();
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
        render2D();
    }
    
    // Update coordinates
    coordinatesDisplay.textContent = `X: ${player.x.toFixed(1)}, Y: ${player.y.toFixed(1)}, Z: ${player.z.toFixed(1)}`;
    
    // Update minimap (top-down view of planet)
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
    
    // Continue game loop
    requestAnimationFrame(gameLoop);
}

// Initial setup
resizeRenderer();
generatePyramidPlanet();
switchTo3DView(); // Ensure 3D view is active at startup
requestAnimationFrame(gameLoop);
