const M_PI = Math.PI;
const NUM_ENEMIES = 3;
const MAX_PROJECTILES = 10;
const MAX_ENEMY_PROJECTILES = 20;
const SENSITIVITY = 0.1;
const MOVE_SPEED = 0.1;
const PROJECTILE_SPEED = 0.5;
const ENEMY_PROJECTILE_SPEED = 2.5; // Double effective enemy speed
const PROJECTILE_LIFETIME = 2.0;
const ENEMY_SHOOT_INTERVAL = 1.0;
const PLAYER_MAX_HEALTH = 100.0;
const ENEMY_MAX_HEALTH = 3.0;
const ENEMY_MOVE_SPEED = 0.02;
const PROJECTILE_RADIUS = 0.05;
const PLAYER_RADIUS = 0.5;

// Scene setup
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 200);
const renderer = new THREE.WebGLRenderer();
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);

// Camera/player variables
let player = {
    x: 0, y: 1, z: 5,
    yaw: -90, pitch: 0,
    front: new THREE.Vector3(),
    health: PLAYER_MAX_HEALTH
};
let enemies = [];
let projectiles = [];
let enemyProjectiles = [];
let lastTime = performance.now() / 1000;

// Mouse control
let mouseLocked = false;
let mouseLastY = 0;

// Initialize scene
function init() {
    camera.position.set(player.x, player.y, player.z);
    updateCameraDirection();

    // Ground plane
    const groundGeometry = new THREE.PlaneGeometry(200, 200);
    const groundMaterial = new THREE.MeshBasicMaterial({ color: 0xE6E6E6 });
    const ground = new THREE.Mesh(groundGeometry, groundMaterial);
    ground.rotation.x = -M_PI / 2;
    scene.add(ground);

    // Enemies (red cubes)
    enemies = [
        { x: 0, y: 0.5, z: -10, radius: 0.7, alive: true, health: ENEMY_MAX_HEALTH, lastShotTime: 0, mesh: null },
        { x: 5, y: 0.5, z: -15, radius: 0.7, alive: true, health: ENEMY_MAX_HEALTH, lastShotTime: 0, mesh: null },
        { x: -5, y: 0.5, z: -20, radius: 0.7, alive: true, health: ENEMY_MAX_HEALTH, lastShotTime: 0, mesh: null }
    ];
    enemies.forEach(enemy => {
        const geometry = new THREE.BoxGeometry(1, 1, 1);
        const material = new THREE.MeshBasicMaterial({ color: 0xFF0000 });
        enemy.mesh = new THREE.Mesh(geometry, material);
        enemy.mesh.position.set(enemy.x, enemy.y, enemy.z);
        scene.add(enemy.mesh);
    });

    // Projectiles
    for (let i = 0; i < MAX_PROJECTILES; i++) {
        projectiles.push({ x: 0, y: 0, z: 0, dx: 0, dy: 0, dz: 0, timeAlive: 0, active: false, mesh: null });
    }
    for (let i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        enemyProjectiles.push({ x: 0, y: 0, z: 0, dx: 0, dy: 0, dz: 0, timeAlive: 0, active: false, mesh: null });
    }

    // Gun (blue cube)
    const gunGeometry = new THREE.BoxGeometry(0.1, 0.1, 0.6);
    const gunMaterial = new THREE.MeshBasicMaterial({ color: 0x0080FF });
    const gun = new THREE.Mesh(gunGeometry, gunMaterial);
    gun.position.set(0.3, -0.3, -0.8);
    camera.add(gun);

    // Lock mouse on click
    document.addEventListener('click', () => {
        renderer.domElement.requestPointerLock();
    });
    document.addEventListener('pointerlockchange', () => {
        mouseLocked = document.pointerLockElement === renderer.domElement;
    });
}

// Update camera direction
function updateCameraDirection() {
    const yawRad = player.yaw * M_PI / 180;
    const pitchRad = player.pitch * M_PI / 180;
    player.front.set(
        Math.cos(yawRad) * Math.cos(pitchRad),
        Math.sin(pitchRad),
        Math.sin(yawRad) * Math.cos(pitchRad)
    );
    camera.lookAt(
        player.x + player.front.x,
        player.y + player.front.y,
        player.z + player.front.z
    );
}

// Input handling
document.addEventListener('keydown', (event) => {
    const right = new THREE.Vector3().crossVectors(player.front, new THREE.Vector3(0, 1, 0)).normalize();
    switch (event.key.toLowerCase()) {
        case 'w':
            player.x += player.front.x * MOVE_SPEED;
            player.z += player.front.z * MOVE_SPEED;
            break;
        case 's':
            player.x -= player.front.x * MOVE_SPEED;
            player.z -= player.front.z * MOVE_SPEED;
            break;
        case 'a':
            player.x -= right.x * MOVE_SPEED;
            player.z -= right.z * MOVE_SPEED;
            break;
        case 'd':
            player.x += right.x * MOVE_SPEED;
            player.z += right.z * MOVE_SPEED;
            break;
        case 'f':
            shoot();
            break;
        case 'escape':
            document.exitPointerLock();
            break;
    }
    camera.position.set(player.x, player.y, player.z);
});

document.addEventListener('mousemove', (event) => {
    if (!mouseLocked) return;
    player.pitch -= event.movementY * SENSITIVITY;
    player.yaw += event.movementX * SENSITIVITY;
    player.pitch = Math.max(-89, Math.min(89, player.pitch));
    updateCameraDirection();
});

// Shoot player projectile
function shoot() {
    const right = new THREE.Vector3().crossVectors(player.front, new THREE.Vector3(0, 1, 0)).normalize();
    const spawnForward = 1.1;
    const spawnRight = 0.3;
    const spawnDown = -0.3;
    for (let i = 0; i < MAX_PROJECTILES; i++) {
        if (!projectiles[i].active) {
            projectiles[i].x = player.x + player.front.x * spawnForward + right.x * spawnRight;
            projectiles[i].y = player.y + player.front.y * spawnForward + spawnDown;
            projectiles[i].z = player.z + player.front.z * spawnForward + right.z * spawnRight;
            projectiles[i].dx = player.front.x * PROJECTILE_SPEED;
            projectiles[i].dy = player.front.y * PROJECTILE_SPEED;
            projectiles[i].dz = player.front.z * PROJECTILE_SPEED;
            projectiles[i].timeAlive = 0;
            projectiles[i].active = true;
            const geometry = new THREE.SphereGeometry(PROJECTILE_RADIUS, 16, 16);
            const material = new THREE.MeshBasicMaterial({ color: 0x0000FF });
            projectiles[i].mesh = new THREE.Mesh(geometry, material);
            scene.add(projectiles[i].mesh);
            break;
        }
    }
}

// Enemy shoot
function enemyShoot(enemyIdx) {
    if (!enemies[enemyIdx].alive) return;
    const dx = player.x - enemies[enemyIdx].x;
    const dy = player.y - enemies[enemyIdx].y;
    const dz = player.z - enemies[enemyIdx].z;
    const dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
    if (dist === 0) return;
    const spawnForward = 0.8;
    for (let i = 0; i < MAX_ENEMY_PROJECTILES; i++) {
        if (!enemyProjectiles[i].active) {
            enemyProjectiles[i].x = enemies[enemyIdx].x + (dx / dist) * spawnForward;
            enemyProjectiles[i].y = enemies[enemyIdx].y + (dy / dist) * spawnForward;
            enemyProjectiles[i].z = enemies[enemyIdx].z + (dz / dist) * spawnForward;
            enemyProjectiles[i].dx = (dx / dist) * ENEMY_PROJECTILE_SPEED;
            enemyProjectiles[i].dy = (dy / dist) * ENEMY_PROJECTILE_SPEED;
            enemyProjectiles[i].dz = (dz / dist) * ENEMY_PROJECTILE_SPEED;
            enemyProjectiles[i].timeAlive = 0;
            enemyProjectiles[i].active = true;
            const geometry = new THREE.SphereGeometry(PROJECTILE_RADIUS, 16, 16);
            const material = new THREE.MeshBasicMaterial({ color: 0xFFFF00 });
            enemyProjectiles[i].mesh = new THREE.Mesh(geometry, material);
            scene.add(enemyProjectiles[i].mesh);
            enemies[enemyIdx].lastShotTime = performance.now() / 1000;
            break;
        }
    }
}

// Update projectiles
function updateProjectiles(deltaTime) {
    projectiles.forEach(p => {
        if (p.active) {
            p.x += p.dx * deltaTime;
            p.y += p.dy * deltaTime;
            p.z += p.dz * deltaTime;
            p.timeAlive += deltaTime;
            if (p.timeAlive > PROJECTILE_LIFETIME) {
                p.active = false;
                scene.remove(p.mesh);
            } else {
                p.mesh.position.set(p.x, p.y, p.z);
            }
        }
    });
    enemyProjectiles.forEach(p => {
        if (p.active) {
            p.x += p.dx * deltaTime;
            p.y += p.dy * deltaTime;
            p.z += p.dz * deltaTime;
            p.timeAlive += deltaTime;
            if (p.timeAlive > PROJECTILE_LIFETIME) {
                p.active = false;
                scene.remove(p.mesh);
            } else {
                p.mesh.position.set(p.x, p.y, p.z);
            }
        }
    });
}

// Update enemies
function updateEnemies(deltaTime) {
    const currentTime = performance.now() / 1000;
    enemies.forEach((enemy, i) => {
        if (!enemy.alive) return;
        const dx = player.x - enemy.x;
        const dz = player.z - enemy.z;
        const dist = Math.sqrt(dx * dx + dz * dz);
        if (dist > 1) {
            enemy.x += (dx / dist) * ENEMY_MOVE_SPEED;
            enemy.z += (dz / dist) * ENEMY_MOVE_SPEED;
            enemy.mesh.position.set(enemy.x, enemy.y, enemy.z);
        }
        if (currentTime - enemy.lastShotTime >= ENEMY_SHOOT_INTERVAL) {
            enemyShoot(i);
        }
    });
}

// Check collisions
function checkCollisions() {
    projectiles.forEach(p => {
        if (!p.active) return;
        enemies.forEach(enemy => {
            if (!enemy.alive) return;
            const dx = p.x - enemy.x;
            const dy = p.y - enemy.y;
            const dz = p.z - enemy.z;
            const distance = Math.sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < PROJECTILE_RADIUS + enemy.radius) {
                enemy.health -= 1;
                p.active = false;
                scene.remove(p.mesh);
                if (enemy.health <= 0) {
                    enemy.alive = false;
                    scene.remove(enemy.mesh);
                }
            }
        });
    });
    enemyProjectiles.forEach(p => {
        if (!p.active) return;
        const dx = p.x - player.x;
        const dy = p.y - player.y;
        const dz = p.z - player.z;
        const distance = Math.sqrt(dx * dx + dy * dy + dz * dz);
        if (distance < PROJECTILE_RADIUS + PLAYER_RADIUS) {
            player.health -= 10;
            p.active = false;
            scene.remove(p.mesh);
            if (player.health <= 0) {
                alert("Game Over!");
                document.exitPointerLock();
            }
        }
    });
}

// Draw HUD
function drawHud() {
    const hud = document.getElementById('hud');
    hud.innerHTML = `Pos: (${player.x.toFixed(1)}, ${player.y.toFixed(1)}, ${player.z.toFixed(1)}) Health: ${player.health.toFixed(0)}`;
    enemies.forEach(enemy => {
        if (!enemy.alive) return;
        const vector = new THREE.Vector3(enemy.x, enemy.y + 1, enemy.z);
        vector.project(camera);
        const x = (vector.x * 0.5 + 0.5) * window.innerWidth;
        const y = (1 - (vector.y * 0.5 + 0.5)) * window.innerHeight;
        if (vector.z >= -1 && vector.z <= 1) {
            const healthRatio = enemy

.health / ENEMY_MAX_HEALTH;
            const bar = document.createElement('div');
            bar.style.position = 'absolute';
            bar.style.left = `${x - 20}px`;
            bar.style.top = `${y - 5}px`;
            bar.style.width = `${40 * healthRatio}px`;
            bar.style.height = '5px';
            bar.style.background = `linear-gradient(to right, red, green ${healthRatio * 100}%)`;
            bar.style.border = '1px solid black';
            document.body.appendChild(bar);
            setTimeout(() => bar.remove(), 16); // Remove after one frame
        }
    });
}

// Game loop
function animate() {
    requestAnimationFrame(animate);
    const currentTime = performance.now() / 1000;
    const deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    updateProjectiles(deltaTime);
    updateEnemies(deltaTime);
    checkCollisions();
    drawHud();
    renderer.render(scene, camera);
}

// Handle window resize
window.addEventListener('resize', () => {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
});

// Future features roadmap
// 1. Save/Load: Use localStorage or download JSON files
//    - Save: JSON.stringify({ player, enemies, projectiles, inventory })
//    - Load: JSON.parse(localStorage.getItem('gameState'))
// 2. Level Design: Load JSON levels defining walls, spawns
//    - Example: { walls: [{ x, y, z, w, h, d }], spawns: [{ x, y, z, type }] }
//    - Parse and render with Three.js meshes
// 3. Story/Dialogue: Add DOM-based dialogue UI
//    - Example: <div id="dialogue">NPC: Welcome! <button>Option 1</button></div>
// 4. Crafting/Farming/Mining: Inventory system with array of items
//    - Crafting: Check recipes ({ input: { wood: 2 }, output: 'sword' })
//    - Farming: Update crop objects over time
//    - Mining: Raycast mouse click to remove blocks
// 5. Multiplayer: Use WebSocket for real-time sync
//    - Server: Node.js with ws, broadcast { playerId, x, y, z }
//    - Client: Send inputs, receive state
// 6. Pixel Graphics: Voxel grid for Minecraft-like building
//    - Store as 3D array, save to JSON
// 7. Shops/Quests: Data-driven with JSON
//    - Shop: { items: [{ id: 'sword', price: 10 }] }
//    - Quest: { id: 'quest1', target: 'kill', count: 5 }

// Start game
init();
animate();