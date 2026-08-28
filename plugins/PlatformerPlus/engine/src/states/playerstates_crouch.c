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
void crouch_state(void) BANKED {
        //INITIALIZE VARS
    WORD temp_y = 0;
    UBYTE tile_y = SUBPX_TO_TILE(PLAYER.pos.y + PLAYER.bounds.top + PX_TO_SUBPX(1));
    UBYTE old_x = 0;
    col = 0;

    //A. INPUT CHECK=================================================================================================
    if (que_attacking != stat_attacking){
		stat_attacking = que_attacking;
		load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_CROUCHATTACK: STATE_CROUCH, PLAYER.animations);
	}

    UBYTE drop_press =  FALSE;
    switch(plat_drop_through){
        case 1:
        if(INPUT_DOWN){
            drop_press = TRUE;
        }
        break;
        case 2:
        if (INPUT_PRESSED(INPUT_DOWN)){
            drop_press = TRUE;
        }
        break;
        case 3:
        if (INPUT_DOWN && INPUT_PLATFORM_JUMP){
            drop_press = TRUE;
        }
        break;
        case 4:
        if ((INPUT_PRESSED(INPUT_DOWN) && INPUT_PLATFORM_JUMP) || (INPUT_DOWN && INPUT_PRESSED(INPUT_PLATFORM_JUMP))){
            drop_press = TRUE;
        }
        break;
    }
    //B. STATE PHASE 1
    //Add X & Y motion from moving platforms
    //Transform velocity into positional data, to keep the precision of the platform's movement
    grounded = true;
    if (actor_attached){
        //If the platform has been disabled, detach the player
        if(CHK_FLAG(last_actor->flags, ACTOR_FLAG_DISABLED) == TRUE){
            que_state = FALL_INIT;
            actor_attached = FALSE;
        //If the player is off the platform to the right, detach from the platform
        } else if (PLAYER.pos.x + PLAYER.bounds.left > last_actor->pos.x + EXCLUSIVE_OFFSET(last_actor->bounds.right)) {
            que_state = FALL_INIT;
            actor_attached = FALSE;
        //If the player is off the platform to the left, detach
        } else if (PLAYER.pos.x + EXCLUSIVE_OFFSET(PLAYER.bounds.right) < last_actor->pos.x + last_actor->bounds.left){
            que_state = FALL_INIT;
            actor_attached = FALSE;
        } else{
        //Otherwise, add any change in movement from platform
            deltaX += (last_actor->pos.x - mp_last_x);
            mp_last_x = last_actor->pos.x;
        }

        //If we're on a platform, zero out any other motion from gravity or other sources
        pl_vel_y = 0;

        //Add any change from the platform we're standing on
        deltaY += last_actor->pos.y - mp_last_y;

        //We're setting these to the platform's position, rather than the actor so that if something causes the player to
        //detach (like hitting the roof), they won't automatically get re-attached in the subsequent actor collision step.
        mp_last_y = last_actor->pos.y;
        temp_y = last_actor->pos.y;
    } else if (nocollide != 0){
        //If we're dropping through a platform
        pl_vel_y = 7000; //magic number, rough minimum for actually having the player descend through a platform
        temp_y = PLAYER.pos.y;
    } else {
        //Normal gravity
        pl_vel_y += plat_grav;
        temp_y = PLAYER.pos.y;
        que_state = FALL_INIT; //Use this to test for Falling, avoids an If test in YCollision
    }
    // Add Collision Offset from Moving Platforms
    deltaY += LEGACY_VEL_TO_SUBPX(pl_vel_y);

    //DECELERATION
    if (pl_vel_x < 0) {
        pl_vel_x += (plat_dec >> 1);
        if (pl_vel_x > 0) {
            pl_vel_x = 0;
        }
    } else if (pl_vel_x > 0) {
        pl_vel_x -= (plat_dec >> 1);
        if (pl_vel_x < 0) {
            pl_vel_x = 0;
        }
    }
    run_stage = 0;
    deltaX += LEGACY_VEL_TO_SUBPX(pl_vel_x);

    //FUNCTION X COLLISION
    {
        deltaX = CLAMP(deltaX, -127, 127);
        old_x = PLAYER.pos.x;
        UBYTE tile_start = SUBPX_TO_TILE(PLAYER.pos.y + PLAYER.bounds.top);
        UBYTE tile_end   = SUBPX_TO_TILE(PLAYER.pos.y + PLAYER.bounds.bottom) + 1;
        UWORD new_x = PLAYER.pos.x + deltaX;

        UBYTE tile_x = 0;
        UBYTE col_mid = 0;

        //Edge Locking
        //If the player is past the right edge (camera or screen)
        if (new_x > PX_TO_SUBPX(*edge_right + SCREEN_WIDTH - 16)){
            //If the player is trying to go FURTHER right
            if (new_x > PLAYER.pos.x){
                new_x = PLAYER.pos.x;
                pl_vel_x = 0;
            } else {
            //If the player is already off the screen, push them back
                new_x = PLAYER.pos.x - MIN(PLAYER.pos.x - PX_TO_SUBPX(*edge_right + SCREEN_WIDTH - 16), PX_TO_SUBPX(1));
            }
        //Same but for left side. This side needs a 1 tile (8px) buffer so it doesn't overflow the variable.
        } else if (new_x < PX_TO_SUBPX(*edge_left)){
            if (deltaX < 0){
                new_x = PLAYER.pos.x;
                pl_vel_x = 0;
            } else {
                new_x = PLAYER.pos.x + MIN(PX_TO_SUBPX(*edge_left + 8) - PLAYER.pos.x, PX_TO_SUBPX(1));
            }
        }

        //Step-Check for collisions one tile left or right for each avatar height tile
        if (new_x > PLAYER.pos.x) {
            tile_x = SUBPX_TO_TILE(new_x + PLAYER.bounds.right);
            while (tile_start < tile_end) {
                col = tile_at(tile_x, tile_start);
                if (col & COLLISION_LEFT) {
					new_x = TILE_TO_SUBPX(tile_x) - EXCLUSIVE_OFFSET(PLAYER.bounds.right);
					pl_vel_x = 0;
					col = 1;
					last_wall = 1;
					game_on_player_metatile_collision(tile_x, tile_start, DIR_RIGHT);
					break;
                } else {
					reset_collision_cache(DIR_RIGHT);
				}
                tile_start++;
            }
        } else if (new_x < PLAYER.pos.x) {
            tile_x = SUBPX_TO_TILE(new_x + PLAYER.bounds.left);

            while (tile_start < tile_end) {
                col = tile_at(tile_x, tile_start);

                if (col & COLLISION_RIGHT) {
                    new_x = TILE_TO_SUBPX(tile_x + 1) - PLAYER.bounds.left + 1;
                    pl_vel_x = 0;
                    col = -1;
                    last_wall = -1;
					game_on_player_metatile_collision(tile_x, tile_start, DIR_LEFT);
                    break;
                } else {
					reset_collision_cache(DIR_LEFT);
				}
                tile_start++;
            }
        }
        PLAYER.pos.x = new_x;
    }

    //FUNCTION Y COLLISION
    {
        deltaY = CLAMP(deltaY, -127, 127);

        UBYTE tile_start = SUBPX_TO_TILE(PLAYER.pos.x + PLAYER.bounds.left);
        UBYTE tile_end   = SUBPX_TO_TILE(PLAYER.pos.x + PLAYER.bounds.right) + 1;
		UBYTE is_leftmost = 1;
        if (deltaY > 0) {
            //Moving Downward
            WORD new_y = PLAYER.pos.y + deltaY;
            tile_y = SUBPX_TO_TILE(new_y + PLAYER.bounds.bottom);


            if (nocollide == 0){
                //Check collisions from left to right with the bottom of the player
                while (tile_start < tile_end) {
                    if (tile_at(tile_start, tile_y) & COLLISION_TOP) {
                        //Drop-Through Floor Check
                        if (drop_press){
                            //If it's a regular tile, do not drop through
                            while (tile_start < tile_end) {
                                if (tile_at(tile_start, tile_y) & COLLISION_BOTTOM){
                                    //Escape two levels of looping.
                                    goto land;
                                }
                            tile_start++;
                            }
                            nocollide = 5; //Magic Number, how many frames to steal vertical control
                            pl_vel_y += plat_grav;
                            break;
                        }
                        //Land on Floor
                        land:
                        new_y = TILE_TO_SUBPX(tile_y) - EXCLUSIVE_OFFSET(PLAYER.bounds.bottom);
                        actor_attached = FALSE; //Detach when MP moves through a solid tile.
                        //The distinction here is used so that we can check the velocity when the player hits the ground.
						switch(plat_state){
							case CROUCH_STATE:
								que_state = CROUCH_STATE;
								pl_vel_y = 256;
								break;
							case CROUCH_INIT:
								if (is_leftmost == 1){//check only mario's left leg for left pipe part
									UBYTE tile_id = sram_map_data[VRAM_OFFSET(tile_start, tile_y)];
									if ((tile_id == 57 || tile_id == 233) && specific_events[ENTER_DOWN_PIPE_EVENT].script_addr != 0){
										script_execute(specific_events[ENTER_DOWN_PIPE_EVENT].script_bank, specific_events[ENTER_DOWN_PIPE_EVENT].script_addr, 0, 0);
									}
								}
								que_state = CROUCH_STATE;
								break;
							default:
								que_state = CROUCH_INIT;
								break;
						}
						game_on_player_metatile_collision(tile_start, tile_y, DIR_DOWN);
                        break;
                    } else {
						reset_collision_cache(DIR_DOWN);
					}
                    tile_start++;
					is_leftmost = 0;
                }
            }
            PLAYER.pos.y = new_y;
			reset_collision_cache(DIR_UP);

        } else if (deltaY < 0) {
            //Moving Upward
            WORD new_y = PLAYER.pos.y + deltaY;

            UBYTE tile_y = (SUBPX_TO_TILE(new_y + PLAYER.bounds.top));
            while (tile_start < tile_end) {
                if (tile_at(tile_start, tile_y) & COLLISION_BOTTOM) {
                    new_y = TILE_TO_SUBPX(tile_y + 1) - PLAYER.bounds.top + 1;
                    pl_vel_y = 0;
                    //MP Test: Attempting stuff to stop the player from continuing upward
                    if(actor_attached){
                        temp_y = last_actor->pos.y;
                        if (last_actor->bounds.top > 0){
                            temp_y += last_actor->bounds.top + last_actor->bounds.bottom;
                        }
                        new_y = temp_y;
                    }
                    ct_val = 0;
                    que_state = FALL_INIT;
                    actor_attached = FALSE;
					game_on_player_metatile_collision(tile_start, tile_y, DIR_UP);
                    break;
                } else {
					reset_collision_cache(DIR_UP);
				}
                tile_start++;
            }
            PLAYER.pos.y = new_y;
			reset_collision_cache(DIR_DOWN);
        }
    }

    //Actor Collisions
    gotoActorColCrouch:
    {
        deltaX = 0;
        deltaY = 0;

        actor_t *hit_actor = PLAYER.prev;
		while (hit_actor) {
			if (!CHK_FLAG(hit_actor->flags, ACTOR_FLAG_COLLISION) || (actor_attached && last_actor == hit_actor) || hit_actor->collision_group == 0) {
				hit_actor = hit_actor->prev;
				continue;
			};
			if (bb_intersects(&PLAYER.bounds, &PLAYER.pos, &hit_actor->bounds, &hit_actor->pos)) {
				if (hit_actor->collision_group == plat_mp_group){
					//Platform Actors
					if(!actor_attached || hit_actor != last_actor){
						if (temp_y < hit_actor->pos.y + hit_actor->bounds.top && pl_vel_y >= 0){
							//Attach to MP
							last_actor = hit_actor;
							mp_last_x = hit_actor->pos.x;
							mp_last_y = hit_actor->pos.y;
							PLAYER.pos.y = hit_actor->pos.y + hit_actor->bounds.top - EXCLUSIVE_OFFSET(PLAYER.bounds.bottom);
							//Other cleanup
							pl_vel_y = 0;
							actor_attached = TRUE;
							que_state = CROUCH_INIT;
						}
					}
				}

				//All Other Collisions
				player_register_collision_with(hit_actor);
				break;

			}
			hit_actor = hit_actor->prev;
        }
    }

    //ANIMATION---------------------------------------------------------------------------------------------------
    //Button direction overrides velocity, for slippery run reasons
    if (INPUT_LEFT){
		actor_set_dir(&PLAYER, DIR_LEFT, FALSE);
    } else if (INPUT_RIGHT){
        actor_set_dir(&PLAYER, DIR_RIGHT, FALSE);
    } else {
        actor_set_anim_idle(&PLAYER);
    }


    //STATE CHANGE: Above, basic_y_col can shift to FALL_STATE.--------------------------------------------------

    //CROUCH -> JUMP Check
    if (INPUT_PRESSED(INPUT_PLATFORM_JUMP) || jb_val != 0){
        if (nocollide == 0){
            //Standard Jump
            que_state = JUMP_INIT;

        }
    }
    jb_val = 0;

	//GROUND -> Crouch
	if (!INPUT_DOWN){
		que_state = GROUND_INIT;
	}

    //Check for final frame
    if (que_state != CROUCH_STATE){
		if (script_memory[VAR_MARIOSTATUS_0] > 0){
			PLAYER.bounds.top = PX_TO_SUBPX(-7);
		}
		crouched = 0;
        plat_state = CROUCH_END;
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

    //State-Based Events
    if(state_events[plat_state].script_addr != 0){
        script_execute(state_events[plat_state].script_bank, state_events[plat_state].script_addr, 0, 0);
    }

}
