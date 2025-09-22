#pragma once
// Safe door helpers that avoid dereferencing null pointers and avoid
// using EXITN before it's defined. Include this early in act.movement.cpp.
//
// This header replaces the fragile OPEN_DOOR / LOCK_DOOR / DOOR_IS_OPENABLE
// macros with function-backed versions that short-circuit correctly.

#include "structs.hpp"
// utils.hpp is usually pulled in by structs.hpp; include defensively anyway.
#include "utils.hpp"

// If the original macros exist, drop them so we can redefine.
#ifdef DOOR_IS_OPENABLE
#undef DOOR_IS_OPENABLE
#endif
#ifdef OPEN_DOOR
#undef OPEN_DOOR
#endif
#ifdef LOCK_DOOR
#undef LOCK_DOOR
#endif

// Forward decls are already in structs.hpp, but keep typical names visible.
struct char_data;
struct obj_data;
struct exit_data;

// Helper: toggle a flag on a room's exit safely.
template <typename RoomLike>
static inline void toggle_room_exit_flag(RoomLike* room, int door, int flag) {
    if (!room || door < 0) return;
    // Many MUD codebases use an array of exit_data* named dir_option.
    // We defensively check the pointer before toggling.
    // Note: we intentionally do NOT bound-check door against MAX_DIR;
    // that macro might not be visible here. We rely on caller passing a sane door.
    if (room->dir_option && room->dir_option[door]) {
        TOGGLE_BIT(room->dir_option[door]->exit_info, flag);
    }
}

// Function-backed implementations.
static inline bool DOOR_IS_OPENABLE_FN(char_data* ch, obj_data* obj, int door) {
    if (obj) {
        // Container path: only read fields when obj != nullptr.
        return (GET_OBJ_TYPE(obj) == ITEM_CONTAINER) && IS_SET(GET_OBJ_VAL(obj, 1), CONT_CLOSEABLE);
    } else {
        // Exit path: use the existing EXIT(ch, door) macro (already available in this codebase).
        auto ex = EXIT(ch, door);
        return ex
            && IS_SET(ex->exit_info, EX_ISDOOR)
            && !IS_SET(ex->exit_info, EX_DESTROYED)
            && !IS_SET(ex->exit_info, EX_HIDDEN);
    }
}

template <typename RoomLike>
static inline void OPEN_DOOR_FN(RoomLike* room, obj_data* obj, int door) {
    if (obj) {
        TOGGLE_BIT(GET_OBJ_VAL(obj, 1), CONT_CLOSED);
    } else {
        toggle_room_exit_flag(room, door, EX_CLOSED);
    }
}

template <typename RoomLike>
static inline void LOCK_DOOR_FN(RoomLike* room, obj_data* obj, int door) {
    if (obj) {
        TOGGLE_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED);
    } else {
        toggle_room_exit_flag(room, door, EX_LOCKED);
    }
}

// Public-facing macros that now call the safe functions.
// These preserve the original call sites unchanged.
#define DOOR_IS_OPENABLE(ch, obj, door) (DOOR_IS_OPENABLE_FN((ch), (obj), (door)))
#define OPEN_DOOR(room, obj, door)      (OPEN_DOOR_FN((room), (obj), (door)))
#define LOCK_DOOR(room, obj, door)      (LOCK_DOOR_FN((room), (obj), (door)))
