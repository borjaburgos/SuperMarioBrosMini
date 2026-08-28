#pragma bank 255

#include "data/states_defines.h"
#include "states/platform.h"
#include "actor.h"
#include "camera.h"
#include "collision.h"
#include "data_manager.h"
#include "music_manager.h"
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
#include "actor_behavior.h"

#ifndef INPUT_PLATFORM_JUMP
#define INPUT_PLATFORM_JUMP        INPUT_A
#endif
#ifndef INPUT_PLATFORM_RUN
#define INPUT_PLATFORM_RUN         INPUT_B
#endif
#ifndef PLATFORM_CAMERA_DEADZONE_Y
#define PLATFORM_CAMERA_DEADZONE_Y 16
#endif

#define MAX_METATILE_CHANGE_CHECKS 4

script_state_t state_events[24];

script_state_t specific_events[9];

UBYTE grounded;

//DEFAULT ENGINE VARIABLES
WORD plat_min_vel;
WORD plat_walk_vel;
WORD plat_run_vel;
WORD plat_climb_vel;
WORD plat_walk_acc;
WORD plat_run_acc;
WORD plat_dec;
WORD plat_jump_vel;
WORD plat_grav;
WORD plat_hold_grav;
WORD plat_max_fall_vel;

// Compatibility storage for GB Studio 4.3's built-in platformer VM API.
// This project uses the custom state machine below, but the current engine
// still exports native helpers that reference these standard symbols.
UBYTE plat_drop_through_active;
WORD plat_jump_hold_vel;
UBYTE plat_jump_hold_frames;
UBYTE plat_extra_jumps;
UBYTE plat_coyote_frames;
UBYTE plat_jump_buffer_frames;
UBYTE plat_wall_jump_max;
UBYTE plat_wall_slide;
WORD plat_wall_slide_vel;
WORD plat_wall_kick;
UBYTE plat_float_active;
WORD plat_float_vel;
UBYTE plat_air_control;
UBYTE plat_dash_active;
UBYTE plat_dash_from;
UBYTE plat_dash_jump_from;
UBYTE plat_dash_mask;
WORD plat_dash_dist;
UBYTE plat_dash_frames;
UBYTE plat_dash_ready_frames;
UBYTE plat_dash_deadzone;
WORD plat_knockback_vel_x;
WORD plat_knockback_vel_y;
UBYTE plat_knockback_frames;
WORD plat_blank_grav;

WORD plat_vel_x;
WORD plat_vel_y;
UBYTE plat_extra_jumps_counter;
UBYTE plat_wall_jump_counter;
UBYTE plat_is_actor_attached;
UBYTE plat_grounded;
BYTE plat_run_stage;
UBYTE plat_jump_type;
BYTE plat_wall_col;
pStates plat_next_state;

//PLATFORMER PLUS ENGINE VARIABLES
//All engine fields are prefixed with plat_
BYTE plat_camera_deadzone_x; // Camera deadzone
UBYTE plat_camera_block;    //Limit the player's movement to the camera's edges
UBYTE plat_drop_through;    //Drop-through control
UBYTE plat_mp_group;        //Collision group for platform actors
UBYTE plat_solid_group;     //Collision group for solid actors
WORD plat_jump_min;         //Jump amount applied on the first frame of jumping
UBYTE plat_hold_jump_max;   //Maximum number for frames for continuous input
WORD plat_jump_reduction;   //Reduce height each double jump
UBYTE plat_coyote_max;      //Coyote Time maximum frames
UBYTE plat_buffer_max;      //Jump Buffer maximum frames
UBYTE plat_float_input;     //Input type for float (hold up or hold jump)
WORD plat_float_grav;       //Speed of fall descent while floating
UBYTE plat_turn_control;    //Controls the amount of slippage when the player turns while running.
WORD plat_air_dec;          // air deceleration rate
WORD plat_turn_acc;         //Speed with which a character turns
UBYTE plat_run_boost;        //Additional jump height based on player horizontal speed
BYTE run_stage;             //Tracks the stage of running based on the run type

UBYTE nocollide;            //Turns off vertical collisions, currently only for dropping through platforms
WORD deltaX;
WORD deltaY;

//COUNTER variables
UBYTE ct_val;               //Coyote Time Variable
UBYTE jb_val;               //Jump Buffer Variable
UBYTE dj_val;               //Current double jump
UBYTE hold_jump_val;        //Jump input hold variable

//WALL variables
BYTE last_wall;             //tracks the last wall the player touched
BYTE col;

//COLLISION VARS
actor_t *last_actor;        //The last actor the player hit, and that they were attached to
UBYTE actor_attached;       //Keeps track of whether the player is currently on an actor and inheriting its movement
WORD mp_last_x;             //Keeps track of the pos.x of the attached actor from the previous frame
WORD mp_last_y;             //Keeps track of the pos.y of the attached actor from the previous frame


//JUMPING VARIABLES
WORD jump_reduction_val;    //Holds a temporary jump velocity reduction
WORD jump_per_frame;        //Holds a jump amount that has been normalized over the number of jump frames
WORD jump_reduction;        //Holds the reduction amount that has been normalized over the number of jump frames
WORD boost_val;
UBYTE enemy_bounce;

//WALKING AND RUNNING VARIABLES
WORD pl_vel_x;              //Tracks the player's x-velocity between frames
WORD pl_vel_y;              //Tracks the player's y-velocity between frames

//VARIABLES FOR CAMERAS
WORD *edge_left;
WORD *edge_right;
WORD mod_image_right;
WORD mod_image_left;

//TEMP VARIABLES
WORD temp_counter;


UBYTE game_previous_top_tile;
UBYTE game_previous_left_tile;
UBYTE game_previous_bottom_tile;
UBYTE game_previous_right_tile;

UBYTE game_current_left_tile;
UBYTE game_current_top_tile;
UBYTE game_current_right_tile;
UBYTE game_current_bottom_tile;

UBYTE actors_collisionx_cache[4];
UBYTE actors_collisiony_cache[4];

UBYTE current_vine_tile_x;

UBYTE underwater_pit;
UBYTE crouched;
UBYTE que_attacking;
UBYTE stat_attacking;

void platform_init(void) BANKED {
    //Initialize Camera
    camera_offset_x = 0;
    camera_offset_y = 0;
    camera_deadzone_x = plat_camera_deadzone_x;
    camera_deadzone_y = PLATFORM_CAMERA_DEADZONE_Y;
    if ((camera_settings & CAMERA_LOCK_X_FLAG)){
        camera_x = PLAYER.pos.x + PX_TO_SUBPX(8);
    } else{
        camera_x = 0;
    }
    if ((camera_settings & CAMERA_LOCK_Y_FLAG)){
        camera_y = PLAYER.pos.y + PX_TO_SUBPX(8);
    } else{
        camera_y = 0;
    }

    //Initialize Camera Bounds
    mod_image_right = image_width - SCREEN_WIDTH;
    mod_image_left = 0;
    if (plat_camera_block & 1){
        edge_left = &scroll_x;
    }
    else{
        edge_left = &mod_image_left;
    }

    if (plat_camera_block & 2){
        edge_right = &scroll_x;
    }
    else{
        edge_right = &image_width;
    }

    //Make sure jumping doesn't overflow variables
    //First, check for jumping based on Frames and Initial Jump Min
    while (32000 - (plat_jump_vel/MIN(15,plat_hold_jump_max)) - plat_jump_min < 0){
        plat_hold_jump_max += 1;
    }

    //This ensures that, by itself, the plat run boost active on any single frame cannot overflow a WORD.
    //It is complemented by another check in the jump itself that works with the actual velocity.
    if (plat_run_boost != 0){
        while((32000/plat_run_boost) < ((plat_run_vel>>8)/plat_hold_jump_max)){
            plat_run_boost--;
        }
    }

    //Normalize variables by number of frames
    jump_per_frame = plat_jump_vel / MIN(15, plat_hold_jump_max);   //jump force applied per frame in the JUMP_STATE
    jump_reduction = plat_jump_reduction / plat_hold_jump_max;      //Amount to reduce subequent jumps per frame in JUMP_STATE
    boost_val = plat_run_boost / plat_hold_jump_max;                  //Vertical boost from horizontal speed per frame in JUMP STATE

    //Initialize State
	plat_state = FALL_INIT;
	que_state = FALL_INIT;

    actor_attached = FALSE;
    nocollide = 0;
    if (PLAYER.dir == DIR_UP || PLAYER.dir == DIR_DOWN || PLAYER.dir == DIR_NONE) {
        PLAYER.dir = DIR_RIGHT;
    }

    //Initialize other vars
    game_time = 0;
    pl_vel_x = 0;
    pl_vel_y = 4000;                //Magic number for preventing a small glitch when loading into a scene
    hold_jump_val = 0;//plat_hold_jump_max;
    deltaX = 0;
    deltaY = 0;
	dj_val = 0;
	underwater_pit = 0;
	crouched = 0;
	actor_behavior_init();
}

void platform_update(void) BANKED {
    //The switch here is to disagreggate the state code, because it had collectively grown too large to stay in a single bank.
	actor_behavior_update();
    plat_state = que_state;
	script_memory[VAR_FRAMECOINS] = 0;
	PLAYER.anim_tick = 7;
    switch(plat_state){
        case FALL_INIT:
            que_state = FALL_STATE;
			load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_ATTACK: STATE_DEFAULT, PLAYER.animations);
			crouched = 0;
        case FALL_STATE:
            fall_state();
            break;
        case GROUND_INIT:
            que_state = GROUND_STATE;
			load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_ATTACK: STATE_DEFAULT, PLAYER.animations);
            //pl_vel_y = 256;
            ct_val = plat_coyote_max;
			dj_val = 1;
            jump_reduction_val = 0;
			enemy_bounce = 0;
			crouched = 0;
        case GROUND_STATE:
            ground_state();
            break;
		case CROUCH_INIT:
			que_state = CROUCH_STATE;
			load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_CROUCHATTACK: STATE_CROUCH , PLAYER.animations);
			PLAYER.bounds.top = PX_TO_SUBPX(1);
			crouched = 1;
			//pl_vel_y = 256;
		case CROUCH_STATE:
			crouch_state();
			break;
		case SKID_INIT:
			que_state = SKID_STATE;
			load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, STATE_SKID, PLAYER.animations);
		case SKID_STATE:
			ground_state();
			break;
        case JUMP_INIT:
            que_state = JUMP_STATE;
			load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_ATTACK: STATE_DEFAULT, PLAYER.animations);
            hold_jump_val = plat_hold_jump_max;
            actor_attached = FALSE;
            pl_vel_y = -plat_jump_min;
            jb_val = 0;
            ct_val = 0;
			crouched = 0;
        case JUMP_STATE:
            jump_state();
            break;
        case CLIMB_INIT:
			que_state = CLIMB_STATE;
			load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, STATE_CLIMB, PLAYER.animations);
			pl_vel_y = 0;
			pl_vel_x = 0;
		case CLIMB_STATE:
			climb_state();
			break;
        case BLANK_INIT:
            que_state = BLANK_STATE;
            pl_vel_x = 0;
            pl_vel_y = 0;
        case BLANK_STATE:
            //State-Based Events
			if(state_events[plat_state].script_addr != 0){
				script_execute(state_events[plat_state].script_bank, state_events[plat_state].script_addr, 0, 0);
			}
            break;
		case SWIM_INIT:
			que_state = SWIM_STATE;
			load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_ATTACK: STATE_SWIM, PLAYER.animations);
			actor_attached = FALSE;
			pl_vel_y = -plat_jump_min;
		case SWIM_STATE:
			swim_state();
			break;
    }
	if (script_memory[VAR_PLAYER_IFRAMES] > 0){
		script_memory[VAR_PLAYER_IFRAMES]--;
	}
	if (script_memory[VAR_HASYOSHI] && que_attacking){
		upoint16_t tongue_offset;
		rect16_t yoshi_tongue_boundingbox;
		tongue_offset.y = PLAYER.pos.y + ((crouched) ? PX_TO_SUBPX(2) : PX_TO_SUBPX(-4));
		tongue_offset.x = PLAYER.pos.x + ((PLAYER.dir == DIR_LEFT) ? PX_TO_SUBPX(-20) : PX_TO_SUBPX(4));
		yoshi_tongue_boundingbox.right = PX_TO_SUBPX(24) - 1;
		yoshi_tongue_boundingbox.left = 0;
		yoshi_tongue_boundingbox.top = 0;
		yoshi_tongue_boundingbox.bottom = PX_TO_SUBPX(8) - 1;

		for (UBYTE i = 1; i < MAX_ACTORS; i++){
			actor_t * hit_actor = (actors + i);
			if (!CHK_FLAG(hit_actor->flags, ACTOR_FLAG_ACTIVE) || !CHK_FLAG(hit_actor->flags, ACTOR_FLAG_COLLISION)){
				continue;
			}
			if (bb_intersects(&yoshi_tongue_boundingbox, &tongue_offset, &hit_actor->bounds, &hit_actor->pos)) {
				if (hit_actor->script.bank){
					script_execute(hit_actor->script.bank, hit_actor->script.ptr, 0, 1, 2);
				}
			}
		}
	}
	game_check_player_metatiles_entered();
	if(script_memory[VAR_FRAMECOINS] && specific_events[COIN_COLLECTED_EVENT].script_addr != 0){
        script_execute(specific_events[COIN_COLLECTED_EVENT].script_bank, specific_events[COIN_COLLECTED_EVENT].script_addr, 0, 0);
    }
}


inline void game_on_metatile_enter(UBYTE tile_x, UBYTE tile_y) {
	UBYTE tile_id = sram_map_data[VRAM_OFFSET(tile_x, tile_y)];
	switch(tile_id){
		case 4://coin
			replace_meta_tile(tile_x, tile_y, 0, TRUE);
			script_memory[VAR_FRAMECOINS]++;
			break;
		case 1://underwater coin
			replace_meta_tile(tile_x, tile_y, 2, TRUE);
			script_memory[VAR_FRAMECOINS]++;
			break;
		case 202://big coin (top left)
			replace_meta_tile(tile_x, tile_y, 0, TRUE);
			replace_meta_tile(tile_x + 1, tile_y + 1, 0, TRUE);
			replace_meta_tile(tile_x + 1, tile_y, 0, TRUE);
			replace_meta_tile(tile_x, tile_y + 1, 0, TRUE);
			script_memory[VAR_FRAMECOINS] += 4;
			break;
		case 203://big coin (top right)
			replace_meta_tile(tile_x, tile_y, 0, TRUE);
			replace_meta_tile(tile_x - 1, tile_y + 1, 0, TRUE);
			replace_meta_tile(tile_x - 1, tile_y, 0, TRUE);
			replace_meta_tile(tile_x, tile_y + 1, 0, TRUE);
			script_memory[VAR_FRAMECOINS] += 4;
			break;
		case 204://big coin (bottom left)
			replace_meta_tile(tile_x, tile_y, 0, TRUE);
			replace_meta_tile(tile_x + 1, tile_y - 1, 0, TRUE);
			replace_meta_tile(tile_x + 1, tile_y, 0, TRUE);
			replace_meta_tile(tile_x, tile_y - 1, 0, TRUE);
			script_memory[VAR_FRAMECOINS] += 4;
			break;
		case 205://big coin (bottom right)
			replace_meta_tile(tile_x, tile_y, 0, TRUE);
			replace_meta_tile(tile_x - 1, tile_y - 1, 0, TRUE);
			replace_meta_tile(tile_x - 1, tile_y, 0, TRUE);
			replace_meta_tile(tile_x, tile_y - 1, 0, TRUE);
			script_memory[VAR_FRAMECOINS] += 4;
			break;
	}
}

void game_check_player_metatiles_entered(void) BANKED {

    game_current_left_tile = SUBPX_TO_TILE(PLAYER.pos.x + PLAYER.bounds.left);
    game_current_top_tile = SUBPX_TO_TILE(PLAYER.pos.y + PLAYER.bounds.top);
	game_current_right_tile = SUBPX_TO_TILE(PLAYER.pos.x + PLAYER.bounds.right);
    game_current_bottom_tile = SUBPX_TO_TILE(PLAYER.pos.y + PLAYER.bounds.bottom);

	UBYTE tmp_tile = 0;
	UBYTE left_side_checked = 0;
	UBYTE right_side_checked = 0;
	UBYTE max_counter = 0;

	//check left
	if (game_current_left_tile < game_previous_left_tile){
		tmp_tile = game_current_top_tile;
		while (max_counter < MAX_METATILE_CHANGE_CHECKS && tmp_tile <= game_current_bottom_tile){
			game_on_metatile_enter(game_current_left_tile, tmp_tile);
			tmp_tile++;
			max_counter++;
		}
		max_counter = 0;
		left_side_checked = 1;
	}
	//check right
	if (game_current_left_tile != game_current_right_tile && (game_current_right_tile > game_previous_right_tile)){
		tmp_tile = game_current_top_tile;
		while (max_counter < MAX_METATILE_CHANGE_CHECKS && tmp_tile <= game_current_bottom_tile){
			game_on_metatile_enter(game_current_right_tile, tmp_tile);
			tmp_tile++;
			max_counter++;
		}
		max_counter = 0;
		right_side_checked = 1;
	}

	//Check top
	if (game_current_top_tile < game_previous_top_tile){
		tmp_tile = game_current_left_tile + left_side_checked;
		while (max_counter < MAX_METATILE_CHANGE_CHECKS && tmp_tile <= (game_current_right_tile - right_side_checked)){
			game_on_metatile_enter(tmp_tile, game_current_top_tile);
			tmp_tile++;
			max_counter++;
		}
		max_counter = 0;
	}

	//Check bottom
	if (game_current_top_tile != game_current_bottom_tile && (game_current_bottom_tile > game_previous_bottom_tile)){
		tmp_tile = game_current_left_tile + left_side_checked;
		while (max_counter < MAX_METATILE_CHANGE_CHECKS && tmp_tile <= (game_current_right_tile - right_side_checked)){
			game_on_metatile_enter(tmp_tile, game_current_bottom_tile);
			tmp_tile++;
			max_counter++;
		}
		max_counter = 0;
	}

	game_previous_left_tile = game_current_left_tile;
	game_previous_top_tile = game_current_top_tile;
	game_previous_right_tile = game_current_right_tile;
	game_previous_bottom_tile = game_current_bottom_tile;

}

void game_on_player_metatile_collision(UBYTE tile_x, UBYTE tile_y, UBYTE direction) BANKED{

	if (actors_collisionx_cache[direction] != tile_x || actors_collisiony_cache[direction] != tile_y) {
		actors_collisionx_cache[direction] = tile_x;
		actors_collisiony_cache[direction] = tile_y;
		switch(direction){
			case DIR_UP:
				UBYTE tile_id = sram_map_data[VRAM_OFFSET(tile_x, tile_y)];
				switch(tile_id){
					case 5://coin block
					case 7://brick
					case 152://multi coin brick
					case 153://powerup brick
					case 154://star brick
					case 155://1up brick
					case 156://powerup block
					case 157://beanstalk block
					case 158://star block
					case 159://1up block
					case 160://invis coin block
					case 161://invis powerup block
					case 162://invis star block
					case 163://invis 1up block
					case 169://beanstalk brick
					case 171://egg brick
					case 172://egg block
					case 173://invis egg block
					case 198://Big coin block (top left)
					case 199://Big coin block (top right)
					case 200://Big coin block (bottom left)
					case 201://Big coin block (bottom right)
					case 210://Big brick
						if(specific_events[HIT_BLOCK_EVENT].script_addr != 0){
							script_memory[VAR_HITBLOCKID] = tile_id;
							script_memory[VAR_HITBLOCKX] = tile_x;
							script_memory[VAR_HITBLOCKY] = tile_y;
							script_memory[VAR_HITBLOCKSOURCE] = 0;
							script_execute(specific_events[HIT_BLOCK_EVENT].script_bank, specific_events[HIT_BLOCK_EVENT].script_addr, 0, 0);
						}
						break;
				}
				break;
		}
	}
}

void reset_collision_cache(UBYTE direction) BANKED {
	actors_collisionx_cache[direction] = 0;
	actors_collisiony_cache[direction] = 0;
}


void fall_state(void) BANKED {
    //INITIALIZE VARS
    WORD temp_y = 0;
    UWORD p_half_width = (PLAYER.bounds.right - PLAYER.bounds.left) >> 1;
    UBYTE tile_x_mid = SUBPX_TO_TILE(PLAYER.pos.x + PLAYER.bounds.left + p_half_width);
    UBYTE tile_y = SUBPX_TO_TILE(PLAYER.pos.y + PLAYER.bounds.top + PX_TO_SUBPX(1));
    col = 0;
    //A. INPUT CHECK=================================================================================================
      //Crouched
	if (INPUT_DOWN && !crouched){
		load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_CROUCHATTACK : STATE_CROUCH, PLAYER.animations);
		PLAYER.bounds.top = PX_TO_SUBPX(1);
		crouched = 1;
	} else if (!INPUT_DOWN && crouched){
		load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_ATTACK : STATE_DEFAULT, PLAYER.animations);
		if (script_memory[VAR_MARIOSTATUS_0] > 0){
			PLAYER.bounds.top = PX_TO_SUBPX(-7);
		}
		crouched = 0;
	} else if (que_attacking != stat_attacking){
		stat_attacking = que_attacking;
		load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_ATTACK : STATE_DEFAULT, PLAYER.animations);
	}

    //B. STATE SPECIFIC LOGIC
    //Vertical Movement--------------------------------------------------------------------------------------------
    //FLOAT INPUT
    if (((plat_float_input == 1 && INPUT_PLATFORM_JUMP) || (plat_float_input == 2 && INPUT_UP)) && pl_vel_y >= 0){
        pl_vel_y = plat_float_grav;
    } else if (nocollide != 0){
        //magic number, rough minimum for actually having the player descend through a platform
        pl_vel_y = 7000;
    } else if (INPUT_PLATFORM_JUMP && pl_vel_y < 0) {
        //Gravity while holding jump
        pl_vel_y += plat_hold_grav;
        pl_vel_y = MIN(pl_vel_y,plat_max_fall_vel);
    } else {
        //Normal gravity
        pl_vel_y += plat_grav;
        pl_vel_y = MIN(pl_vel_y,plat_max_fall_vel);
    }

    //Collision ---------------------------------------------------------------------------------------------------
    //Vertical Collision Checks
    deltaY += LEGACY_VEL_TO_SUBPX(pl_vel_y);
    temp_y = PLAYER.pos.y;

    //Horizontal Movement----------------------------------------------------------------------------------------
    //C. ACCELERATION
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

    //D. COLLISIONS
    //X-COLLISION
    gotoXCol:
    {
        deltaX = CLAMP(deltaX, -127, 127);
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
			switch(sram_map_data[VRAM_OFFSET(tile_x, tile_start)]){
				case 151: //beanstalk tile
					que_state = CLIMB_INIT;
					current_vine_tile_x = tile_x;
					actor_set_dir(&PLAYER, DIR_LEFT, FALSE);
					break;
			}

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
            switch(sram_map_data[VRAM_OFFSET(tile_x, tile_start)]){
				case 151: //beanstalk tile
					que_state = CLIMB_INIT;
					current_vine_tile_x = tile_x;
					actor_set_dir(&PLAYER, DIR_RIGHT, FALSE);
					break;
			}
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

    //Y-COLLISION
    {
        deltaY = CLAMP(deltaY, -127, 127);

        //New Y Slopes 1
        UBYTE prev_grounded = grounded;
        UWORD old_y = PLAYER.pos.y;
        grounded = FALSE;
        //End New Y Slopes 1

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

                        new_y = TILE_TO_SUBPX(tile_y) - EXCLUSIVE_OFFSET(PLAYER.bounds.bottom);
                        actor_attached = FALSE; //Detach when MP moves through a solid tile.
                        //The distinction here is used so that we can check the velocity when the player hits the ground.
                        if(plat_state == GROUND_STATE){
                            que_state = GROUND_STATE;
                            pl_vel_y = 0;
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
    gotoActorColFall:
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

    //ANIMATION--------------------------------------------------------------------------------------------------
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
    //Above: FALL -> GROUND in basic_y_col()

	//FALL -> SWIM check
	if (script_memory[VAR_CANSWIM] != 0 && PLAYER.pos.y > PX_TO_SUBPX(48)){
		que_state = SWIM_INIT;
	}

    //FALL -> JUMP check
    if (INPUT_PRESSED(INPUT_PLATFORM_JUMP)){
        if (ct_val != 0){
        //Coyote Time Jump
            que_state = JUMP_INIT;
        } else if (script_memory[VAR_HASYOSHI] != 0 && dj_val != 0){
        //Double Jump
            dj_val = 0;
            jump_reduction_val += jump_reduction;
            que_state = JUMP_INIT;
        } else {
        // Setting the Jump Buffer when jump is pressed while not on the ground
        jb_val = plat_buffer_max;
        }
    }

	//Pit check
	if (specific_events[FELL_IN_PIT_EVENT].script_addr != 0 && (SUBPX_TO_PX(PLAYER.pos.y)) > (scroll_y + 160)){
		script_execute(specific_events[FELL_IN_PIT_EVENT].script_bank, specific_events[FELL_IN_PIT_EVENT].script_addr, 0, 0);
		que_state = BLANK_INIT;
	}

    //Check for final frame
    if (que_state != FALL_STATE){
        plat_state = FALL_END;
    }

    //COUNTERS
    // Counting down Jump Buffer Window
    // Set in Fall and checked in Ground state
    if (jb_val != 0){
        jb_val -= 1;
    }

    // Counting down Coyote Time Window
    // Set in ground and checked in fall state
    if (ct_val != 0 && que_state != GROUND_STATE){
        ct_val -= 1;
    }
    // Counting down the drop-through floor frames
    // XX Checked in Fall, Wall, Ground, and basic_y_col, set in basic_y_col
    if (nocollide != 0){
        nocollide -= 1;
    }

    //FUNCTION TRIGGERS
    trigger_activate_at_intersection(&PLAYER.bounds, &PLAYER.pos, 0);

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

void swim_state(void) BANKED {
	//INITIALIZE VARS
    UBYTE tile_y = SUBPX_TO_TILE(PLAYER.pos.y + PLAYER.bounds.top + PX_TO_SUBPX(1));
    col = 0;
	WORD temp_y = PLAYER.pos.y;

	if (que_attacking != stat_attacking){
		stat_attacking = que_attacking;
		load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, (que_attacking != 0) ? STATE_ATTACK: STATE_SWIM, PLAYER.animations);
	}

	//Vertical Movement-------------------------------------------------------------------------------------------
    //Add jump force during each jump frame
    if (hold_jump_val !=0){
        //Add the boost per frame amount.
        pl_vel_y -= (jump_per_frame >> 1);
        hold_jump_val -=1;
		actor_attached = FALSE;
    } else {
        //Water gravity
		if (underwater_pit){
			pl_vel_y += plat_grav >> 1;
		} else {
			pl_vel_y += plat_grav >> 2;
		}
    }
    if (underwater_pit){
		pl_vel_y = MIN(pl_vel_y,plat_max_fall_vel  >> 1);
	} else {
		pl_vel_y = MIN(pl_vel_y,plat_max_fall_vel  >> 2);
	}

	if (actor_attached){
        //If the platform has been disabled, detach the player
        if(CHK_FLAG(last_actor->flags, ACTOR_FLAG_DISABLED) == TRUE){
            actor_attached = FALSE;
        //If the player is off the platform to the right, detach from the platform
        } else if (PLAYER.pos.x + PLAYER.bounds.left > last_actor->pos.x + EXCLUSIVE_OFFSET(last_actor->bounds.right)) {
            actor_attached = FALSE;
        //If the player is off the platform to the left, detach
        } else if (PLAYER.pos.x + EXCLUSIVE_OFFSET(PLAYER.bounds.right) < last_actor->pos.x + last_actor->bounds.left){
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
    }

    //Collision ---------------------------------------------------------------------------------------------------
    //Vertical Collision Checks
    deltaY += LEGACY_VEL_TO_SUBPX(pl_vel_y);


    //Horizontal Movement----------------------------------------------------------------------------------------

    //C. ACCELERATION
    if (INPUT_LEFT || INPUT_RIGHT){
        BYTE dir = 1;
        if (INPUT_LEFT){
            dir = -1;
            pl_vel_x = -pl_vel_x;
        }
		if (pl_vel_x > (plat_run_vel >> 1)){
			pl_vel_x = CLAMP(pl_vel_x - 64, plat_min_vel >> 1, pl_vel_x);
		} else {
			pl_vel_x = CLAMP(pl_vel_x + plat_run_acc, plat_min_vel >> 1, plat_run_vel >> 1);
		}

        pl_vel_x *= dir;
        deltaX += LEGACY_VEL_TO_SUBPX(pl_vel_x);
        run_stage = 1;
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

    //D. COLLISIONS
    //X-COLLISION
    gotoXCol:
    {
        deltaX = CLAMP(deltaX, -127, 127);
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
			switch(sram_map_data[VRAM_OFFSET(tile_x, tile_start)]){
				case 151: //beanstalk tile
					que_state = CLIMB_INIT;
					current_vine_tile_x = tile_x;
					actor_set_dir(&PLAYER, DIR_LEFT, FALSE);
					break;
			}

            while (tile_start < tile_end) {

                col = tile_at(tile_x, tile_start);
                if (col & COLLISION_LEFT) {
					new_x = TILE_TO_SUBPX(tile_x) - EXCLUSIVE_OFFSET(PLAYER.bounds.right);
					pl_vel_x = 0;
					col = 1;
					last_wall = 1;
					if (grounded){
						switch(sram_map_data[VRAM_OFFSET(tile_x, tile_start)]){
							case 61: //top part of right pipe
							case 62: //bottom part of right pipe
								if (specific_events[ENTER_RIGHT_PIPE_EVENT].script_addr != 0){
									script_execute(specific_events[ENTER_RIGHT_PIPE_EVENT].script_bank, specific_events[ENTER_RIGHT_PIPE_EVENT].script_addr, 0, 0);
								}
								break;
						}
					}
					game_on_player_metatile_collision(tile_x, tile_start, DIR_RIGHT);
					break;
                } else {
					reset_collision_cache(DIR_RIGHT);
				}
                tile_start++;
            }
        } else if (new_x < PLAYER.pos.x) {
            tile_x = SUBPX_TO_TILE(new_x + PLAYER.bounds.left);
            switch(sram_map_data[VRAM_OFFSET(tile_x, tile_start)]){
				case 151: //beanstalk tile
					que_state = CLIMB_INIT;
					current_vine_tile_x = tile_x;
					actor_set_dir(&PLAYER, DIR_RIGHT, FALSE);
					break;
			}
            while (tile_start < tile_end) {
                col = tile_at(tile_x, tile_start);
                if (col & COLLISION_RIGHT) {

                    new_x = TILE_TO_SUBPX(tile_x + 1) - PLAYER.bounds.left + 1;
                    pl_vel_x = 0;
                    col = -1;
                    last_wall = -1;
					if (grounded){
						switch(sram_map_data[VRAM_OFFSET(tile_x, tile_start)]){
							case 61: //top part of LEFT pipe
							case 62: //bottom part of LEFT pipe
								if (specific_events[ENTER_LEFT_PIPE_EVENT].script_addr != 0){
									script_execute(specific_events[ENTER_LEFT_PIPE_EVENT].script_bank, specific_events[ENTER_LEFT_PIPE_EVENT].script_addr, 0, 0);
								}
								break;
						}
					}
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

    //Y-COLLISION
    {
        deltaY = CLAMP(deltaY, -127, 127);

        //New Y Slopes 1
        UBYTE prev_grounded = grounded;
        UWORD old_y = PLAYER.pos.y;
        grounded = FALSE;
        //End New Y Slopes 1

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
                        new_y = TILE_TO_SUBPX(tile_y) - EXCLUSIVE_OFFSET(PLAYER.bounds.bottom);
						grounded = true;

						if (is_leftmost == 1 && INPUT_DOWN){//check only mario's left leg for left pipe part
							UBYTE tile_id = sram_map_data[VRAM_OFFSET(tile_start, tile_y)];
							if ((tile_id == 57 || tile_id == 233) && specific_events[ENTER_DOWN_PIPE_EVENT].script_addr != 0){
								script_execute(specific_events[ENTER_DOWN_PIPE_EVENT].script_bank, specific_events[ENTER_DOWN_PIPE_EVENT].script_addr, 0, 0);
							}
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
    gotoActorColSwim:
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

    //ANIMATION--------------------------------------------------------------------------------------------------
    //This animation is currently shared by jumping, dashing, and falling. Dashing doesn't need this complexity though.
    //Here velocity overrides direction. Whereas on the ground it is the reverse.

	if (INPUT_PRESSED(INPUT_LEFT)){
		actor_set_dir(&PLAYER, DIR_LEFT, (pl_vel_x != 0)?TRUE:FALSE);
    } else if (INPUT_PRESSED(INPUT_RIGHT)){
		actor_set_dir(&PLAYER, DIR_RIGHT, (pl_vel_x != 0)?TRUE:FALSE);
    } else if (pl_vel_x != 0) {
        actor_set_anim_moving(&PLAYER);
    } else {
		actor_set_anim_idle(&PLAYER);
	}

	if (pl_vel_y < 0){
		PLAYER.anim_tick = 3;
	} else {
		PLAYER.anim_tick = 7;
	}

    //STATE CHANGE------------------------------------------------------------------------------------------------


    //SWIM up check
    if (INPUT_PRESSED(INPUT_PLATFORM_JUMP) && PLAYER.pos.y > PX_TO_SUBPX(48)){
		que_state = SWIM_INIT;
		hold_jump_val = plat_hold_jump_max;
		pl_vel_x = ((INPUT_LEFT)? -4096: (INPUT_RIGHT)? 4096: pl_vel_x);
    }

	//Swim out check
	if (script_memory[VAR_CANSWIM] == 0){
		if (INPUT_PLATFORM_JUMP){
			que_state = JUMP_INIT;
		} else {
			que_state = FALL_INIT;
		}
	}

	//Pit check
	if (specific_events[FELL_IN_PIT_EVENT].script_addr != 0 && (SUBPX_TO_PX(PLAYER.pos.y)) > (scroll_y + 160)){
		script_execute(specific_events[FELL_IN_PIT_EVENT].script_bank, specific_events[FELL_IN_PIT_EVENT].script_addr, 0, 0);
		que_state = BLANK_INIT;
	}

    //Check for final frame
    if (que_state != SWIM_STATE){
        plat_state = SWIM_END;
    }

    //COUNTERS

    // Counting down the drop-through floor frames
    // XX Checked in Fall, Wall, Ground, and basic_y_col, set in basic_y_col
    if (nocollide != 0){
        nocollide -= 1;
    }

    //FUNCTION TRIGGERS
    trigger_activate_at_intersection(&PLAYER.bounds, &PLAYER.pos, 0);

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

void assign_state_script(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UWORD *slot = VM_REF_TO_PTR(FN_ARG2);
    UBYTE *bank = VM_REF_TO_PTR(FN_ARG1);
    UBYTE **ptr = VM_REF_TO_PTR(FN_ARG0);
    state_events[*slot].script_bank = *bank;
    state_events[*slot].script_addr = *ptr;
}

void clear_state_script(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UWORD *slot = VM_REF_TO_PTR(FN_ARG0);
    state_events[*slot].script_bank = NULL;
    state_events[*slot].script_addr = NULL;
}

void assign_specific_script(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UWORD *slot = VM_REF_TO_PTR(FN_ARG2);
    UBYTE *bank = VM_REF_TO_PTR(FN_ARG1);
    UBYTE **ptr = VM_REF_TO_PTR(FN_ARG0);
    specific_events[*slot].script_bank = *bank;
    specific_events[*slot].script_addr = *ptr;
}

void clear_specific_script(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UWORD *slot = VM_REF_TO_PTR(FN_ARG0);
    specific_events[*slot].script_bank = NULL;
    specific_events[*slot].script_addr = NULL;
}
