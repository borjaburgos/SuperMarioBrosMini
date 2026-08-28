#pragma bank 255

#include <string.h>
#include <stdlib.h>
#include <gbdk/platform.h>
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
#include "actor_behavior_b.h"
#include "actor_behavior_c.h"
#include "actor_behavior_d.h"
#include "states/platform.h"
#include "states/playerstates.h"
#include "data/states_defines.h"
#include "meta_tiles.h"
#include "collision.h"
#include "data_manager.h"
#include "data/game_globals.h"

void actor_behavior_update_a2(UBYTE i, actor_t * actor) BANKED {
	switch (current_behavior) {
				case 10://Bouncing entity
				switch(actor_states[i]){
					case 0: //Init
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){ actor_states[i] = 1; }
						break;
					case 1: //Main state
						current_actor_x = ((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						actor_vel_y[i] += (plat_grav >> 11);
						actor_vel_y[i] = MIN(actor_vel_y[i], (plat_max_fall_vel >> 8));
						//Apply velocity
						UWORD new_y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
						UWORD new_x =  actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);
						//Tile Collision
						actor->pos.x = check_collision(new_x, actor->pos.y, &actor->bounds, ((actor->pos.x > new_x) ? CHECK_DIR_LEFT : CHECK_DIR_RIGHT));
						if (actor->pos.x != new_x){
							actor_vel_x[i] = -actor_vel_x[i];
						}
						actor->pos.y = check_collision(actor->pos.x, new_y, &actor->bounds, ((actor->pos.y > new_y) ? CHECK_DIR_UP : CHECK_DIR_DOWN));
						if (actor->pos.y < new_y){
							actor_vel_y[i] = -30;
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
				case 11://Fire ball
				switch(actor_states[i]){
					case 0: //Init
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_states[i] = 1;
						}
						break;
					case 1: //Main state
						current_actor_x = ((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD || (SUBPX_TO_TILE(actor->pos.y)) > image_tile_height){
							actor_states[i] = 255;
							break;
						}
						actor_vel_y[i] += (plat_grav >> 9);
						actor_vel_y[i] = MIN(actor_vel_y[i], (plat_max_fall_vel >> 8));
						//Apply velocity
						UWORD new_y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
						UWORD new_x =  actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);
						//Tile Collision
						actor->pos.x = check_collision(new_x, actor->pos.y, &actor->bounds, ((actor->pos.x > new_x) ? CHECK_DIR_LEFT : CHECK_DIR_RIGHT));
						if (actor->pos.x != new_x){
							script_execute(actor->script.bank, actor->script.ptr, 0, 1, 2);
							actor_states[i] = 255;
							break;
						}
						actor->pos.y = check_collision(actor->pos.x, new_y, &actor->bounds, ((actor->pos.y > new_y) ? CHECK_DIR_UP : CHECK_DIR_DOWN));
						if (actor->pos.y < new_y){
							actor_vel_y[i] = -40;
						} else if (actor->pos.y > new_y){
							actor_vel_y[i] = 0;
						}
						//Actor Collision
						actor_t * hit_actor = actor_overlapping_bb(&actor->bounds, &actor->pos, actor);
						if (hit_actor && hit_actor->script.bank && actor->collision_group != hit_actor->collision_group){
							script_execute(hit_actor->script.bank, hit_actor->script.ptr, 0, 1, 4);
							script_execute(actor->script.bank, actor->script.ptr, 0, 1, 2);
							actor_states[i] = 255;
						}
						break;
					case 255: //Deactivate
						deactivate_actor(actor);
						break;
				}
				break;
				case 12: //Growing Beanstalk
				switch(actor_states[i]){
					case 0:
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_counter_a[i] = 0;
							actor_states[i] = 1;
							actor->pos.y = TILE_TO_SUBPX(SUBPX_TO_TILE(actor->pos.y));
							actor->pos.x = TILE_TO_SUBPX(SUBPX_TO_TILE(actor->pos.x));
						}
						break;
					case 1: //Move up state
						current_actor_x = ((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (actor->pos.y > 0){
							actor->pos.y -= LEGACY_DELTA_TO_SUBPX(8);
							actor_counter_a[i] = actor_counter_a[i] + 1;
							if (actor_counter_a[i] > 15){
								actor_counter_a[i] = 0;
								if (tile_at((SUBPX_TO_TILE(actor->pos.x)), (SUBPX_TO_TILE(actor->pos.y))) & COLLISION_BOTTOM) {
									actor_states[i] = 255;
								} else {
									replace_meta_tile((SUBPX_TO_TILE(actor->pos.x)), (SUBPX_TO_TILE(actor->pos.y)), 151, TRUE);
								}
							}
						}
						else{
							actor_states[i] = 255;
						}
						break;
					case 255:
						actor_counter_a[i] = 0;
						deactivate_actor(actor);
						break;
				}
				break;
				case 13: //Moving platform (activates on player touch)
				switch(actor_states[i]){
					case 0:
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){ actor_states[i] = 1; }
						break;
					case 1: //Not moving state
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) > BEHAVIOR_DEACTIVATION_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (actor_attached && last_actor == actor) {//start moving on player attach
							actor_states[i] = 2;
						}
						break;
					case 2: //moving state
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) > BEHAVIOR_DEACTIVATION_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						actor->pos.x = actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);
						actor->pos.y = actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
						break;
					case 255:
						actor_counter_a[i] = 0;
						deactivate_actor(actor);
						break;
				}
				break;
				case 14: //Spring
				switch(actor_states[i]){
					case 0:
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_states[i] = 1;
						}
						break;
					case 1: //Idle state
						if (actor_attached && last_actor == actor) {//start springing on player attach
							actor_counter_a[i] = 0;
							actor_states[i] = 2;
						}
						break;
					case 2: //Springing state
						que_state = GROUND_STATE;
						if (!actor_attached || last_actor != actor) {
							actor_counter_a[i] = 0;
							actor_states[i] = 1;
							actor->frame = actor->frame_start;
							break;
						}
						if (!(game_time & 1)){
							actor_counter_a[i]++;
							if (actor_counter_a[i] > 4){
								actor_counter_a[i] = 0;
								actor_states[i] = 1;
								load_animations(PLAYER.sprite.ptr, PLAYER.sprite.bank, STATE_DEFAULT, PLAYER.animations);
								hold_jump_val = (plat_hold_jump_max << 1);
								actor_attached = FALSE;
								pl_vel_y = -(plat_jump_min << 1);
								jb_val = 0;
								ct_val = 0;
								enemy_bounce = 1;
								que_state = JUMP_STATE;
							}
							actor->frame = actor->frame_start + actor_counter_a[i];
							PLAYER.pos.y = actor->pos.y + actor->bounds.top + PX_TO_SUBPX(spring_bb_top_lookup[actor_counter_a[i]]);
						}
						break;
					case 255:
						actor_counter_a[i] = 0;
						deactivate_actor(actor);
						break;
				}
				break;
				case 15://Knocked enemy
				switch(actor_states[i]){
					case 0: //Init
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_states[i] = 1;
							actor_vel_y[i] = -40;
							actor_vel_x[i] = 0;
							CLR_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
						}
						break;
					case 1: //Main state
						current_actor_x = ((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD || (SUBPX_TO_TILE(actor->pos.y)) > image_tile_height){
							actor_states[i] = 255;
							break;
						}
						actor_vel_y[i] += (plat_grav >> 10);
						actor_vel_y[i] = MIN(actor_vel_y[i], (plat_max_fall_vel >> 8));
						//Apply velocity
						actor->pos.y =  actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
						break;
					case 255: //Deactivate
						SET_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
						actor_reset_anim(actor);
						deactivate_actor(actor);
						break;
				}
				break;
				case 16: //Elevator platform
				switch(actor_states[i]){
					case 0:
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){ actor_states[i] = 1; }
						break;
					case 1: //moving state
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (actor_vel_y[i] < 0 && actor->pos.y < 0) {
							actor->pos.y += TILE_TO_SUBPX(image_tile_height);
							actor_attached = FALSE;
						} else if (actor_vel_y[i] > 0 && (SUBPX_TO_TILE(actor->pos.y)) > image_tile_height) {
							actor->pos.y -= TILE_TO_SUBPX(image_tile_height);
							actor_attached = FALSE;
						}
						actor->pos.y = actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[i]);
						break;
					case 255:
						actor_counter_a[i] = 0;
						deactivate_actor(actor);
						break;
				}
				break;
				case 17: //Hit block bump
				switch(actor_states[i]){
					case 0:
						//Actor Collision
						for (UBYTE j = 1; j < MAX_ACTORS; j++){
							actor_t * hit_actor = (actors + j);
							if (!CHK_FLAG(hit_actor->flags, ACTOR_FLAG_ACTIVE) || hit_actor == actor || !CHK_FLAG(hit_actor->flags, ACTOR_FLAG_COLLISION)){
								continue;
							}
							if (bb_intersects(&actor->bounds, &actor->pos, &hit_actor->bounds, &hit_actor->pos)) {

								if ((hit_actor->pos.x > actor->pos.x && actor_vel_x[j] < 0) ||
									(hit_actor->pos.x < actor->pos.x && actor_vel_x[j] > 0)) {
									actor_vel_x[j] = -actor_vel_x[j];
								}
								actor_vel_y[j] = -60;
								if (hit_actor->script.bank){
									script_memory[VAR_SHELLCOMBOSCORE] = 0;
									script_execute(hit_actor->script.bank, hit_actor->script.ptr, 0, 1, 2);
								}
							}
						}
						actor_states[i] = 255;
						break;
				}
				break;
				case 18: //Sideway moving actor
				switch(actor_states[i]){
					case 0:
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_states[i] = 1;
							actor_vel_x[i] = 8;
							actor_counter_a[i] = 0;
						}
						break;
					case 1: //Move left state
						current_actor_x = ((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x;
						if (current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (!(game_time & 7)){
							actor_vel_x[i] = MAX(actor_vel_x[i]--, -8);
							if (actor_counter_a[i] <= 16){
								actor_counter_a[i]++;
							} else {
								actor_states[i] = 2;
							}
						}
						actor->pos.x = actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);
						break;
					case 2: //Move right state
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) > BEHAVIOR_DEACTIVATION_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (!(game_time & 7)){
							actor_vel_x[i] = MIN(actor_vel_x[i]++, 8);
							if (actor_counter_a[i] > 0){
								actor_counter_a[i]--;
							} else {
								actor_states[i] = 1;
							}
						}
						actor->pos.x = actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[i]);
						break;
					case 255:
						deactivate_actor(actor);
						break;
				}
				break;
				case 19: //Falling platform (activates on player touch)
				switch(actor_states[i]){
					case 0:
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){ actor_states[i] = 1; }
						break;
					case 1: //Not falling state
						current_actor_x = ((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x;
						if (current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (actor_attached && last_actor == actor) {//start moving on player attach
							actor_states[i] = 2;
						}
						break;
					case 2: //falling state
						if ((((SUBPX_TO_PX(actor->pos.x)) + 8) - draw_scroll_x) > BEHAVIOR_DEACTIVATION_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						actor->pos.y += LEGACY_DELTA_TO_SUBPX(16);
						if (!actor_attached) {//stop moving on player dettach
							actor_states[i] = 1;
						}

						if ((SUBPX_TO_TILE(actor->pos.y)) > image_tile_height) {
							actor_states[i] = 255;
							actor_attached = FALSE;
						}
						break;
					case 255:
						deactivate_actor(actor);
						break;
				}
				break;
	}
}
