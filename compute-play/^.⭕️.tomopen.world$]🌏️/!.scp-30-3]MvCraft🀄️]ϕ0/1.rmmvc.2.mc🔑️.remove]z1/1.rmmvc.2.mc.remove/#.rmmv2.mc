

currently this code shows a game editor for 2d and 3d games. 
no .h header files. we edit in vim so that makes it hard    

i want to refactor it to be a 2d/3d minecraft clone game. we may remove a few minor features (like event editor etc)
but honestly most things will stay the same, since minecraft has survival and creative mode. get it? 
i like the controls but i have some modifications:

move selector with arrow keys + space = jump 
move picker with arrow keeys + c = down v = up 
move camera with wasd + q & e for rotate head left and right + g reset

alt = change pov of camera following player
enter = use item in hand  ; enter in selector mode = control selected
(when controlling selected we see 'object inventory / stats & control it)
m = open/close menu  ( arrow or mouse for menu controls)
esc = use selector instead of player controls 
2 = show game from 2d top down view (like a rougelike)
3 = show game in 3d mode + mini map of 2d in upper rightr

we want to use "rougelike style turn based gameplay loop 4 cleanliness"
but in game "clock" will pass normally ; when not in menu screen
(this can trigger calendar based events)


. we want to add survival loop and crafting quick (hunger , eating, crafting , day / night , creepers) as well as level generation
and saving/ loading. 

wanna make a 2do list before we get started? 

i suggest we make fresh code files, but use reference of