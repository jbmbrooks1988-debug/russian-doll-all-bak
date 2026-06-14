//=============================================================================
// rpg_managers.js
//=============================================================================

//-----------------------------------------------------------------------------
// DataManager
//
// The static class that manages the database and game objects.

function DataManager() {
    throw new Error('This is a static class');
}

window.DATABASE = null;
window.SESSION = null;

DataManager.loadProject = function() {
    if (window.RPGATLAS_PROJECT) {
        window.DATABASE = RA.migrateProject(RA.clone(window.RPGATLAS_PROJECT));
        return;
    }
    try {
        const raw = localStorage.getItem("rpgatlas_project") || localStorage.getItem("driftwood_project");
        if (raw) {
            const p = JSON.parse(raw);
            if (p && p.meta && (p.meta.engine === "rpgatlas" || p.meta.engine === "driftwood")) {
                window.DATABASE = RA.migrateProject(p);
                return;
            }
        }
    } catch (e) {
        console.warn("Stored project unreadable, using sample.", e);
    }
    window.DATABASE = DataDefaults.newProject();
};

DataManager.setupNewGame = function() {
    window.SESSION = {
        switches: {},
        vars: {},
        selfSw: {},
        gold: window.DATABASE.system.startGold || 0,
        inv: { item: {}, weapon: {}, armor: {} },
        party: [],
        steps: 0,
        mapId: window.DATABASE.system.startMapId,
        player: {
            x: window.DATABASE.system.startX,
            y: window.DATABASE.system.startY,
            dir: window.DATABASE.system.startDir,
            transparent: !!window.DATABASE.system.startTransparent
        }
    };
    // Initialize party with Game_Actor objects
    const startParty = (window.DATABASE.system.party || []).slice(0, 4);
    if (startParty.length === 0 && window.DATABASE.actors.length > 0) {
        startParty.push(window.DATABASE.actors[0].id);
    }
    window.SESSION.party = startParty.map(id => new Game_Actor(id));
};

//-----------------------------------------------------------------------------
// SceneManager
//
// The static class that manages scene transitions.

function SceneManager() {
    throw new Error('This is a static class');
}

SceneManager._scene = null;
SceneManager._nextScene = null;

SceneManager.goto = function(scene) {
    this._nextScene = scene;
};

SceneManager.update = function() {
    if (this._nextScene) {
        this._scene = this._nextScene;
        this._nextScene = null;
    }
};

//-----------------------------------------------------------------------------
// BattleManager
//
// The static class that manages battle progress.

function BattleManager() {
    throw new Error('This is a static class');
}

BattleManager.setup = function(troopId, canEscape) {
    this._troopId = troopId;
    this._canEscape = canEscape;
    this._phase = 'start';
};
