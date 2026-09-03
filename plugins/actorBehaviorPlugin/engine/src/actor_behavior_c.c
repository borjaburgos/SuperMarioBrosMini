#pragma bank 255

#include <string.h>
#include <stdlib.h>
#include <gbdk/platform.h>
#include <rand.h>
#include "system.h"
#include "vm.h"
#include "gbs_types.h"
#include "events.h"
#include "input.h"
#include "math.h"
#include "actor.h"
#include "scroll.h"
#include "game_time.h"
#include "actor_behavior.h"
#include "actor_behavior_c.h"
#include "states/platform.h"
#include "states/playerstates.h"
#include "data/states_defines.h"
#include "meta_tiles.h"
#include "collision.h"
#include "data_manager.h"
#include "data/game_globals.h"
#include "ui.h"

UWORD check_collision_c(UWORD start_x, UWORD start_y, rect16_t *bounds, col_check_dir_e check_dir) BANKED{
    switch (check_dir) {
        case CHECK_DIR_LEFT:  // Check left (bottom left)
            col_tx = SUBPX_TO_TILE(start_x + bounds->left);
            col_ty = SUBPX_TO_TILE(start_y + bounds->bottom);
            if (tile_at(col_tx, col_ty) & COLLISION_RIGHT) {
                return TILE_TO_SUBPX(col_tx + 1) - bounds->left;
            }
            return start_x;
        case CHECK_DIR_RIGHT:  // Check right (bottom right)
            col_tx = SUBPX_TO_TILE(start_x + bounds->right);
            col_ty = SUBPX_TO_TILE(start_y + bounds->bottom);
            if (tile_at(col_tx, col_ty) & COLLISION_LEFT) {
                return TILE_TO_SUBPX(col_tx) - EXCLUSIVE_OFFSET(bounds->right);
            }
            return start_x;
        case CHECK_DIR_UP:  // Check up (middle up)
            col_ty = SUBPX_TO_TILE(start_y + bounds->top);
            col_tx = SUBPX_TO_TILE(start_x + ((bounds->left + bounds->right) >> 1));
            if (tile_at(col_tx, col_ty) & COLLISION_BOTTOM) {
                return TILE_TO_SUBPX(col_ty + 1) - bounds->top;
            }
            return start_y;
        case CHECK_DIR_DOWN:  // Check down (right bottom and left bottom)
            col_ty = SUBPX_TO_TILE(start_y + bounds->bottom);
            col_tx = SUBPX_TO_TILE(start_x + bounds->left);
            if (tile_at(col_tx, col_ty) & COLLISION_TOP) {
                return TILE_TO_SUBPX(col_ty) - EXCLUSIVE_OFFSET(bounds->bottom);
            }
			col_tx = SUBPX_TO_TILE(start_x + bounds->right);
			if (tile_at(col_tx, col_ty) & COLLISION_TOP) {
                return TILE_TO_SUBPX(col_ty) - EXCLUSIVE_OFFSET(bounds->bottom);
            }
            return start_y;
    }
    return start_x;
}

void apply_gravity_c(UBYTE actor_idx) BANKED {
	actor_vel_y[actor_idx] += (plat_grav >> 8);
	actor_vel_y[actor_idx] = MIN(actor_vel_y[actor_idx], (plat_max_fall_vel >> 8));
}

void apply_velocity_c(UBYTE actor_idx, actor_t * actor) BANKED {
	//Apply velocity
	new_actor_y = actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[actor_idx]);
	new_actor_x = actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[actor_idx]);
	if (CHK_FLAG(actor->flags, ACTOR_FLAG_COLLISION)){
		//Tile Collision
		actor->pos.x = check_collision_c(new_actor_x, actor->pos.y, &actor->bounds, ((actor->pos.x > new_actor_x) ? CHECK_DIR_LEFT : CHECK_DIR_RIGHT));
		if (actor->pos.x != new_actor_x){
			actor_vel_x[actor_idx] = -actor_vel_x[actor_idx];
		}
		actor->pos.y = check_collision_c(actor->pos.x, new_actor_y, &actor->bounds, ((actor->pos.y > new_actor_y) ? CHECK_DIR_UP : CHECK_DIR_DOWN));
	} else {
		actor->pos.x = new_actor_x;
		actor->pos.y = new_actor_y;
	}
}

void actor_behavior_update_c(UBYTE i, actor_t * actor) BANKED {
	switch(current_behavior){
		case 34://Totomesu
		switch(actor_states[i]){
			case 0: //Init
				if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
					actor_states[i] = 1;
					actor_counter_a[i] = 32;
					actor_counter_b[i] = 0;
					actor_vel_x[i] = 0;
					actor_vel_y[i] = 0;
				}
				break;
			case 1: //Main state
				current_actor_x = ((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x;
				if (current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
					actor_states[i] = 255;
					break;
				}

				apply_gravity_c(i);
				apply_velocity_c(i, actor);
				//Animation
				if (PLAYER.pos.x < actor->pos.x) {
					actor_set_dir(actor, DIR_LEFT, actor_counter_b[i]);
				} else {
					actor_set_dir(actor, DIR_RIGHT, actor_counter_b[i]);
				}
				if (!(game_time & 1)){
					if (!(actor_counter_a[i] & 63) && (PLAYER.pos.x < actor->pos.x)){
						actor_counter_a[i] = rand();
						if (actor_counter_a[i] < 64){
							//jump
							actor_states[i] = 2;
							actor_vel_y[i] = -32;
							actor_vel_x[i] = (rand() & 15) - 8;
							SET_FLAG(actor->flags, ACTOR_FLAG_ANIM_NOLOOP);
						} else {
							//breath fire
							UBYTE attack_idx = actor_linked_actor_idx[i];
							if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255){
								//breath fire
								attack_idx = actor_linked_actor_idx[attack_idx];
								if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255){
									//breath fire
									attack_idx = actor_linked_actor_idx[attack_idx];
								}
							}
							if (attack_idx != 0 && (actor_states[attack_idx] == 0 || actor_states[attack_idx] == 255)){
								actor_t * attack_actor = (actors + attack_idx);
								actor_states[attack_idx] = 0;
								if (!CHK_FLAG(attack_actor->flags, ACTOR_FLAG_ACTIVE)){
									CLR_FLAG(attack_actor->flags, ACTOR_FLAG_DISABLED);
									activate_actor(attack_actor);
								}
								SET_FLAG(attack_actor->flags, ACTOR_FLAG_COLLISION);
								attack_actor->pos.y = actor->pos.y - LEGACY_DELTA_TO_SUBPX(actor_counter_a[i] - 128);
								actor_counter_b[i] = 15;
								actor_set_dir(attack_actor, DIR_LEFT, FALSE);
								attack_actor->pos.x = actor->pos.x - PX_TO_SUBPX(8);
								actor_vel_x[attack_idx]	= -12;
							}
						}
					}
					actor_counter_a[i]++;
					if (actor_counter_b[i] > 0){
						actor_counter_b[i]--;
					}
				}
				break;
			case 2: //Jump state
				if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) > BEHAVIOR_DEACTIVATION_THRESHOLD){
					actor_states[i] = 255;
					break;
				}
				actor_vel_y[i] += (plat_grav >> 11);
				actor_vel_y[i] = MIN(actor_vel_y[i], plat_max_fall_vel >> 9);
				//Apply velocity
				UWORD new_y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				UWORD new_x =  actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);
				//Tile Collision
				actor->pos.x = check_collision_c(new_x, actor->pos.y, &actor->bounds, ((actor->pos.x > new_x) ? CHECK_DIR_LEFT : CHECK_DIR_RIGHT));
				if (actor->pos.x != new_x){
					actor_vel_x[i] = -actor_vel_x[i];
				}
				actor->pos.y = check_collision_c(actor->pos.x, new_y, &actor->bounds, ((actor->pos.y > new_y) ? CHECK_DIR_UP : CHECK_DIR_DOWN));
				if (actor->pos.y < new_y){
					actor_vel_y[i] = 0;
					actor_vel_x[i] = 0;
					CLR_FLAG(actor->flags, ACTOR_FLAG_ANIM_NOLOOP);
					actor_states[i] = 1;
				}
				//Animation
				if (PLAYER.pos.x < actor->pos.x) {
					actor_set_anim(actor, ANIM_JUMP_LEFT);
				} else {
					actor_set_anim(actor, ANIM_JUMP_RIGHT);
				}
				break;
			case 3: //death
				if ((SUBPX_TO_TILE(actor->pos.y)) > (image_tile_height + 4)){
					actor_states[i] = 255;
					break;
				}
				actor_vel_y[i] += (plat_grav >> 10);
				actor_vel_y[i] = MIN(actor_vel_y[i], plat_max_fall_vel >> 8);
				//Apply velocity
				actor->pos.y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				CLR_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
				break;
			case 255: //Deactivate
				deactivate_actor(actor);
				break;
		}
		break;
		case 35://Dragonzamazu
		switch(actor_states[i]){
			case 0:
					if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
						actor_states[i] = 1;
						actor_vel_y[i] = 8;
						actor_vel_x[i] = 0;
					}
					break;
			case 1: //Move up state
				if (!(game_time & 3)){
					actor_vel_y[i] = MAX(actor_vel_y[i]--, -8);
					if (actor->pos.y < PX_TO_SUBPX(80)){
						actor_states[i] = 2;
					}
				}
				actor->pos.y = actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				goto dragonzamazu_mainstate;
			case 2: //Move down state
				if (!(game_time & 3)){
					actor_vel_y[i] = MIN(actor_vel_y[i]++, 8);
					if (actor->pos.y > PX_TO_SUBPX(96)){
						actor_states[i] = 1;
					}
				}
				actor->pos.y = actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
			dragonzamazu_mainstate: //Main state
				//Animation
				if (PLAYER.pos.x < actor->pos.x) {
					actor_set_dir(actor, DIR_LEFT, actor_counter_b[i]);
				} else {
					actor_set_dir(actor, DIR_RIGHT, actor_counter_b[i]);
				}
				if (!(game_time & 1)){
					if (!(actor_counter_a[i] & 63)){
						actor_counter_a[i] = rand();
						if (actor_counter_a[i] < 128 && (PLAYER.pos.x < actor->pos.x)){
							//bullet bill 1
							UBYTE attack_idx = actor_linked_actor_idx[i];
							if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255){
								//bullet bill 2
								attack_idx = actor_linked_actor_idx[attack_idx];
								if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255){
									//bullet bill 3
									attack_idx = actor_linked_actor_idx[attack_idx];
								}
							}
							if (attack_idx != 0 && (actor_states[attack_idx] == 0 || actor_states[attack_idx] == 255)){
								actor_t * attack_actor = (actors + attack_idx);
								actor_behavior_ids[attack_idx] = 6;
								actor_states[attack_idx] = 0;
								if (!CHK_FLAG(attack_actor->flags, ACTOR_FLAG_ACTIVE)){
									CLR_FLAG(attack_actor->flags, ACTOR_FLAG_DISABLED);
									activate_actor(attack_actor);
								}
								SET_FLAG(attack_actor->flags, ACTOR_FLAG_COLLISION);
								attack_actor->pos.x = actor->pos.x;
								attack_actor->pos.y = actor->pos.y - PX_TO_SUBPX(16);
								actor_counter_b[i] = 15;
								actor_set_dir(attack_actor, DIR_LEFT, FALSE);
								actor_vel_x[attack_idx]	= -12;
							}
						}
					}
					actor_counter_a[i]++;
					if (actor_counter_b[i] > 0){
						actor_counter_b[i]--;
					}
				}
				break;
			case 3: //death
				if ((SUBPX_TO_TILE(actor->pos.y)) > (image_tile_height + 4)){
					actor_states[i] = 255;
					break;
				}
				actor_vel_y[i] += (plat_grav >> 10);
				actor_vel_y[i] = MIN(actor_vel_y[i], plat_max_fall_vel >> 8);
				//Apply velocity
				actor->pos.y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				CLR_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
				break;
			case 255: //Deactivate
				deactivate_actor(actor);
				break;
		}
		break;
		case 36://Hiyoihoi
		switch(actor_states[i]){
			case 0:
					if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
						actor_states[i] = 1;
					}
					break;
			case 1: //Main state
				//Animation
				if (!(game_time & 1)){
					actor_set_dir(actor, DIR_LEFT, actor_counter_b[i]);
					if (actor_counter_a[i] > 64){
						actor_counter_a[i] = (rand() & 31);
						if (PLAYER.pos.x < actor->pos.x){
							//boulder 1
							UBYTE attack_idx = actor_linked_actor_idx[i];
							if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255){
								//boulder 2
								attack_idx = actor_linked_actor_idx[attack_idx];
								if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255){
									//boulder 3
									attack_idx = actor_linked_actor_idx[attack_idx];
								}
							}
							if (attack_idx != 0 && (actor_states[attack_idx] == 0 || actor_states[attack_idx] == 255)){
								actor_t * attack_actor = (actors + attack_idx);
								actor_states[attack_idx] = 0;
								if (!CHK_FLAG(attack_actor->flags, ACTOR_FLAG_ACTIVE)){
									CLR_FLAG(attack_actor->flags, ACTOR_FLAG_DISABLED);
									activate_actor(attack_actor);
								}
								SET_FLAG(attack_actor->flags, ACTOR_FLAG_COLLISION);
								attack_actor->pos.x = actor->pos.x;
								attack_actor->pos.y = actor->pos.y - PX_TO_SUBPX(16);
								actor_counter_b[i] = 15;
								actor_set_dir(attack_actor, DIR_LEFT, TRUE);
								actor_vel_x[attack_idx]	= (rand() & 7) - 12;
								actor_vel_y[attack_idx]	= (rand() & 15) - 30;
							}
						}
					}
					actor_counter_a[i]++;
					if (actor_counter_b[i] > 0){
						actor_counter_b[i]--;
					}
				}
				break;
			case 3: //death
				if ((SUBPX_TO_TILE(actor->pos.y)) > (image_tile_height + 4)){
					actor_states[i] = 255;
					break;
				}
				actor_vel_y[i] += (plat_grav >> 10);
				actor_vel_y[i] = MIN(actor_vel_y[i], plat_max_fall_vel >> 8);
				//Apply velocity
				actor->pos.y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				CLR_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
				break;
			case 255: //Deactivate
				deactivate_actor(actor);
				break;
		}
		break;
		case 37://Boulder
		switch(actor_states[i]){
			case 0: //Init
				if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){ actor_states[i] = 1; }
				break;
			case 1: //Main state
				current_actor_x = ((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x;
				if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD || (SUBPX_TO_TILE(actor->pos.y)) > image_tile_height){
					actor_states[i] = 255;
					break;
				}
				actor_vel_y[i] += (plat_grav >> 11);
				actor_vel_y[i] = MIN(actor_vel_y[i], (plat_max_fall_vel >> 8));
				//Apply velocity
				UWORD new_y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				UWORD new_x =  actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);
				//Tile Collision
				actor->pos.x = check_collision_c(new_x, actor->pos.y, &actor->bounds, ((actor->pos.x > new_x) ? CHECK_DIR_LEFT : CHECK_DIR_RIGHT));
				if (actor->pos.x != new_x){
					actor_vel_x[i] = -actor_vel_x[i];
				}
				actor->pos.y = check_collision_c(actor->pos.x, new_y, &actor->bounds, ((actor->pos.y > new_y) ? CHECK_DIR_UP : CHECK_DIR_DOWN));
				if (actor->pos.y < new_y){
					actor_vel_y[i] = -20;
					if (actor_vel_x[i] == 0){
						actor_vel_x[i] = (rand() >= 128) ? -8: 8;
					}
				} else if (actor->pos.y > new_y){
					actor_vel_y[i] = 0;
				}
				//Animation
				if (actor_vel_x[i] < 0) {
					actor_set_dir(actor, DIR_LEFT, TRUE);
				} else if (actor_vel_x[i] > 0) {
					actor_set_dir(actor, DIR_RIGHT, TRUE);
				} else {
					actor_set_anim_idle(actor);
				}
				break;
			case 255: //Deactivate
				deactivate_actor(actor);
				break;
		}
		break;
		case 38://Alt projectile Lakitu
		switch(actor_states[i]){
			case 0: //Init
				if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
					actor_states[i] = 1;
					actor->frame = actor->frame_start;
				}
				break;
			case 1: //Main state
				if (!(game_time & 3)){
					if (actor->pos.x > PLAYER.pos.x){
						actor_set_dir(actor, DIR_LEFT, TRUE);
						if (actor_vel_x[i] > (pl_vel_x >> 8) - 16){
							actor_vel_x[i]--;
						}
					} else {
						actor_set_dir(actor, DIR_RIGHT, TRUE);
						if (actor_vel_x[i] < (pl_vel_x >> 8) + 16){
							actor_vel_x[i]++;
						}
					}
				}
				//Apply velocity
				actor->pos.x =  actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);
				if (!(game_time & 1)){
					if (!(actor_counter_a[i] & 63)){
						actor_counter_a[i] = rand();
						if (actor_counter_a[i] < 128){
							actor->frame = actor->frame_start + 1;
							actor_counter_b[i] = 32;
						}
					}
					actor_counter_a[i]++;
					if (actor_counter_b[i] != 0){
						actor_counter_b[i]--;
						if (actor_counter_b[i] == 0){
							actor->frame = actor->frame_start;
							//throw item 1
							UBYTE item_idx = actor_linked_actor_idx[i];
							if (actor_states[item_idx] != 0 && actor_states[item_idx] != 255){
								//throw item 2
								item_idx = actor_linked_actor_idx[item_idx];
								if (actor_states[item_idx] != 0 && actor_states[item_idx] != 255){
									//throw item 3
									item_idx = actor_linked_actor_idx[item_idx];
								}
							}
							if (item_idx != 0 && (actor_states[item_idx] == 0 || actor_states[item_idx] == 255)){
								actor_t * item_actor = (actors + item_idx);
								actor_states[item_idx] = 0;
								if (!CHK_FLAG(item_actor->flags, ACTOR_FLAG_ACTIVE)){
									CLR_FLAG(item_actor->flags, ACTOR_FLAG_DISABLED);
									activate_actor(item_actor);
								}
								SET_FLAG(item_actor->flags, ACTOR_FLAG_COLLISION);
								item_actor->pos.y = actor->pos.y;
								item_actor->pos.x = actor->pos.x;
								actor_vel_y[item_idx] = -24;
								if ((PLAYER.pos.x - PX_TO_SUBPX(16)) < actor->pos.x) {
									actor_set_dir(item_actor, DIR_LEFT, FALSE);
									actor_vel_x[item_idx]	= (pl_vel_x >> 8) - 8;
								} else {
									actor_set_dir(item_actor, DIR_RIGHT, FALSE);
									actor_vel_x[item_idx]	= (pl_vel_x >> 8) + 8;
								}
							}
						}
					}
				}
				break;
			case 255: //Deactivate
				deactivate_actor(actor);
				break;
		}
		break;
		case 39://Tatanga
		switch(actor_states[i]){
			case 0:
					if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
						actor_states[i] = 1;
						actor_vel_y[i] = 8;
						actor_vel_x[i] = 0;
					}
					break;
			case 1: //Move up state
				if (!(game_time & 3)){
					actor_vel_y[i] = MAX(actor_vel_y[i]--, -8);
					if (actor->pos.y < PX_TO_SUBPX(80)){
						actor_states[i] = 2;
					}
				}
				actor->pos.y = actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				goto tatanga_mainstate;
			case 2: //Move down state
				if (!(game_time & 3)){
					actor_vel_y[i] = MIN(actor_vel_y[i]++, 8);
					if (actor->pos.y > PX_TO_SUBPX(96)){
						actor_states[i] = 1;
					}
				}
				actor->pos.y = actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
			tatanga_mainstate: //Main state
				//Animation
				if (!(game_time & 3)){
					if (actor->pos.x > PLAYER.pos.x){
						actor_set_dir(actor, DIR_LEFT, TRUE);
					} else {
						actor_set_dir(actor, DIR_RIGHT, TRUE);
					}
					if ((SUBPX_TO_PX(actor->pos.x)) > draw_scroll_x + ((script_memory[VAR_BOWSER_COUNTER] == 1)? 144: 128)){
						if (actor_vel_x[i] > 16){
							actor_vel_x[i] = 0;
						}
						if (actor_vel_x[i] > -16){
							actor_vel_x[i]--;
						}
					} else {
						if (actor_vel_x[i] < 16){
							actor_vel_x[i]++;
						}
					}
				}
				if ((script_memory[VAR_BOWSER_COUNTER] == 1) && (SUBPX_TO_PX(actor->pos.x)) < draw_scroll_x + 120){
					actor_vel_x[i] = 24;
				}
				actor->pos.x =  actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);

				if (!(game_time & 1)){
					if (!(actor_counter_a[i] & 63)){
						actor_counter_a[i] = rand();
						if (actor_counter_a[i] < 128 && (PLAYER.pos.x < actor->pos.x)){
							//Attack 1
							UBYTE attack_idx = actor_linked_actor_idx[i];
							if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255 || rand() > 128){
								//Attack 2
								attack_idx = actor_linked_actor_idx[attack_idx];
							}
							if (attack_idx != 0 && (actor_states[attack_idx] == 0 || actor_states[attack_idx] == 255)){
								actor_t * attack_actor = (actors + attack_idx);
								actor_states[attack_idx] = 0;
								if (!CHK_FLAG(attack_actor->flags, ACTOR_FLAG_ACTIVE)){
									CLR_FLAG(attack_actor->flags, ACTOR_FLAG_DISABLED);
									activate_actor(attack_actor);
								}
								SET_FLAG(attack_actor->flags, ACTOR_FLAG_COLLISION);
								attack_actor->pos.x = actor->pos.x;
								attack_actor->pos.y = actor->pos.y;
								actor_set_dir(attack_actor, DIR_LEFT, FALSE);
								actor_vel_x[attack_idx]	= 0;
							}
						}
					}
					actor_counter_a[i]++;
				}
				break;
			case 3: //death
				if ((SUBPX_TO_TILE(actor->pos.y)) > (image_tile_height + 4)){
					actor_states[i] = 255;
					break;
				}
				actor_vel_y[i] += (plat_grav >> 10);
				actor_vel_y[i] = MIN(actor_vel_y[i], plat_max_fall_vel >> 8);
				//Apply velocity
				actor->pos.y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				CLR_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
				break;
			case 255: //Deactivate
				deactivate_actor(actor);
				break;
		}
		break;
		case 40: //Wario
		switch(actor_states[i]){
			case 0:
				if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
					actor_states[i] = 1;
					actor_vel_y[i] = 0;
					actor_vel_x[i] = 0;
					actor_counter_a[i] = rand();
				}
				break;
			case 1: //Grounded
				actor_vel_y[i] += (plat_grav >> 8);
				actor_vel_y[i] = MIN(actor_vel_y[i], (plat_max_fall_vel >> 8));
				actor_states[i] = 4;
				//animation
				if (actor_vel_x[i] > 0){
					actor_set_dir(actor, DIR_RIGHT, TRUE);
				} else if (actor_vel_x[i] < 0){
					actor_set_dir(actor, DIR_LEFT, TRUE);
				} else if (actor->pos.x < PLAYER.pos.x){
					actor_set_dir(actor, DIR_RIGHT, FALSE);
				} else {
					actor_set_dir(actor, DIR_LEFT, FALSE);
				}
				if (!(game_time & 3) && script_memory[VAR_BOWSER_COUNTER] == 2){
					if (!(actor_counter_a[i] & 31)){
						if (rand() < 200){
							actor_states[i] = 2;
							break;
						} else {
							actor_vel_x[i] = -actor_vel_x[i];
							actor_counter_a[i] = rand();
						}
					}
					actor_counter_a[i]++;
				}
				goto wario_mainstate;
			case 2: //init jump
				SET_FLAG(actor->flags, ACTOR_FLAG_ANIM_NOLOOP);
				actor_vel_y[i] = -30;
				actor_counter_a[i] = 10;
				actor_states[i] = 3;
				if (actor_vel_x[i] > 0){
					actor_set_anim(actor, ANIM_JUMP_RIGHT);
				} else {
					actor_set_anim(actor, ANIM_JUMP_LEFT);
				}
			case 3: //Jump
				if (actor_counter_a[i] !=0){
					actor_vel_y[i] -= 1;
					actor_counter_a[i] -=1;
				} else if (actor_vel_y[i] < 0){
					actor_vel_y[i] += (plat_hold_grav >> 8);
				} else if (actor_vel_y[i] >= 0){
					CLR_FLAG(actor->flags, ACTOR_FLAG_ANIM_NOLOOP);
					actor_states[i] = 4;
					actor_counter_a[i] = 0;
					actor_vel_y[i] += (plat_grav >> 8);
				} else {
					actor_vel_y[i] += (plat_grav >> 8);
				}
				goto wario_mainstate;
			case 4: //Falling
				actor_vel_y[i] += (plat_grav >> 8);
				actor_vel_y[i] = MIN(actor_vel_y[i], (plat_max_fall_vel >> 8));
			wario_mainstate: //Main state
				//Apply velocity
				UWORD new_y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				UWORD new_x =  actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);
				//Tile Collision
				actor->pos.x = check_collision_c(new_x, actor->pos.y, &actor->bounds, ((actor->pos.x > new_x) ? CHECK_DIR_LEFT : CHECK_DIR_RIGHT));
				if (script_memory[VAR_BOWSER_COUNTER] == 2 && (actor->pos.x != new_x || (SUBPX_TO_PX(actor->pos.x)) < draw_scroll_x || (SUBPX_TO_PX(actor->pos.x)) > draw_scroll_x + 144)){
					actor_vel_x[i] = -actor_vel_x[i];
				}
				actor->pos.y = check_collision_c(actor->pos.x, new_y, &actor->bounds, ((actor->pos.y > new_y) ? CHECK_DIR_UP : CHECK_DIR_DOWN));
				if (actor->pos.y < new_y && actor_states[i] != 1){ //grounded
					actor_states[i] = 1;
					actor_counter_a[i] = rand();
				} else if (actor->pos.y > new_y){
					actor_vel_y[i] = 0;
					UBYTE tile_id = sram_map_data[VRAM_OFFSET(col_tx, col_ty)];
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
						case 169://beanstalk brick
						case 171://egg brick
						case 172://egg block
						if(specific_events[HIT_BLOCK_EVENT].script_addr != 0){
							script_memory[VAR_HITBLOCKID] = tile_id;
							script_memory[VAR_HITBLOCKX] = col_tx;
							script_memory[VAR_HITBLOCKY] = col_ty;
							script_memory[VAR_HITBLOCKSOURCE] = i;
							script_execute(specific_events[HIT_BLOCK_EVENT].script_bank, specific_events[HIT_BLOCK_EVENT].script_addr, 0, 0);
						}
						break;
						default:
						if (actor->script.bank){
							script_execute(actor->script.bank, actor->script.ptr, 0, 1, 8);
						}
						break;
					}
				}
				break;
			case 5: //static
				break;
			case 6: //hurt init
				actor_counter_a[i] = (rand() & 15) + 15;
				actor_states[i] = 7;
			case 7: //hurt
				if (!(game_time & 3)){
					actor_counter_a[i]--;
					if (actor_counter_a[i] <= 0){
						actor_states[i] = 1;
						load_animations(actor->sprite.ptr, actor->sprite.bank, STATE_DEFAULT, actor->animations);
					}
				}
				break;
			case 8: //death
				if ((SUBPX_TO_TILE(actor->pos.y)) > (image_tile_height + 4)){
					actor_states[i] = 255;
					break;
				}
				actor_vel_y[i] += (plat_grav >> 10);
				actor_vel_y[i] = MIN(actor_vel_y[i], plat_max_fall_vel >> 8);
				//Apply velocity
				actor->pos.y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				CLR_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
				break;
			case 255: //Deactivate
				deactivate_actor(actor);
				break;
		}
		break;
		case 41://DonkeyKong
		switch(actor_states[i]){
			case 0:
					if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
						actor_states[i] = 1;
						actor_counter_b[i] = 0;
						actor_counter_a[i] = 0;
					}
					break;
			case 1: //Main state
				//Animation
				if (!(game_time & 1)){
					if (actor_counter_b[i]){
						actor_set_dir(actor, actor->dir, actor_counter_b[i]);
					} else {
						actor_set_dir(actor, DIR_DOWN, FALSE);
					}
					if (!(actor_counter_a[i] & 127)){
						actor_counter_a[i] = rand() & 63;
						if (actor_counter_a[i] < 48){
							//barrel 1
							UBYTE attack_idx = actor_linked_actor_idx[i];
							if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255){
								//barrel 2
								attack_idx = actor_linked_actor_idx[attack_idx];
								if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255){
									//barrel 3
									attack_idx = actor_linked_actor_idx[attack_idx];
									if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255){
										//barrel 4
										attack_idx = actor_linked_actor_idx[attack_idx];
										if (actor_states[attack_idx] != 0 && actor_states[attack_idx] != 255){
											//barrel 5
											attack_idx = actor_linked_actor_idx[attack_idx];
										}
									}
								}
							}
							if (attack_idx != 0 && (actor_states[attack_idx] == 0 || actor_states[attack_idx] == 255)){
								actor_t * attack_actor = (actors + attack_idx);
								actor_behavior_ids[attack_idx] = 42;
								actor_states[attack_idx] = 0;
								load_animations(attack_actor->sprite.ptr, attack_actor->sprite.bank, STATE_DEFAULT, attack_actor->animations);
								if (!CHK_FLAG(attack_actor->flags, ACTOR_FLAG_ACTIVE)){
									CLR_FLAG(attack_actor->flags, ACTOR_FLAG_DISABLED);
									activate_actor(attack_actor);
								}
								SET_FLAG(attack_actor->flags, ACTOR_FLAG_COLLISION);
								attack_actor->pos.y = actor->pos.y;
								if (PLAYER.pos.x < actor->pos.x) {
									actor_set_dir(actor, DIR_LEFT, TRUE);
									actor_set_dir(attack_actor, DIR_LEFT, TRUE);
									attack_actor->pos.x = TILE_TO_SUBPX(SUBPX_TO_TILE(actor->pos.x) - 1);
									actor_vel_x[attack_idx]	= -16;
								} else {
									actor_set_dir(actor, DIR_RIGHT, TRUE);
									actor_set_dir(attack_actor, DIR_RIGHT, TRUE);
									attack_actor->pos.x = TILE_TO_SUBPX(SUBPX_TO_TILE(actor->pos.x) + 1);
									actor_vel_x[attack_idx]	= 16;
								}
								actor_counter_b[i] = 15;
							}
						}
					}
					actor_counter_a[i]++;
					if (actor_counter_b[i] > 0){
						actor_counter_b[i]--;
					}
				}
				break;
			case 2: //static
				break;
			case 3: //death
				if ((SUBPX_TO_TILE(actor->pos.y)) > (image_tile_height + 4)){
					actor_states[i] = 255;
					break;
				}
				actor_vel_y[i] += (plat_grav >> 10);
				actor_vel_y[i] = MIN(actor_vel_y[i], plat_max_fall_vel >> 8);
				//Apply velocity
				actor->pos.y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
				CLR_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
				break;
			case 255: //Deactivate
				deactivate_actor(actor);
				break;
		}
		break;
		case 42: //Barrel
		switch(actor_states[i]){
			case 0: //Init
				if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
					actor_states[i] = 1;
					actor_counter_a[i] = 0;
					actor_counter_b[i] = 0;
				}
				break;
			case 1: //Main state
				if (!(actor_counter_b[i] & 1)){
					if ((SUBPX_TO_PX(actor->pos.y)) > (draw_scroll_y + 144)){
						actor_states[i] = 255;
						break;
					}
					actor_vel_y[i] += (plat_grav >> 11);
					actor_vel_y[i] = MIN(actor_vel_y[i], (plat_max_fall_vel >> 8));
					//Apply velocity
					UWORD new_y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
					UWORD new_x =  actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);
					//Tile Collision
					actor->pos.x = check_collision_c(new_x, actor->pos.y, &actor->bounds, ((actor->pos.x > new_x) ? CHECK_DIR_LEFT : CHECK_DIR_RIGHT));
					if (actor->pos.x != new_x){
						actor_vel_x[i] = -actor_vel_x[i];
					}
					actor->pos.y = check_collision_c(actor->pos.x, new_y, &actor->bounds, ((actor->pos.y > new_y) ? CHECK_DIR_UP : CHECK_DIR_DOWN));
					if (actor->pos.y != new_y){
						if (actor_vel_x[i] == 0){
							actor_vel_x[i] = (rand() >= 128) ? -16: 16;
						}
					} else {
						actor_vel_x[i] = 0;
					}
					//Animation
					if (actor_vel_x[i] < 0) {
						actor_set_dir(actor, DIR_LEFT, TRUE);
					} else if (actor_vel_x[i] > 0) {
						actor_set_dir(actor, DIR_RIGHT, TRUE);
					} else if (actor_vel_y[i] > 0) {
						actor_set_dir(actor, DIR_DOWN, TRUE);
					} else {
						actor_set_anim_idle(actor);
					}
				}
				actor_counter_b[i]++;
				break;
			case 255: //Deactivate
				SET_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
				actor_counter_a[i] = 0;
				deactivate_actor(actor);
				break;
		}
		break;
	}
}
