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
void jump_state(void) BANKED {
    //INITIALIZE VARS
    WORD temp_y = 0;
    UBYTE tile_y = SUBPX_TO_TILE(PLAYER.pos.y + PLAYER.bounds.top + PX_TO_SUBPX(1));
    UBYTE old_x = 0;
    col = 0;

    //A. INPUT CHECK=================================================================================================
	//Crouched
	if (INPUT_DOWN && !crouched){
		load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, STATE_CROUCH, PLAYER.animations);
		PLAYER.bounds.top = PX_TO_SUBPX(1);
		crouched = 1;
	} else if (!INPUT_DOWN && crouched){
		load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, STATE_DEFAULT, PLAYER.animations);
		if (script_memory[VAR_MARIOSTATUS_0] > 0){
			PLAYER.bounds.top = PX_TO_SUBPX(-7);
		}
		crouched = 0;
	} else if (que_attacking != stat_attacking){
		stat_attacking = que_attacking;
		load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_ATTACK: STATE_DEFAULT, PLAYER.animations);
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


    //Vertical Movement-------------------------------------------------------------------------------------------
    //Add jump force during each jump frame
    if (hold_jump_val !=0 && (INPUT_PLATFORM_JUMP || enemy_bounce)){
        //Add the boost per frame amount.
        pl_vel_y -= jump_per_frame;
        //Reduce subsequent jump amounts (for double jumps)
        if (plat_jump_vel >= jump_reduction_val){
            pl_vel_y += jump_reduction_val;
        } else {
            //When reducing that value, zero out if it's negative
            pl_vel_y = 0;
        }
        //Add jump boost from horizontal movement and/or enemy bounce
        WORD tempBoost = (pl_vel_x >> 8) * (enemy_bounce + 1) * boost_val;

        //Take the positive value of x-vel
        tempBoost = MAX(tempBoost, -tempBoost);
        //This is a test to see if the results will overflow pl_vel_y. Note, pl_vel_y is negative here.
        if (tempBoost > 32767 + pl_vel_y){
            pl_vel_y = -32767;
        }
        else{
            pl_vel_y += -tempBoost;
        }
        hold_jump_val -=1;
    } else if (INPUT_PLATFORM_JUMP  && pl_vel_y < 0){
        //After the jump frames end, use the reduced gravity
        pl_vel_y += plat_hold_grav;
    } else if (pl_vel_y >= 0){
        que_state = FALL_INIT;
        actor_attached = FALSE;
        pl_vel_y += plat_grav;
    } else {
        pl_vel_y += plat_grav;
    }

    temp_y = PLAYER.pos.y;
    //Start DeltaX with Actor offsets
    deltaY += LEGACY_VEL_TO_SUBPX(pl_vel_y);

    //Horizontal Movement-----------------------------------------------------------------------------------------

    //FUNCTION ACCELERATION
    if (INPUT_LEFT || INPUT_RIGHT){
        BYTE dir = 1;
        if (INPUT_LEFT){
            dir = -1;
            pl_vel_x = -pl_vel_x;
        }

        if (INPUT_PLATFORM_RUN){
             //Type 1: Smooth Acceleration as the Default in GBStudio
            pl_vel_x = CLAMP(pl_vel_x + plat_run_acc, plat_min_vel, plat_run_vel);
            pl_vel_x *= dir;
            deltaX += LEGACY_VEL_TO_SUBPX(pl_vel_x);
            run_stage = 1;
        } else {
            //Ordinay Walk
            if(pl_vel_x < 0 && plat_turn_acc != 0){
                pl_vel_x += plat_turn_acc;
                run_stage = -1;
            } else {
                run_stage = 0;
                pl_vel_x += plat_walk_acc;
                pl_vel_x = CLAMP(pl_vel_x, plat_min_vel, plat_walk_vel);
            }
            pl_vel_x *= dir;
            deltaX += LEGACY_VEL_TO_SUBPX(pl_vel_x);

        }
    } else{
        //DECELERATION
        if (pl_vel_x < 0) {
            pl_vel_x += plat_air_dec;
            if (pl_vel_x > 0) {
                pl_vel_x = 0;
            }
        } else if (pl_vel_x > 0) {
            pl_vel_x -= plat_air_dec;
            if (pl_vel_x < 0) {
                pl_vel_x = 0;
            }
        }
        run_stage = 0;
        deltaX += LEGACY_VEL_TO_SUBPX(pl_vel_x);
    }

    //FUNCTION X COLLISION

    gotoXCol:
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

        tile_x = SUBPX_TO_TILE(new_x + PLAYER.bounds.right);

		if (pl_vel_x > 0) {
			switch(sram_map_data[VRAM_OFFSET(tile_x, tile_start)]){
				case 151: //beanstalk tile
					que_state = CLIMB_INIT;
					current_vine_tile_x = tile_x;
					actor_set_dir(&PLAYER, DIR_LEFT, FALSE);
					break;
			}
		}

        while (tile_start < tile_end) {

            col = tile_at(tile_x, tile_start);
            if (col & COLLISION_LEFT) {

				new_x = TILE_TO_SUBPX(tile_x) - EXCLUSIVE_OFFSET(PLAYER.bounds.right);
				pl_vel_x = 0;
				col = 1;
				last_wall = 1;
				break;
            }
            tile_start++;
        }

		tile_start = SUBPX_TO_TILE(PLAYER.pos.y + PLAYER.bounds.top);
        tile_x = SUBPX_TO_TILE(new_x + PLAYER.bounds.left);

		if (pl_vel_x < 0) {
			switch(sram_map_data[VRAM_OFFSET(tile_x, tile_start)]){
				case 151: //beanstalk tile
					que_state = CLIMB_INIT;
					current_vine_tile_x = tile_x;
					actor_set_dir(&PLAYER, DIR_RIGHT, FALSE);
					break;
			}
		}

        while (tile_start < tile_end) {
            col = tile_at(tile_x, tile_start);
            if (col & COLLISION_RIGHT) {
                new_x = TILE_TO_SUBPX(tile_x + 1) - PLAYER.bounds.left + 1;
                pl_vel_x = 0;
                col = -1;
                last_wall = -1;
                break;
            }
            tile_start++;
        }

        PLAYER.pos.x = new_x;
    }

    gotoYCol:
    {
        //FUNCTION Y COLLISION
        deltaY = CLAMP(deltaY, -127, 127);

        UBYTE tile_start = SUBPX_TO_TILE(PLAYER.pos.x + PLAYER.bounds.left);
        UBYTE tile_end   = SUBPX_TO_TILE(PLAYER.pos.x + PLAYER.bounds.right) + 1;
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
                        if(plat_state == GROUND_STATE){
                            que_state = GROUND_STATE;
                            pl_vel_y = 256;
                        } else if(plat_state == GROUND_INIT){
                            que_state = GROUND_STATE;
                        } else {que_state = GROUND_INIT;}
						game_on_player_metatile_collision(tile_start, tile_y, DIR_DOWN);
                        break;
                    } else {
						reset_collision_cache(DIR_DOWN);
					}
                    tile_start++;
                }
            }
            PLAYER.pos.y = new_y;
			reset_collision_cache(DIR_UP);

        } else if (deltaY < 0) {
            //Moving Upward
			tile_start = SUBPX_TO_TILE(PLAYER.pos.x + ((PLAYER.bounds.left + PLAYER.bounds.right) >> 1));
            WORD new_y = PLAYER.pos.y + deltaY;

            UBYTE tile_y = (SUBPX_TO_TILE(new_y + PLAYER.bounds.top));
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
            } else {
				reset_collision_cache(DIR_UP);
			}
            PLAYER.pos.y = new_y;
			reset_collision_cache(DIR_DOWN);
        }
    }

    //FUNCTION ACTOR CHECK
    //Actor Collisions
    gotoActorColJump:
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
							que_state = GROUND_INIT;
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
        //This animation is currently shared by jumping, dashing, and falling. Dashing doesn't need this complexity though.
    //Here velocity overrides direction. Whereas on the ground it is the reverse.
    if(plat_turn_control){
        if (INPUT_LEFT){
            PLAYER.dir = DIR_LEFT;
        } else if (INPUT_RIGHT){
            PLAYER.dir = DIR_RIGHT;
        } else if (pl_vel_x < 0) {
            PLAYER.dir = DIR_LEFT;
        } else if (pl_vel_x > 0) {
            PLAYER.dir = DIR_RIGHT;
        }
    }

    if (PLAYER.dir == DIR_LEFT){
        actor_set_anim(&PLAYER, ANIM_JUMP_LEFT);
    } else {
        actor_set_anim(&PLAYER, ANIM_JUMP_RIGHT);
    }

    //STATE CHANGE------------------------------------------------------------------------------------------------
    //Above: JUMP-> NEUTRAL when a) player starts descending, b) player hits roof, c) player stops pressing, d)jump frames run out.


    //Check for final frame
    if (que_state != JUMP_STATE){
        plat_state = JUMP_END;
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
