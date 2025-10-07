#!/bin/bash
gcc -Wall -Wextra -std=c99 -o c_rewrite/gl_renderer c_rewrite/gl_renderer.c -lglut -lGL -lGLU
gcc -Wall -Wextra -std=c99 -o c_rewrite/generate_map c_rewrite/generate_map.c
gcc -Wall -Wextra -std=c99 -o c_rewrite/player_control c_rewrite/player_control.c
gcc -Wall -Wextra -std=c99 -o c_rewrite/spawn_monsters c_rewrite/spawn_monsters.c
gcc -Wall -Wextra -std=c99 -o c_rewrite/monster_ai c_rewrite/monster_ai.c
gcc -Wall -Wextra -std=c99 -o c_rewrite/combat_resolver c_rewrite/combat_resolver.c
gcc -Wall -Wextra -std=c99 -o c_rewrite/spawn_items c_rewrite/spawn_items.c
gcc -Wall -Wextra -std=c99 -o c_rewrite/pickup_item c_rewrite/pickup_item.c
gcc -Wall -Wextra -std=c99 -o c_rewrite/use_item c_rewrite/use_item.c
gcc -Wall -Wextra -std=c99 -o c_rewrite/cli_game]b1 c_rewrite/cli_game]b1.c
