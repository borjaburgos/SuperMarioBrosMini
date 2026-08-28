#ifndef ACTOR_BEHAVIOR_H
#define ACTOR_BEHAVIOR_H

#define BEHAVIOR_ACTIVATION_THRESHOLD 168
#define BEHAVIOR_DEACTIVATION_THRESHOLD 176
#define BEHAVIOR_DEACTIVATION_LOWER_THRESHOLD -8

#include <gbdk/platform.h>
#include "actor.h"
#include "macro.h"

// Actor behavior speeds are stored in the plug-in's former 1/16 px units.
#ifndef LEGACY_DELTA_TO_SUBPX
#define LEGACY_DELTA_TO_SUBPX(v) ((v) << 1)
#endif

void actor_behavior_init(void) BANKED;
void actor_behavior_update(void) BANKED;
void actor_behavior_update_a1(UBYTE actor_idx, actor_t *actor) BANKED;
void actor_behavior_update_a2(UBYTE actor_idx, actor_t *actor) BANKED;

UWORD check_collision(UWORD start_x, UWORD start_y, rect16_t *bounds, col_check_dir_e check_dir) BANKED;
UWORD check_pit(UWORD start_x, UWORD start_y, rect16_t *bounds, col_check_dir_e check_dir) BANKED;
void apply_gravity(UBYTE actor_idx) BANKED;
void apply_velocity(UBYTE actor_idx, actor_t *actor) BANKED;
void apply_velocity_avoid_fall(UBYTE actor_idx, actor_t *actor) BANKED;

extern UBYTE actor_behavior_ids[MAX_ACTORS];
extern UBYTE actor_states[MAX_ACTORS];
extern WORD actor_vel_x[MAX_ACTORS];
extern WORD actor_vel_y[MAX_ACTORS];
extern UBYTE actor_counter_a[MAX_ACTORS];
extern UBYTE actor_counter_b[MAX_ACTORS];
extern UBYTE actor_linked_actor_idx[MAX_ACTORS];

extern UBYTE current_behavior;
extern WORD current_actor_x;
extern UWORD new_actor_x;
extern UWORD new_actor_y;
extern WORD col_tx;
extern WORD col_ty;
extern upoint16_t tmp_point;

extern const BYTE firebar_incx_lookup[];
extern const BYTE firebar_incy_lookup[];
extern const BYTE spring_bb_top_lookup[];

#endif
