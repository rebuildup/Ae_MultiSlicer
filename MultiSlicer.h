/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */

#define MULTISLICER_WIDTH_MIN         0
#define MULTISLICER_WIDTH_MAX         100
#define MULTISLICER_WIDTH_DFLT        100

#define MULTISLICER_SLICES_MIN        2
#define MULTISLICER_SLICES_MAX        100
#define MULTISLICER_SLICES_DFLT       10

#define MULTISLICER_SEED_MIN          0
#define MULTISLICER_SEED_MAX          10000
#define MULTISLICER_SEED_DFLT         1234

// Macro for math operations
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(x) ((x) < 0 ? -(x) : (x))

enum {
    MULTISLICER_INPUT = 0,
    MULTISLICER_ANGLE,
    MULTISLICER_SHIFT,
    MULTISLICER_WIDTH,
    MULTISLICER_SLICES,
    MULTISLICER_SEED,
    MULTISLICER_NUM_PARAMS
};

enum {
    ANGLE_DISK_ID = 1,
}

#endif // MultiSlicer_H
