____________________________

using this project as an example of what works on our machine: 
we now wanna make an emoji paint program. SPEX:  shows alot of emojis on the side and tools to paint, fill / shape etc. i also wanna do regular colors as well, and ability to create "tiles/emoijs" and save them in a "tab bank". the trick. i want it to also show up in the cli/terminal . as the file is edited i also want it to change in debug.csv in a way that the debug state could be read and loaded from "file save/load"

example : 
"

🎰️💩️,
"\033[38;2;255;255;255;48;2;0;0;255m\U00002580\033[0m" 🎨️,🗡️,
printf("\033[38;2;255;0;0;48;2;0;255;0m\U00002580\033[0m""\n");,,,
"
its ok if somethings from gl context (like layers) cant render in ascii 
but we will allow use to c thru layers in ascii context. 
______________________🍀️lucky🍀️
