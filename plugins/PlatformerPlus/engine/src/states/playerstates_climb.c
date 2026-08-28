#pragma bank 255

#include "data/states_defines.h"
#include "states/platform.h"
#include "actor.h"
#include "camera.h"
#include "collision.h"
#include "data_manager.h"
#include "game_time.h"
#include "input.h"
#include "math.h"
#include "macro.h"
#include "scroll.h"
#include "trigger.h"
#include "vm.h"
#include "states/playerstates.h"
#include "meta_tiles.h"
#include "data/game_globals.h"

#ifndef INPUT_PLATFORM_JUMP
#define INPUT_PLATFORM_JUMP        INPUT_A
#endif
#ifndef INPUT_PLATFORM_RUN
#define INPUT_PLATFORM_RUN         INPUT_B
#endif
#ifndef INPUT_PLATFORM_INTERACT
#define INPUT_PLATFORM_INTERACT    INPUT_A
#endif
#ifndef PLATFORM_CAMERA_DEADZONE_Y
#define PLATFORM_CAMERA_DEADZONE_Y 16
#endif
void climb_state(void) BANKED {
	    //INITIALIZE VARS
    WORD temp_y = 0;
    UBYTE tile_y = ((PLAYER.pos.y >> 4) + PLAYER.bounds.top + 1) >> 3;
    UBYTE old_x = 0;
    col = 0;

    //A. INPUT CHECK=================================================================================================

    if (INPUT_DOWN){
		pl_vel_y = plat_climb_vel;
	} else if (INPUT_UP){
		pl_vel_y = -plat_climb_vel;
	} else {
		pl_vel_y = 0;
	}

    deltaY += pl_vel_y >> 8;

    //FUNCTION Y COLLISION
    {
        deltaY = CLAMP(deltaY, -127, 127);

        UBYTE tile_start = (((PLAYER.pos.x >> 4) + PLAYER.bounds.left)  >> 3);
        UBYTE tile_end   = (((PLAYER.pos.x >> 4) + PLAYER.bounds.right) >> 3) + 1;
        if (deltaY > 0) {
            //Moving Downward
            WORD new_y = PLAYER.pos.y + deltaY;
            tile_y = ((new_y >> 4) + PLAYER.bounds.bottom) >> 3;
            if (nocollide == 0){
                //Check collisions from left to right with the bottom of the player
                while (tile_start < tile_end) {
                    if (sram_map_data[VRAM_OFFSET(current_vine_tile_x, tile_y)] != 151 || tile_at(tile_start, tile_y) & COLLISION_TOP) {
                        new_y = ((((tile_y) << 3) - PLAYER.bounds.bottom) << 4) - 1;
                        actor_attached = FALSE; //Detach when MP moves through a solid tile.
                        //The distinction here is used so that we can check the velocity when the player hits the ground.
                        break;
                    }
                    tile_start++;
                }
            }
            PLAYER.pos.y = new_y;
			reset_collision_cache(DIR_UP);

        } else if (deltaY < 0) {
            //Moving Upward
            WORD new_y = PLAYER.pos.y + deltaY;
            tile_y = (((new_y >> 4) + PLAYER.bounds.top) >> 3);
            while (tile_start < tile_end) {
                if (sram_map_data[VRAM_OFFSET(current_vine_tile_x, tile_y)] != 151 || tile_at(tile_start, tile_y) & COLLISION_BOTTOM) {
                    new_y = ((((UBYTE)(tile_y + 1) << 3) - PLAYER.bounds.top) << 4) + 1;
                    actor_attached = FALSE;
					game_on_player_metatile_collision(tile_start, tile_y, DIR_UP);
                    break;
                } else {
					reset_collision_cache(DIR_UP);
				}
                tile_start++;
            }
            PLAYER.pos.y = new_y;
        } else if (sram_map_data[VRAM_OFFSET(current_vine_tile_x, ((PLAYER.pos.y >> 4) + PLAYER.bounds.bottom) >> 3)] != 151 && sram_map_data[VRAM_OFFSET(current_vine_tile_x, (((PLAYER.pos.y >> 4) + PLAYER.bounds.top) >> 3))] != 151){
			que_state = JUMP_INIT;
		}
    }

    //Actor Collisions
    gotoActorColClimb:
    {
        deltaX = 0;
        deltaY = 0;
        actor_t *hit_actor;
        hit_actor = actor_overlapping_player();
        if (hit_actor != NULL && hit_actor->collision_group) {
            player_register_collision_with(hit_actor);
        }
    }

    //ANIMATION---------------------------------------------------------------------------------------------------
    //Button direction overrides velocity, for slippery run reasons
    if (INPUT_PRESSED(INPUT_LEFT)){
		actor_set_dir(&PLAYER, DIR_LEFT, (pl_vel_y != 0)?TRUE:FALSE);
    } else if (INPUT_PRESSED(INPUT_RIGHT)){
		actor_set_dir(&PLAYER, DIR_RIGHT, (pl_vel_y != 0)?TRUE:FALSE);
    } else if (pl_vel_y != 0) {
        actor_set_anim_moving(&PLAYER);
    } else {
		actor_set_anim_idle(&PLAYER);
	}
	PLAYER.pos.x = (current_vine_tile_x << 7) + ((PLAYER.dir == DIR_LEFT)? -64: 64);

    //STATE CHANGE: Above, basic_y_col can shift to FALL_STATE.--------------------------------------------------

    //GROUND -> JUMP Check
    if (INPUT_PRESSED(INPUT_PLATFORM_JUMP) || jb_val != 0){
        if (nocollide == 0){
            //Standard Jump
            que_state = JUMP_INIT;
			pl_vel_x = ((INPUT_LEFT)? -plat_run_vel: (INPUT_RIGHT)? plat_run_vel: 0);
        }
    }
    jb_val = 0;

    //Check for final frame
    if (que_state != CLIMB_STATE){
        plat_state = CLIMB_END;
    }

    //COUNTERS
    // Counting down the drop-through floor frames
    // XX Checked in Fall, Wall, Ground, and basic_y_col, set in basic_y_col
    if (nocollide != 0){
        nocollide -= 1;
    }

    gotoTriggerCol:
    //FUNCTION TRIGGERS
    trigger_activate_at_intersection(&PLAYER.bounds, &PLAYER.pos, 0);

    gotoCounters:
    //COUNTERS===============================================================

    //Hone Camera after the player has dashed
    if (camera_deadzone_x > plat_camera_deadzone_x){
        camera_deadzone_x -= 1;
    }

	if (PLAYER.pos.y < 128 && specific_events[VINE_WARP_EVENT].script_addr != 0){
		script_execute(specific_events[VINE_WARP_EVENT].script_bank, specific_events[VINE_WARP_EVENT].script_addr, 0, 0);
	}

    //State-Based Events
    if(state_events[plat_state].script_addr != 0){
        script_execute(state_events[plat_state].script_bank, state_events[plat_state].script_addr, 0, 0);
    }

}
