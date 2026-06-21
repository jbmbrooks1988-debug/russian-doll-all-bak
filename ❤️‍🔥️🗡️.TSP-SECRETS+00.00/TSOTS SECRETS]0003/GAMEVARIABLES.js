
Game_Variables.prototype.setValue = function(variableId, value) {
    if (variableId > 0 && variableId < $dataSystem.variables.length) {
        console.log(variableId+ "VARID" );///000 4 
        console.log($dataSystem.variables.length+"LOVE");
        if (typeof value === 'number') {
            console.log('number'+"HELL");
            value = Math.floor(value);
            console.log(value + "value");  /// 13
            console.log("DRAMAMMAMAM");////////  HERE FOR VAR START
        }
        this._data[variableId] = value;
        console.log(value);
       var cuntfuck = value
       console.log ( cuntfuck  + "HIHIHIH");
        this.onChange();
        return cuntfuck;
    }
};

Game_Variables.prototype.onChange = function() {
  
    $gameMap.requestRefresh();
   
    
};


////////////
Game_Interpreter.prototype.operateValue = function(operation, operandType, operand) {
    console.log("NAKEDNIGGER");
    var value = operandType === 0 ? operand : $gameVariables.value(operand);
    return operation === 0 ? value : -value;
};

////////////


// Transfer Player
Game_Interpreter.prototype.command201 = function() {
    if (!$gameParty.inBattle() && !$gameMessage.isBusy()) {
        var mapId, x, y;
        if (this._params[0] === 0) {  // Direct designation
            mapId = this._params[1];
            x = this._params[2];
            y = this._params[3];
        } else {  // Designation with variables
            mapId = $gameVariables.value(this._params[1]);
            x = $gameVariables.value(this._params[2]);
            y = $gameVariables.value(this._params[3]);
        }
        $gamePlayer.reserveTransfer(mapId, x, y, this._params[4], this._params[5]);
        this.setWaitMode('transfer');
        this._index++;
    }
    return false;
};


//////////////

// Get Location Info
Game_Interpreter.prototype.command285 = function() {
    var x, y, value;
    console.log("ERRRRRIIIAIIAIIAIIAIA");
    if (this._params[2] === 0) {  // Direct designation
        x = this._params[3];
        y = this._params[4];
    } else {  // Designation with variables
        console.log("CHESS");
        x = $gameVariables.value(this._params[3]);
        y = $gameVariables.value(this._params[4]);
    }
    switch (this._params[1]) {
    case 0:     // Terrain Tag
        value = $gameMap.terrainTag(x, y);
        break;
    case 1:     // Event ID
        value = $gameMap.eventIdXy(x, y);
        break;
    case 2:     // Tile ID (Layer 1)
    case 3:     // Tile ID (Layer 2)
    case 4:     // Tile ID (Layer 3)
    case 5:     // Tile ID (Layer 4)
        value = $gameMap.tileId(x, y, this._params[1] - 2);
        break;
    default:    // Region ID
        value = $gameMap.regionId(x, y);
            console.log("dfffdg");
        break;
    }
    $gameVariables.setValue(this._params[0], value);
    console.log(value + "HELLLLLI");
    return true;
};













///////





// Control Variables




Game_Interpreter.prototype.command122 = function() {
    var value = 0;
    console.log("variableslogs");
    switch (this._params[3]) {  // Operand
    case 0:  // Constant
        value = this._params[4];
        break;
    case 1:  // Variable
            console.log("pussynigger");
        value = $gameVariables.value(this._params[4]);
        break;
    case 2:  // Random
        value = this._params[4] + Math.randomInt(this._params[5] - this._params[4] + 1);
        break;
    case 3:  // Game Data
            console.log("iminiiitii83838");
        value = this.gameDataOperand(this._params[4], this._params[5], this._params[6]);
        break;
    case 4:  // Script
            console.log("niggaiaiiaiai");
        value = eval(this._params[4]);
        break;
    }
    for (var i = this._params[0]; i <= this._params[1]; i++) {
        this.operateVariable(i, this._params[2], value);
    }
    console.log("fucku");
    console.log(value);
    var minenow = value;
    console.log(minenow +"mineb4A" );
    
    return  true;
};






