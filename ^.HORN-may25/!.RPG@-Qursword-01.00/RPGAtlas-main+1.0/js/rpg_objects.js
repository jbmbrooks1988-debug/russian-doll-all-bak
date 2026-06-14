//=============================================================================
// rpg_objects.js
//=============================================================================

//-----------------------------------------------------------------------------
// Game_Actor
//
// The game object class for an actor.

function Game_Actor() {
    this.initialize.apply(this, arguments);
}

Game_Actor.prototype.initialize = function(actorId) {
    this._actorId = actorId;
    this.setup(actorId);
};

Game_Actor.prototype.setup = function(actorId) {
    // Note: 'window.DATABASE' must be accessible
    const d = RA.byId(window.DATABASE.actors, actorId);
    if (!d) return;
    this._name = d.name;
    this._classId = d.classId;
    this._charset = d.charset;
    this._level = d.level || 1;
    this._exp = this.expForLevel(this._level);
    this._weaponId = d.weaponId || 0;
    this._armorId = d.armorId || 0;
    this.sanitizeEquipment();
    this._hp = this.mhp;
    this._mp = this.mmp;
    this._states = [];
};

Object.defineProperties(Game_Actor.prototype, {
    level: { get: function() { return this._level; }, configurable: true },
    hp: { get: function() { return this._hp; }, set: function(value) { this._hp = value.clamp(0, this.mhp); }, configurable: true },
    mp: { get: function() { return this._mp; }, set: function(value) { this._mp = value.clamp(0, this.mmp); }, configurable: true },
    mhp: { get: function() { return this.param('mhp'); }, configurable: true },
    mmp: { get: function() { return this.param('mmp'); }, configurable: true },
    atk: { get: function() { return this.param('atk'); }, configurable: true },
    def: { get: function() { return this.param('def'); }, configurable: true },
    mat: { get: function() { return this.param('mat'); }, configurable: true },
    mdf: { get: function() { return this.param('mdf'); }, configurable: true },
    agi: { get: function() { return this.param('agi'); }, configurable: true },
});

Game_Actor.prototype.expForLevel = function(lv) {
    let t = 0;
    for (let l = 2; l <= lv; l++) t += Math.floor(20 * Math.pow(l - 1, 1.75) + 30);
    return t;
};

Game_Actor.prototype.actorClass = function() {
    return RA.byId(window.DATABASE.classes, this._classId) || window.DATABASE.classes[0];
};

Game_Actor.prototype.param = function(stat) {
    const c = this.actorClass();
    let v = Math.floor((c.base[stat] || 0) + (c.growth[stat] || 0) * (this._level - 1));
    const w = RA.byId(window.DATABASE.weapons, this._weaponId);
    const ar = RA.byId(window.DATABASE.armors, this._armorId);
    if (w && w.params) v += w.params[stat] || 0;
    if (ar && ar.params) v += ar.params[stat] || 0;
    v = Math.floor(v * RA.traitRate(c, "param", stat, 1));
    return Math.max(1, v);
};

Game_Actor.prototype.sanitizeEquipment = function() {
    if (!RA.canEquip(this.actorClass(), "weapon", this._weaponId)) this._weaponId = 0;
    if (!RA.canEquip(this.actorClass(), "armor", this._armorId)) this._armorId = 0;
};

Game_Actor.prototype.learnedSkills = function() {
    const c = this.actorClass();
    return (c.learnings || [])
      .filter((l) => l.level <= this._level)
      .map((l) => RA.byId(window.DATABASE.skills, l.skillId))
      .filter(Boolean);
};

Game_Actor.prototype.gainExp = function(amount, log) {
    this._exp += amount;
    while (this._exp >= this.expForLevel(this._level + 1)) {
      const before = this.learnedSkills().map((s) => s.id);
      this._level++;
      this._hp = Math.min(this._hp + 10, this.mhp);
      if (log) log(this._name + " reached level " + this._level + "!");
      // Sound effect would be played by manager
      for (const s of this.learnedSkills()) {
        if (!before.includes(s.id) && log) log(this._name + " learned " + s.name + "!");
      }
    }
};

//-----------------------------------------------------------------------------
// Game_Enemy
//
// The game object class for an enemy.

function Game_Enemy() {
    this.initialize.apply(this, arguments);
}

Game_Enemy.prototype.initialize = function(enemyId, index) {
    this._enemyId = enemyId;
    this._index = index;
    this.setup(enemyId);
};

Game_Enemy.prototype.setup = function(enemyId) {
    const d = RA.byId(window.DATABASE.enemies, enemyId);
    this._data = d;
    this._hp = d ? d.stats.mhp : 1;
    this._alive = true;
    this._states = [];
};

Object.defineProperties(Game_Enemy.prototype, {
    hp: { get: function() { return this._hp; }, set: function(value) { this._hp = value.clamp(0, this.mhp); if (this._hp === 0) this._alive = false; }, configurable: true },
    mhp: { get: function() { return this._data.stats.mhp; }, configurable: true },
    atk: { get: function() { return this._data.stats.atk; }, configurable: true },
    def: { get: function() { return this._data.stats.def; }, configurable: true },
    mat: { get: function() { return this._data.stats.mat; }, configurable: true },
    mdf: { get: function() { return this._data.stats.mdf; }, configurable: true },
    agi: { get: function() { return this._data.stats.agi; }, configurable: true },
    isAlive: { get: function() { return this._alive; }, configurable: true },
});

//-----------------------------------------------------------------------------
// Game_Map
//
// The game object class for a map.

function Game_Map() {
    this.initialize.apply(this, arguments);
}

Game_Map.prototype.initialize = function() {
    this._mapId = 0;
    this._data = null;
    this._events = [];
};

Game_Map.prototype.setup = function(mapId) {
    this._mapId = mapId;
    this._data = RA.byId(window.DATABASE.maps, mapId);
    this._events = (this._data.events || []).map(ev => new Game_Event(ev));
};

Game_Map.prototype.tileAt = function(layer, x, y) {
    return this._data.layers[layer][y * this._data.width + x];
};

Game_Map.prototype.isPassable = function(x, y) {
    if (x < 0 || y < 0 || x >= this._data.width || y >= this._data.height) return false;
    const ov = this._data.passOv ? this._data.passOv[y * this._data.width + x] : 0;
    if (ov === 1) return true;
    if (ov === 2) return false;
    const d2 = this.tileAt("decor2", x, y);
    if (d2 !== 0) return Assets.tiles[d2] ? Assets.tiles[d2].pass : false;
    const d = this.tileAt("decor", x, y);
    if (d !== 0) return Assets.tiles[d] ? Assets.tiles[d].pass : false;
    const g = this.tileAt("ground", x, y);
    if (g === 0) return false;
    return Assets.tiles[g] ? Assets.tiles[g].pass : false;
};

//-----------------------------------------------------------------------------
// Game_Character
//
// The superclass of Game_Event and Game_Player.

function Game_Character() {
    this.initialize.apply(this, arguments);
}

Game_Character.prototype.initialize = function() {
    this.initMembers();
};

Game_Character.prototype.initMembers = function() {
    this._x = 0;
    this._y = 0;
    this._realX = 0;
    this._realY = 0;
    this._targetX = 0;
    this._targetY = 0;
    this._direction = 0;
    this._isMoving = false;
    this._animationCount = 0;
    this._z = 0; // Future Z-level support
};

//-----------------------------------------------------------------------------
// Game_Event
//
// The game object class for an event.

function Game_Event() {
    this.initialize.apply(this, arguments);
}

Game_Event.prototype = Object.create(Game_Character.prototype);
Game_Event.prototype.constructor = Game_Event;

Game_Event.prototype.initialize = function(eventData) {
    Game_Character.prototype.initialize.call(this);
    this._data = eventData;
    this._x = eventData.x;
    this._y = eventData.y;
    this._realX = eventData.x;
    this._realY = eventData.y;
};

//-----------------------------------------------------------------------------
// Game_Interpreter
//
// The interpreter for running event commands.

function Game_Interpreter() {
    this.initialize.apply(this, arguments);
}

Game_Interpreter.prototype.initialize = function(depth) {
    this._depth = depth || 0;
    this._list = null;
    this._index = 0;
    this._eventRT = null;
};
