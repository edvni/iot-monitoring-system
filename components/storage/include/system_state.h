#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

// System states enum
typedef enum {
    STATE_NORMAL = 0,
    STATE_FIRST_BLOCK_RECOVERY = 1,
    STATE_SECOND_BLOCK_RECOVERY = 2, 
    STATE_THIRD_BLOCK_RECOVERY = 3
} system_state_t;

#endif // SYSTEM_STATE_H