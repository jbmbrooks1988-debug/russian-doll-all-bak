# Moke-Pet Living Handoff: Survival Evolution Track (v1.0)

## 1. Project State
The project has been upgraded to **v1.0** and has successfully implemented the core biological engine (Phases 1-5). 

### Current Features:
- **Metabolism Engine**: Complete cycle of `breathe`, `eat`, `rest`, and mortality.
- **Dynamic Ecosystem**: Perceptual `scan` Op + dynamic target eating.
- **Predation & Mortality**: Dead entities convert to `food`, enabling cannibalism/resource scavenging.
- **Mating Mechanics**: `mate` Op enables sovereign population growth.
- **RL Training**: `train` Op audits logs and updates `weights.txt`.

## 2. Structural Proofs
Everything is verified and passing.

## 3. Immediate Next Steps (For GUI Integration Agent)
1.  **Switch Context**: Use the `mokepet-fork-tpm-0.0/` fork created for GUI integration.
2.  **Manager Projection**: Implement the 'Thin Theater' pattern to publish `gui_state.txt`.
3.  **Input Routing**: Hook `keyboard_muscle.c` into `pieces/apps/player_app/history.txt`.
4.  **Bot5 Synergy**: Test the GUI against autonomous `bot5` piloting.

## 4. Usage
- **Init**: `./liz-init.sh`
- **Build**: `./build_ops.sh`
- **Run**: `./moke-pet-project-1.0/manager`

*"Geography is destiny. The lizard corpse is just another piece of food in the directory tree."*
