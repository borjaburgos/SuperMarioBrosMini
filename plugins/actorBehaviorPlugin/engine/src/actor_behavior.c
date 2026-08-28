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

UBYTE actor_behavior_ids[MAX_ACTORS];
UBYTE actor_states[MAX_ACTORS];
WORD actor_vel_x[MAX_ACTORS];
WORD actor_vel_y[MAX_ACTORS];
UBYTE actor_counter_a[MAX_ACTORS];
UBYTE actor_counter_b[MAX_ACTORS];
UBYTE actor_linked_actor_idx[MAX_ACTORS];

UBYTE current_behavior;
UWORD new_actor_x;
UWORD new_actor_y;
WORD col_tx;
WORD col_ty;
WORD current_actor_x;
upoint16_t tmp_point;
const BYTE firebar_incx_lookup[] = { 0, 3, 6, 7, 8, 7, 6, 3, 0, -3, -6, -7, -8, -7, -6, -3 };
const BYTE firebar_incy_lookup[] = { -8, -7, -6, -3, 0, 3, 6, 7, 8, 7, 6, 3, 0, -3, -6, -7 };
const BYTE spring_bb_top_lookup[] = { -8, -4, 0, -4, -8 };

void actor_behavior_init(void) BANKED {
    memset(actor_behavior_ids, 0, sizeof(actor_behavior_ids));
	memset(actor_states, 0, sizeof(actor_states));
	memset(actor_vel_x, 0, sizeof(actor_vel_x));
	memset(actor_vel_y, 0, sizeof(actor_vel_y));
	memset(actor_counter_a, 0, sizeof(actor_counter_a));
	memset(actor_counter_b, 0, sizeof(actor_counter_b));
	memset(actor_linked_actor_idx, 0, sizeof(actor_linked_actor_idx));
}

UWORD check_collision(UWORD start_x, UWORD start_y, rect16_t *bounds, col_check_dir_e check_dir) BANKED{
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

UWORD check_pit(UWORD start_x, UWORD start_y, rect16_t *bounds, col_check_dir_e check_dir) BANKED {
     WORD tx, ty;
    switch (check_dir) {
        case CHECK_DIR_LEFT:  // Check left (bottom left)
            tx = SUBPX_TO_TILE(start_x + bounds->left);
            ty = SUBPX_TO_TILE(start_y + bounds->bottom) + 1;
            if (!(tile_at(tx, ty) & COLLISION_TOP)) {
                return TILE_TO_SUBPX(tx + 1) - bounds->left;
            }
            return start_x;
        case CHECK_DIR_RIGHT:  // Check right (bottom right)
            tx = SUBPX_TO_TILE(start_x + bounds->right);
            ty = SUBPX_TO_TILE(start_y + bounds->bottom) + 1;
            if (!(tile_at(tx, ty) & COLLISION_TOP)) {
                return TILE_TO_SUBPX(tx) - EXCLUSIVE_OFFSET(bounds->right);
            }
            return start_x;
    }
    return start_x;
}

void apply_gravity(UBYTE actor_idx) BANKED {
	actor_vel_y[actor_idx] += (plat_grav >> 8);
	actor_vel_y[actor_idx] = MIN(actor_vel_y[actor_idx], (plat_max_fall_vel >> 8));
}

void apply_velocity(UBYTE actor_idx, actor_t * actor) BANKED {
	//Apply velocity
	new_actor_y = actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[actor_idx]);
	new_actor_x = actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[actor_idx]);
	if (CHK_FLAG(actor->flags, ACTOR_FLAG_COLLISION)){
		//Tile Collision
		actor->pos.x = check_collision(new_actor_x, actor->pos.y, &actor->bounds, ((actor->pos.x > new_actor_x) ? CHECK_DIR_LEFT : CHECK_DIR_RIGHT));
		if (actor->pos.x != new_actor_x){
			actor_vel_x[actor_idx] = -actor_vel_x[actor_idx];
		}
		actor->pos.y = check_collision(actor->pos.x, new_actor_y, &actor->bounds, ((actor->pos.y > new_actor_y) ? CHECK_DIR_UP : CHECK_DIR_DOWN));
	} else {
		actor->pos.x = new_actor_x;
		actor->pos.y = new_actor_y;
	}
}

void apply_velocity_avoid_fall(UBYTE actor_idx, actor_t * actor) BANKED {
	//Apply velocity
	new_actor_y = actor->pos.y + LEGACY_DELTA_TO_SUBPX(actor_vel_y[actor_idx]);
	new_actor_x = actor->pos.x + LEGACY_DELTA_TO_SUBPX(actor_vel_x[actor_idx]);
	if (CHK_FLAG(actor->flags, ACTOR_FLAG_COLLISION)){
		//Tile Collision
		actor->pos.y = check_collision(actor->pos.x, new_actor_y, &actor->bounds, ((actor->pos.y > new_actor_y) ? CHECK_DIR_UP : CHECK_DIR_DOWN));
		if (new_actor_y != actor->pos.y){
			actor->pos.x = check_pit(new_actor_x, actor->pos.y, &actor->bounds, ((actor_vel_x[actor_idx] > 0) ? CHECK_DIR_RIGHT : CHECK_DIR_LEFT));
			if (actor->pos.x != new_actor_x){
				actor_vel_x[actor_idx] = -actor_vel_x[actor_idx];
				return;
			}
		}
		actor->pos.x = check_collision(new_actor_x, actor->pos.y, &actor->bounds, ((actor_vel_x[actor_idx] > 0) ? CHECK_DIR_RIGHT : CHECK_DIR_LEFT));
		if (actor->pos.x != new_actor_x){
			actor_vel_x[actor_idx] = -actor_vel_x[actor_idx];
		}

	} else {
		actor->pos.x = new_actor_x;
		actor->pos.y = new_actor_y;
	}
}

void actor_behavior_update(void) BANKED {
	for (UBYTE i = 0; i < MAX_ACTORS; i++) {
		actor_t * actor = actors + i;
		if (!CHK_FLAG(actor->flags, ACTOR_FLAG_ACTIVE)) continue;

		current_behavior = actor_behavior_ids[i];
		if (current_behavior < 10) actor_behavior_update_a1(i, actor);
		else if (current_behavior < 20) actor_behavior_update_a2(i, actor);
		else if (current_behavior < 34) actor_behavior_update_b(i, actor);
		else if (current_behavior < 44) actor_behavior_update_c(i, actor);
		else actor_behavior_update_d(i, actor);
	}
}

void vm_set_actor_behavior(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UBYTE actor_idx = *(uint8_t *)VM_REF_TO_PTR(FN_ARG0);
    UBYTE behavior_id = *(uint8_t *)VM_REF_TO_PTR(FN_ARG1);
    actor_behavior_ids[actor_idx] = behavior_id;
}

void vm_get_actor_behavior(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UBYTE actor_idx = *(uint8_t *)VM_REF_TO_PTR(FN_ARG0);
	script_memory[*(int16_t*)VM_REF_TO_PTR(FN_ARG1)] = actor_behavior_ids[actor_idx];
}

void vm_set_actor_state(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UBYTE actor_idx = *(uint8_t *)VM_REF_TO_PTR(FN_ARG0);
    UBYTE state_id = *(uint8_t *)VM_REF_TO_PTR(FN_ARG1);
    actor_states[actor_idx] = state_id;
}

void vm_get_actor_state(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UBYTE actor_idx = *(uint8_t *)VM_REF_TO_PTR(FN_ARG0);
	script_memory[*(int16_t*)VM_REF_TO_PTR(FN_ARG1)] = actor_states[actor_idx];
}

void vm_set_actor_velocity_x(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UBYTE actor_idx = *(uint8_t *)VM_REF_TO_PTR(FN_ARG0);
    WORD vel_x = *(int16_t *)VM_REF_TO_PTR(FN_ARG1);
    actor_vel_x[actor_idx] = vel_x;
}

void vm_set_actor_velocity_y(SCRIPT_CTX * THIS) OLDCALL BANKED {
    UBYTE actor_idx = *(uint8_t *)VM_REF_TO_PTR(FN_ARG0);
    WORD vel_y = *(int16_t *)VM_REF_TO_PTR(FN_ARG1);
    actor_vel_y[actor_idx] = vel_y;
}

void vm_set_actor_linked_actor_idx(SCRIPT_CTX * THIS) OLDCALL BANKED {
	UBYTE actor_idx = *(uint8_t *)VM_REF_TO_PTR(FN_ARG0);
    UBYTE linked_actor_idx = *(int16_t *)VM_REF_TO_PTR(FN_ARG1);
    actor_linked_actor_idx[actor_idx] = linked_actor_idx;
}
