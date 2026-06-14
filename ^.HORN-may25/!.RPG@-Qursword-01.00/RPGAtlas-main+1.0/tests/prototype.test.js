"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");

// Mock standard browser/engine objects
const context = vm.createContext({
  console,
  window: {},
  Assets: { tiles: {} },
  RA: {
    byId: (arr, id) => arr.find(e => e.id === id),
    traitRate: () => 1,
    canEquip: () => true
  }
});

// Load the new foundation
vm.runInContext(fs.readFileSync("RPGAtlas-main+0.1/js/rpg_core.js", "utf8"), context);
vm.runInContext(fs.readFileSync("RPGAtlas-main+0.1/js/rpg_objects.js", "utf8"), context);

function evaluate(source) {
  return vm.runInContext(source, context);
}

// Setup dummy database
evaluate(`
  window.DATABASE = {
    actors: [{ id: 1, name: "Test Hero", classId: 1, base: { atk: 10 } }],
    classes: [{ id: 1, name: "Test Class", base: { atk: 10 }, growth: { atk: 2 } }],
    weapons: [], armors: [], skills: [], maps: [], states: []
  };
`);

// Test 1: Method override (Monkey-patching)
evaluate(`
  const _Game_Actor_param = Game_Actor.prototype.param;
  Game_Actor.prototype.param = function(stat) {
    if (stat === 'atk') return 999;
    return _Game_Actor_param.call(this, stat);
  };
`);

const actor = evaluate(`new Game_Actor(1)`);
assert.equal(actor.param('atk'), 999);
console.log("Test 1 Passed: Monkey-patching works!");

// Test 2: Standard utility extensions
assert.equal(evaluate(`(10).clamp(0, 5)`), 5);
assert.equal(evaluate(`"Hero %1".format("Ardan")`), "Hero Ardan");
console.log("Test 2 Passed: Core extensions work!");

console.log("Phase 5: Prototype validation successful.");
