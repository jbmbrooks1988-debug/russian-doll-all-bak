// View mode state
const VIEW_MODE = {
    _3D: '3d',
    _2D: '2d'
};

// Block colors (simple)
const BLOCK_COLORS = {
    GRASS: 0x228B22,
    DIRT: 0x8B4513,
    STONE: 0x808080,
    WOOD: 0xA0522D,
    LEAVES: 0x006400
};

// Store blocks for mining/placing
// Updated to support the required schema: x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z
const blocks = new Map(); // key: `${x}-${y}-${z}`, value: {mesh, type, emoji_idx, fg_color_idx, alpha, scale, rel_x, rel_y, rel_z}

// Player properties (3D version of the example)
const player = {
    x: 0,
    y: 0, // Initial y will be set after planet generation
    z: 0,
    velocityY: 0,
    rotationX: 0, // Yaw
    rotationY: 0, // Pitch
    onGround: false,
    speed: 5, // units per second
    jumpStrength: 8,
    height: 1.8
};

// Track pressed keys
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

// Constants
const GRAVITY = -30;
const PYRAMID_SIZE = 16;
const PYRAMID_HEIGHT = 8;
const centerX = 0;
const centerZ = 0;
const baseY = 5;

// Function to create a block mesh
// Updated to support the required schema: x,y,z,emoji_idx,fg_color_idx,alpha,scale,rel_x,rel_y,rel_z
function createBlock(x, y, z, color, type = 'STONE', emoji_idx = 0, fg_color_idx = 0, alpha = 1, scale = 1, rel_x = 0, rel_y = 0, rel_z = 0) {
    const geometry = new THREE.BoxGeometry(scale, scale, scale);
    const material = new THREE.MeshLambertMaterial({ 
        color: color,
        transparent: alpha < 1,
        opacity: alpha
    });
    const block = new THREE.Mesh(geometry, material);
    block.position.set(x + rel_x, y + rel_y, z + rel_z);
    scene.add(block);
    const key = `${Math.floor(x)}-${Math.floor(y)}-${Math.floor(z)}`;
    blocks.set(key, {
        mesh: block, 
        type,
        emoji_idx,
        fg_color_idx,
        alpha,
        scale,
        rel_x,
        rel_y,
        rel_z
    });
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
                
                // Use our updated createBlock function with all parameters
                createBlock(x, y, z, color, type, 0, 0, 1, 1, 0, 0, 0);
            }
        }
    }
    
    // Add random obstacles (e.g., small "rocks" or "trees" on top)
    const numObstacles = 10;
    for (let i = 0; i < numObstacles; i++) {
        // Random position on top layer
        const topY = baseY + PYRAMID_HEIGHT - 1;
        const randX = centerX + (Math.random() - 0.5) * (PYRAMID_SIZE - 2);
        const randZ = centerZ + (Math.random() - 0.5) * (PYRAMID_SIZE - 2);
        
        // Simple "rock": 1-3 stacked stones
        const height = 1 + Math.floor(Math.random() * 3);
        for (let h = 0; h < height; h++) {
            createBlock(randX, topY + h, randZ, BLOCK_COLORS.STONE, 'STONE', 0, 0, 1, 1, 0, 0, 0);
        }
    }
    player.y = baseY + PYRAMID_HEIGHT + 1.8;
}

// Simple collision with pyramid (basic: check if below player is "ground")
function isOnGround(px, py, pz) {
    // Approximate pyramid: for a given x,z, max y is baseY + height - distance from center
    const dist = Math.max(Math.abs(px - centerX), Math.abs(pz - centerZ));
    const maxHeight = PYRAMID_HEIGHT - Math.floor(dist);
    return py <= baseY + maxHeight + 0.1;
}
