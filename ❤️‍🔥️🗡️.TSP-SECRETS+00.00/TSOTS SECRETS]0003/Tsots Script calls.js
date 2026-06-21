

///////////////chimochio fairy initializer test. 

/// this function corresponds as our first function, getting a 

//  x = Rmmv var number 
// y = rmmv var value 



function labia(){
x = 0001
y= 15
newx = $gamePlayer.x;
    newy = $gamePlayer.y;

$gameVariables.setValue(x,y);
    
    console.log(6,$gamePlayer.y);
console.log(7,$gamePlayer.x);
    console.log("GAMEPLAYA");
    
    console.log("newx "+newx);
    console.log("newy"+newy);
    
newSum = newx + newy
console.log("newSum"+newSum );

return y ;

}


////////////////////////





////////////

function carnival(){
  
newx = $gamePlayer.x;
    newy = $gamePlayer.y;


    
    console.log(77,$gamePlayer.y);
console.log(98,$gamePlayer.x);
    console.log("GAMEPLAYA");
    
    console.log("newx "+newx);
    console.log("newy"+newy);
    
newSum = newx + newy
console.log("newSum"+newSum );

return newSum ;
    
}



////



function mainstreet(){

     eventx =  $gameMap.events()[43].x);
       eventy =  $gameMap.events()[43].x);
    
$gameVariables.setVariable(8, $gameMap.events()[43].x)
$gameVariables.setVariable(9, $gameMap.events()[43].y)

console.log($gameVariable(8,$gameMap.events()[43].x);
   
  console.log($gameVariable(8,$gameMap.events()[43].x);    
    
    eventSum = eventx + eventy
    return eventSum;
    
    

};














function penis(){

$gameVariables.setVariable(8, $gameMap.events()[n].x)
$gameVariables.setVariable(9, $gameMap.events()[n].y)



};





















































/// work out some significant peice script call and variable storage here
/// and later we will move them 2 the offical game script



///// prototype peice////

/// we will work out all the functions and variables for an actual living peice
// we will do an animal first, thats less complex but still proves the method


///


























/*

function peiceChoices(){
    

    
    
    choices = ["Move", "Lyfebook", "Cancel"]; 
//can have as many choices as u want up 2 8
params = [0,1,2];
// params === number of choices
    
    var x = choices
    var y = 0
    
$gameMessage.setChoices(x,y); 
    params.push()
console.log(params);
     console.log(x,y);
//written by jbbrooks
    
}


Game_Interpreter.prototype.command102 = function() {
    if (!$gameMessage.isBusy()) {
        this.setupChoices(this._params);
        this._index++;
        this.setWaitMode('message');
    }
    return false;
};

Game_Interpreter.prototype.setupChoices = function(params) {
    var choices = params[0].clone();
    var cancelType = params[1];
    var defaultType = params.length > 2 ? params[2] : 0;
    var positionType = params.length > 3 ? params[3] : 2;
    var background = params.length > 4 ? params[4] : 0;
    if (cancelType >= choices.length) {
        cancelType = -2;
    }
    $gameMessage.setChoices(choices, defaultType, cancelType);
    $gameMessage.setChoiceBackground(background);
    $gameMessage.setChoicePositionType(positionType);
    $gameMessage.setChoiceCallback(function(n) {
        this._branch[this._indent] = n;
        console.log(n);
    }.bind(this));
};

*/


/*

function checkmoveRange (){

while 
peicemoveRange = 4 
moverangeSum =  xpos + ypos

if  ( peicemoveRange = moverangeSum)
    
$gameSelfSwitches.setValue(key, true/false);

}

*/