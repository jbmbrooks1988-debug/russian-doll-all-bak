let currentView = VIEW_MODE._3D;

// Get containers for both views
const container = document.getElementById('canvasContainer');
const view2DCanvas = document.getElementById('view2DCanvas');
const view2DContext = view2DCanvas.getContext('2d');

// Set up 2D canvas
view2DCanvas.width = container.clientWidth;
view2DCanvas.height = container.clientHeight;

// Three.js setup
const scene = new THREE.Scene();
scene.background = new THREE.Color(0x87CEEB); // Sky blue

const camera = new THREE.PerspectiveCamera(75, container.clientWidth / container.clientHeight, 0.1, 1000);

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(container.clientWidth, container.clientHeight);
container.appendChild(renderer.domElement);

// Make sure renderer is visible by default
renderer.domElement.style.display = 'block';

// Simple lighting
const ambientLight = new THREE.AmbientLight(0x404040, 1);
scene.add(ambientLight);

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
const coordinatesDisplay = document.getElementById('coordinates');

// Update minimap (simple for now)
const minimapCanvas = document.getElementById('minimapCanvas');
const minimapCtx = minimapCanvas.getContext('2d');

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
    const width = container.clientWidth;
    const height = container.clientHeight;
    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    renderer.setSize(width, height);
    view2DCanvas.width = width;
    view2DCanvas.height = height;
}

function toggleInventory() {
    const inv = document.getElementById('inventory');
    inv.style.display = inv.style.display === 'none' ? 'block' : 'none';
}

function toggleMenu() {
    const menu = document.getElementById('menu');
    menu.style.display = menu.style.display === 'none' ? 'block' : 'none';
}
