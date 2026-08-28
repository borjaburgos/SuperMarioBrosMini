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

void actor_behavior_update_a1(UBYTE i, actor_t * actor) BANKED {
	switch (current_behavior) {
				case 0:
				break;
				case 1: //Goomba
				switch(actor_states[i]){
					case 0: //Init
						if ((((actor->pos.x >> 4) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_states[i] = 1;
							actor_counter_a[i] = 0;
							actor_counter_b[i] = 0;
							actor_vel_y[i] = 0;
							actor_vel_x[i] = -16;
						}
						break;
					case 1: //Main state
						if (!(actor_counter_b[i] & 3)){
							current_actor_x = ((actor->pos.x >> 4) + 8) - draw_scroll_x;
							if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
								actor_states[i] = 255;
								break;
							}
							apply_gravity(i);
							apply_velocity(i, actor);
							//Animation
							if (actor_vel_x[i] < 0) {
								actor_set_dir(actor, DIR_LEFT, TRUE);
							} else if (actor_vel_x[i] > 0) {
								actor_set_dir(actor, DIR_RIGHT, TRUE);
							} else {
								actor_set_anim_idle(actor);
							}
							if (new_actor_y == actor->pos.y){
								actor_states[i] = 3;
								actor_vel_x[i] = actor_vel_x[i] >> 2;
							}
						}
						actor_counter_b[i]++;
						break;
					case 2: //Squished state
						if (actor_counter_a[i] == 0){
							actor_reset_anim(actor);
							actor_vel_y[i] = 0;
							actor_vel_x[i] = 0;
							CLR_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
						}
						actor_counter_a[i]++;
						if (actor_counter_a[i] > 30){
							actor_states[i] = 255;
						}
						break;
					case 3: //Falling
						current_actor_x = ((actor->pos.x >> 4) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						apply_gravity(i);
						apply_velocity(i, actor);
						//Animation
						if (actor_vel_x[i] < 0) {
							actor_set_dir(actor, DIR_LEFT, TRUE);
						} else if (actor_vel_x[i] > 0) {
							actor_set_dir(actor, DIR_RIGHT, TRUE);
						} else {
							actor_set_anim_idle(actor);
						}
						if (new_actor_y != actor->pos.y){
							actor_states[i] = 1;
							actor_vel_x[i] = actor_vel_x[i] << 2;
						}
						break;
					case 255: //Deactivate
						SET_FLAG(actor->flags, ACTOR_FLAG_COLLISION);
						actor_counter_a[i] = 0;
						deactivate_actor(actor);
						break;
				}
				break;
				case 2: //Green Koopa
				switch(actor_states[i]){
					case 0:
						if ((((actor->pos.x >> 4) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_states[i] = 1;
							actor_vel_y[i] = 0;
							actor_vel_x[i] = -16;
						}
						break;
					case 1: //Main state
						if (!(actor_counter_b[i] & 3)){
							current_actor_x = ((actor->pos.x >> 4) + 8) - draw_scroll_x;
							if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
								actor_states[i] = 255;
								break;
							}
							apply_gravity(i);
							apply_velocity(i, actor);
							//Animation
							if (actor_vel_x[i] < 0) {
								actor_set_dir(actor, DIR_LEFT, TRUE);
							} else if (actor_vel_x[i] > 0) {
								actor_set_dir(actor, DIR_RIGHT, TRUE);
							} else {
								actor_set_anim_idle(actor);
							}
							if (new_actor_y == actor->pos.y){
								actor_states[i] = 2;
								actor_vel_x[i] = actor_vel_x[i] >> 2;
							}
						}
						actor_counter_b[i]++;
						break;
					case 2: //Falling
						current_actor_x = ((actor->pos.x >> 4) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						apply_gravity(i);
						apply_velocity(i, actor);
						//Animation
						if (actor_vel_x[i] < 0) {
							actor_set_dir(actor, DIR_LEFT, TRUE);
						} else if (actor_vel_x[i] > 0) {
							actor_set_dir(actor, DIR_RIGHT, TRUE);
						} else {
							actor_set_anim_idle(actor);
						}
						if (new_actor_y != actor->pos.y){
							actor_states[i] = 1;
							actor_vel_x[i] = actor_vel_x[i] << 2;
						}
						break;
					case 255:
						deactivate_actor(actor);
						break;
				}
				break;
				case 3: //Red Koopa
				switch(actor_states[i]){
					case 0:
						if ((((actor->pos.x >> 4) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_states[i] = 1;
							actor_vel_y[i] = 0;
							actor_vel_x[i] = -16;
						}
						break;
					case 1: //Main state
						if (!(actor_counter_b[i] & 3)){
							current_actor_x = ((actor->pos.x >> 4) + 8) - draw_scroll_x;
							if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
								actor_states[i] = 255;
								break;
							}
							apply_gravity(i);
							apply_velocity_avoid_fall(i, actor);
							//Animation
							if (actor_vel_x[i] < 0) {
								actor_set_dir(actor, DIR_LEFT, TRUE);
							} else if (actor_vel_x[i] > 0) {
								actor_set_dir(actor, DIR_RIGHT, TRUE);
							} else {
								actor_set_anim_idle(actor);
							}
						}
						actor_counter_b[i]++;
						break;
					case 255:
						deactivate_actor(actor);
						break;
				}
				break;
				case 4: //Koopa shell
				switch(actor_states[i]){
					case 0:
						if ((((actor->pos.x >> 4) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_counter_a[i] = 0;
							actor_vel_y[i] = 0;
							actor_vel_x[i] = 0;
							actor_states[i] = 1;
						}
						break;
					case 1: //tucked state
						current_actor_x = ((actor->pos.x >> 4) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						apply_gravity(i);
						apply_velocity(i, actor);
						break;
					case 2: //init kicked state
						actor_counter_a[i] = 0;
						actor_states[i] = 3;
						script_memory[VAR_SHELLCOMBOSCORE] = 0;
					case 3: //kicked state player iframe
						if (actor_counter_a[i] > 15){
							actor_states[i] = 4;
						} else {
							actor_counter_a[i]++;
						}
					case 4: //kicked state
						current_actor_x = ((actor->pos.x >> 4) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						apply_gravity(i);
						//Apply velocity
						WORD new_y =  actor->pos.y + actor_vel_y[i];
						WORD new_x =  actor->pos.x + actor_vel_x[i];
						if (CHK_FLAG(actor->flags, ACTOR_FLAG_COLLISION)){
							//Tile Collision
							actor->pos.x = check_collision(new_x, actor->pos.y, &actor->bounds, ((actor->pos.x > new_x) ? CHECK_DIR_LEFT : CHECK_DIR_RIGHT));
							if (actor->pos.x != new_x){
								actor_vel_x[i] = -actor_vel_x[i];
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
									case 198://Big coin block (top left)
									case 199://Big coin block (top right)
									case 200://Big coin block (bottom left)
									case 201://Big coin block (bottom right)
									case 210://Big brick
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
							actor->pos.y = check_collision(actor->pos.x, new_y, &actor->bounds, ((actor->pos.y > new_y) ? CHECK_DIR_UP : CHECK_DIR_DOWN));
						} else {
							actor->pos.x = new_x;
							actor->pos.y = new_y;
						}
						//Actor Collision
						actor_t * hit_actor = actor_overlapping_bb(&actor->bounds, &actor->pos, actor);
						if (hit_actor && hit_actor->script.bank){
							script_execute(hit_actor->script.bank, hit_actor->script.ptr, 0, 1, 2);
						}
						break;
					case 255:
						deactivate_actor(actor);
						script_memory[VAR_SHELLCOMBOSCORE] = 0;
						break;
				}
				break;
				case 5: //Powerup
				apply_gravity(i);
				apply_velocity(i, actor);
				break;
				case 6://Horizontal projectile (Bowser fire, bullet bill, tatanga attack 1)
				switch(actor_states[i]){
					case 0: //Init
						if ((((actor->pos.x >> 4)) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_states[i] = 1;
							if (actor_vel_x[i] == 0){
								actor_vel_x[i] = -12;
							}
							if (actor->script.bank){
								script_execute(actor->script.bank, actor->script.ptr, 0, 1, 8);
							}
						}
						break;
					case 1: //Main state
						current_actor_x = ((actor->pos.x >> 4) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						actor->pos.x =  actor->pos.x + actor_vel_x[i];
						break;
					case 255: //Deactivate
						deactivate_actor(actor);
						break;
				}
				break;
				case 7://Pyrahna Plant
				switch(actor_states[i]){
					case 0: //Init
						if ((((actor->pos.x >> 4) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_states[i] = 1;
							actor_counter_a[i] = 30;
							actor_vel_y[i] = 0;
						}
						break;
					case 1: //Main state
						current_actor_x = ((actor->pos.x >> 4) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (actor_vel_y[i] > 0){
							actor->pos.y += 16;
							actor_vel_y[i]--;
						}
						else if (((actor->pos.y >> 7) - 2) != PLAYER.pos.y >> 7){ //dont pop out if player is on top
							actor_counter_a[i]--;
							if (actor_counter_a[i] <= 0){
								actor_counter_a[i] = 120;
								actor_vel_y[i] = 16;
								actor_states[i] = 2;
							}
						}
						break;
					case 2: //Out state
						if ((((actor->pos.x >> 4) + 8) - draw_scroll_x) > BEHAVIOR_DEACTIVATION_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (actor_vel_y[i] > 0){
							actor->pos.y -= 16;
							actor_vel_y[i]--;
						}
						actor_counter_a[i]--;
						if (actor_counter_a[i] <= 0){
							actor_counter_a[i] = 180;
							actor_vel_y[i] = 16;
							actor_states[i] = 1;
						}
						break;
					case 255: //Deactivate
						deactivate_actor(actor);
						break;
				}
				break;
				case 8: //Up-down moving actor (Flying Red Koopa, platforms)
				switch(actor_states[i]){
					case 0:
						if ((((actor->pos.x >> 4) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){
							actor_states[i] = 1;
							actor_vel_y[i] = 8;
							actor_counter_a[i] = 0;
						}
						break;
					case 1: //Move up state
						if ((((actor->pos.x >> 4) + 8) - draw_scroll_x) > BEHAVIOR_DEACTIVATION_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (!(game_time & 7)){
							actor_vel_y[i] = MAX(actor_vel_y[i]--, -8);
							if (actor_counter_a[i] <= 16){
								actor_counter_a[i]++;
							} else {
								actor_states[i] = 2;
							}
						}
						actor->pos.y = actor->pos.y + actor_vel_y[i];
						break;
					case 2: //Move down state
						if ((((actor->pos.x >> 4) + 8) - draw_scroll_x) > BEHAVIOR_DEACTIVATION_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (!(game_time & 7)){
							actor_vel_y[i] = MIN(actor_vel_y[i]++, 8);
							if (actor_counter_a[i] > 0){
								actor_counter_a[i]--;
							} else {
								actor_states[i] = 1;
							}
						}
						actor->pos.y = actor->pos.y + actor_vel_y[i];
						break;
					case 255:
						deactivate_actor(actor);
						break;
				}
				break;
				case 9://Fire bar
				switch(actor_states[i]){
					case 0: //Init
						if ((((actor->pos.x >> 4) + 8) - draw_scroll_x) < BEHAVIOR_ACTIVATION_THRESHOLD){ actor_states[i] = 1; }
						break;
					case 1: //Main state
						current_actor_x = ((actor->pos.x >> 4) + 8) - draw_scroll_x;
						if (current_actor_x > BEHAVIOR_DEACTIVATION_THRESHOLD || current_actor_x < BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD){
							actor_states[i] = 255;
							break;
						}
						if (!(game_time & 15)){
							actor_counter_a[i] = (actor_counter_a[i] + 1) & 15;
							actor->frame = actor->frame_start + actor_counter_a[i];
						}
						tmp_point.x = (actor->pos.x >> 4) + 4;
						tmp_point.y = (actor->pos.y >> 4) - 28;
						for (UBYTE j = 0; j < 4; j++){
							if (bb_contains(&PLAYER.bounds, &PLAYER.pos, &tmp_point)){
								script_execute(actor->script.bank, actor->script.ptr, 0, 1, 0);
								break;
							}
							tmp_point.x += firebar_incx_lookup[actor_counter_a[i]];
							tmp_point.y += firebar_incy_lookup[actor_counter_a[i]];
						}
						break;
					case 255: //Deactivate
						deactivate_actor(actor);
						break;
				}
				break;
	}
}
