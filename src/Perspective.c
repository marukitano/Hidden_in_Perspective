#include <pebble.h>

// Pebble Time 2 (Emery) watchface with perspective date, battery and optional Swiss emblem.

#define SIZE 76
#define OFFSET 9
#define ZPOS -38
// Main geometry and virtual camera
#define EYEZ 480

#define EMERY_WIDTH 200
#define EMERY_HEIGHT 228
#define EMERY_CENTER_X 100
#define EMERY_CENTER_Y 114

// Projection safety limits
#define NEAR_CLIP_DISTANCE 40
#define PROJECTION_MARGIN 4
// Q7 fixed-point precision keeps the rotated geometry in range.
#define SHIFT 7
#define FIXED_ONE (1 << SHIFT)
#define PERSPECTIVE_PIVOT_Y -16
#define PERSPECTIVE_PIVOT_Z ZPOS

// Folded date and battery planes
#define AUX_DATE_DOT_SIZE 2
#define AUX_BATTERY_DOT_SIZE 2
#define AUX_DATE_X_DOT_STEP 5
#define AUX_BATTERY_X_DOT_STEP 2
#define AUX_TOP_DEPTH_STEP 4
#define AUX_BOTTOM_DEPTH_STEP 2
#define AUX_DATE_EMBLEM_WIDTH 13
#define AUX_DATE_EMBLEM_HEIGHT 14
#define AUX_DATE_EMBLEM_X_STEP 1
#define AUX_DATE_EMBLEM_DEPTH_STEP 1
#define AUX_DATE_EMBLEM_DEPTH_OFFSET 3
#define AUX_DATE_EMBLEM_DOT_SIZE 1
#define AUX_TOP_HINGE_Y -178
#define AUX_BOTTOM_HINGE_Y 186
#define AUX_TOP_DEPTH_DIRECTION -1
#define AUX_BOTTOM_DEPTH_DIRECTION 1
#define AUX_TOP_DEPTH_OFFSET 28

// Motion sampling and filtering
#define SENSOR_RATE ACCEL_SAMPLING_50HZ
#define FRAME_INTERVAL_MS 33
// Shake-controlled color theme
// Detection uses changes between consecutive 50 Hz samples.
#define SHAKE_DELTA_THRESHOLD 1800
#define SHAKE_COOLDOWN_SAMPLES 50
#define ANGLE_DEADZONE (TRIG_MAX_ANGLE / 240)
// Visual tilt response
#define VISUAL_TILT_GAIN 2
#define VISUAL_TILT_LIMIT ((TRIG_MAX_ANGLE_BY_4 * 77) / 90)
#define CONFIG_KEY_INVERTED_THEME 3335
#define CONFIG_KEY_SHOW_SWISS_EMBLEM 3336
#define AUX_DATE_SEPARATOR_DOT_SIZE 3

static const int32_t TRIG_MAX_ANGLE_BY_4 = TRIG_MAX_ANGLE / 4;

static Window *window;
static Layer *layer;
static AppTimer *timer;
static AccelData latestAccel;
static bool haveAccel = false;
static int16_t smoothedAccelX = 0;
static int16_t smoothedAccelY = 0;
static int16_t smoothedAccelZ = 0;
static bool smoothedAccelInitialized = false;
static bool orientationInitialized = false;
static bool invertedTheme = false;
static bool showSwissEmblem = true;
static AccelData previousShakeAccel;
static bool havePreviousShakeAccel = false;
static uint8_t shakeCooldownSamples = 0;
static int32_t a=0, b=0, c=0;
static int32_t cosa, sina, cosb, sinb, cosc, sinc;
static int d[6];
static int digitOffsetX[6], digitOffsetY[6];

static int dateDigits[4] = { 0, 1, 0, 1 };
static int batteryPercent = 0;
static GRect fullScreenRect = GRect(0, 0, EMERY_WIDTH, EMERY_HEIGHT);

static const uint16_t __ACOS[1025] = {
  32768, 32115, 31845, 31638, 31463, 31309, 31169, 31041, 30921, 30809, 30703, 30602, 30505, 30412, 30323, 30237,
  30153, 30072, 29994, 29917, 29843, 29770, 29699, 29629, 29561, 29495, 29429, 29365, 29302, 29240, 29179, 29119,
  29060, 29002, 28945, 28889, 28833, 28778, 28724, 28670, 28617, 28565, 28513, 28462, 28412, 28362, 28312, 28263,
  28215, 28167, 28120, 28072, 28026, 27980, 27934, 27889, 27844, 27799, 27755, 27711, 27667, 27624, 27581, 27539,
  27496, 27454, 27413, 27372, 27330, 27290, 27249, 27209, 27169, 27129, 27090, 27051, 27012, 26973, 26934, 26896,
  26858, 26820, 26783, 26745, 26708, 26671, 26634, 26597, 26561, 26525, 26489, 26453, 26417, 26382, 26346, 26311,
  26276, 26241, 26206, 26172, 26137, 26103, 26069, 26035, 26001, 25968, 25934, 25901, 25868, 25835, 25802, 25769,
  25736, 25703, 25671, 25639, 25607, 25574, 25542, 25511, 25479, 25447, 25416, 25384, 25353, 25322, 25291, 25260,
  25229, 25198, 25168, 25137, 25107, 25076, 25046, 25016, 24986, 24956, 24926, 24896, 24867, 24837, 24807, 24778,
  24749, 24719, 24690, 24661, 24632, 24603, 24574, 24546, 24517, 24488, 24460, 24431, 24403, 24375, 24346, 24318,
  24290, 24262, 24234, 24206, 24179, 24151, 24123, 24095, 24068, 24040, 24013, 23986, 23958, 23931, 23904, 23877,
  23850, 23823, 23796, 23769, 23742, 23716, 23689, 23662, 23636, 23609, 23583, 23557, 23530, 23504, 23478, 23452,
  23425, 23399, 23373, 23347, 23321, 23296, 23270, 23244, 23218, 23193, 23167, 23141, 23116, 23090, 23065, 23040,
  23014, 22989, 22964, 22938, 22913, 22888, 22863, 22838, 22813, 22788, 22763, 22738, 22714, 22689, 22664, 22639,
  22615, 22590, 22565, 22541, 22516, 22492, 22468, 22443, 22419, 22394, 22370, 22346, 22322, 22298, 22273, 22249,
  22225, 22201, 22177, 22153, 22129, 22105, 22082, 22058, 22034, 22010, 21987, 21963, 21939, 21916, 21892, 21868,
  21845, 21821, 21798, 21774, 21751, 21728, 21704, 21681, 21658, 21634, 21611, 21588, 21565, 21542, 21518, 21495,
  21472, 21449, 21426, 21403, 21380, 21357, 21334, 21311, 21289, 21266, 21243, 21220, 21197, 21175, 21152, 21129,
  21107, 21084, 21061, 21039, 21016, 20994, 20971, 20949, 20926, 20904, 20881, 20859, 20836, 20814, 20792, 20769,
  20747, 20725, 20702, 20680, 20658, 20636, 20614, 20591, 20569, 20547, 20525, 20503, 20481, 20459, 20437, 20415,
  20393, 20371, 20349, 20327, 20305, 20283, 20261, 20240, 20218, 20196, 20174, 20152, 20131, 20109, 20087, 20065,
  20044, 20022, 20000, 19979, 19957, 19935, 19914, 19892, 19871, 19849, 19827, 19806, 19784, 19763, 19741, 19720,
  19699, 19677, 19656, 19634, 19613, 19591, 19570, 19549, 19527, 19506, 19485, 19463, 19442, 19421, 19400, 19378,
  19357, 19336, 19315, 19294, 19272, 19251, 19230, 19209, 19188, 19167, 19145, 19124, 19103, 19082, 19061, 19040,
  19019, 18998, 18977, 18956, 18935, 18914, 18893, 18872, 18851, 18830, 18809, 18788, 18767, 18746, 18726, 18705,
  18684, 18663, 18642, 18621, 18600, 18579, 18559, 18538, 18517, 18496, 18475, 18455, 18434, 18413, 18392, 18372,
  18351, 18330, 18309, 18289, 18268, 18247, 18227, 18206, 18185, 18164, 18144, 18123, 18103, 18082, 18061, 18041,
  18020, 17999, 17979, 17958, 17938, 17917, 17896, 17876, 17855, 17835, 17814, 17793, 17773, 17752, 17732, 17711,
  17691, 17670, 17650, 17629, 17609, 17588, 17568, 17547, 17527, 17506, 17486, 17465, 17445, 17424, 17404, 17383,
  17363, 17342, 17322, 17301, 17281, 17261, 17240, 17220, 17199, 17179, 17158, 17138, 17117, 17097, 17077, 17056,
  17036, 17015, 16995, 16975, 16954, 16934, 16913, 16893, 16873, 16852, 16832, 16811, 16791, 16771, 16750, 16730,
  16710, 16689, 16669, 16648, 16628, 16608, 16587, 16567, 16546, 16526, 16506, 16485, 16465, 16445, 16424, 16404,
  16384, 16363, 16343, 16322, 16302, 16282, 16261, 16241, 16221, 16200, 16180, 16159, 16139, 16119, 16098, 16078,
  16057, 16037, 16017, 15996, 15976, 15956, 15935, 15915, 15894, 15874, 15854, 15833, 15813, 15792, 15772, 15752,
  15731, 15711, 15690, 15670, 15650, 15629, 15609, 15588, 15568, 15547, 15527, 15506, 15486, 15466, 15445, 15425,
  15404, 15384, 15363, 15343, 15322, 15302, 15281, 15261, 15240, 15220, 15199, 15179, 15158, 15138, 15117, 15097,
  15076, 15056, 15035, 15015, 14994, 14974, 14953, 14932, 14912, 14891, 14871, 14850, 14829, 14809, 14788, 14768,
  14747, 14726, 14706, 14685, 14664, 14644, 14623, 14603, 14582, 14561, 14540, 14520, 14499, 14478, 14458, 14437,
  14416, 14395, 14375, 14354, 14333, 14312, 14292, 14271, 14250, 14229, 14208, 14188, 14167, 14146, 14125, 14104,
  14083, 14062, 14041, 14021, 14000, 13979, 13958, 13937, 13916, 13895, 13874, 13853, 13832, 13811, 13790, 13769,
  13748, 13727, 13706, 13685, 13664, 13643, 13622, 13600, 13579, 13558, 13537, 13516, 13495, 13473, 13452, 13431,
  13410, 13389, 13367, 13346, 13325, 13304, 13282, 13261, 13240, 13218, 13197, 13176, 13154, 13133, 13111, 13090,
  13068, 13047, 13026, 13004, 12983, 12961, 12940, 12918, 12896, 12875, 12853, 12832, 12810, 12788, 12767, 12745,
  12723, 12702, 12680, 12658, 12636, 12615, 12593, 12571, 12549, 12527, 12506, 12484, 12462, 12440, 12418, 12396,
  12374, 12352, 12330, 12308, 12286, 12264, 12242, 12220, 12198, 12176, 12153, 12131, 12109, 12087, 12065, 12042,
  12020, 11998, 11975, 11953, 11931, 11908, 11886, 11863, 11841, 11818, 11796, 11773, 11751, 11728, 11706, 11683,
  11660, 11638, 11615, 11592, 11570, 11547, 11524, 11501, 11478, 11456, 11433, 11410, 11387, 11364, 11341, 11318,
  11295, 11272, 11249, 11225, 11202, 11179, 11156, 11133, 11109, 11086, 11063, 11039, 11016, 10993, 10969, 10946,
  10922, 10899, 10875, 10851, 10828, 10804, 10780, 10757, 10733, 10709, 10685, 10662, 10638, 10614, 10590, 10566,
  10542, 10518, 10494, 10469, 10445, 10421, 10397, 10373, 10348, 10324, 10299, 10275, 10251, 10226, 10202, 10177,
  10152, 10128, 10103, 10078, 10053, 10029, 10004, 9979, 9954, 9929, 9904, 9879, 9854, 9829, 9803, 9778,
  9753, 9727, 9702, 9677, 9651, 9626, 9600, 9574, 9549, 9523, 9497, 9471, 9446, 9420, 9394, 9368,
  9342, 9315, 9289, 9263, 9237, 9210, 9184, 9158, 9131, 9105, 9078, 9051, 9025, 8998, 8971, 8944,
  8917, 8890, 8863, 8836, 8809, 8781, 8754, 8727, 8699, 8672, 8644, 8616, 8588, 8561, 8533, 8505,
  8477, 8449, 8421, 8392, 8364, 8336, 8307, 8279, 8250, 8221, 8193, 8164, 8135, 8106, 8077, 8048,
  8018, 7989, 7960, 7930, 7900, 7871, 7841, 7811, 7781, 7751, 7721, 7691, 7660, 7630, 7599, 7569,
  7538, 7507, 7476, 7445, 7414, 7383, 7351, 7320, 7288, 7256, 7225, 7193, 7160, 7128, 7096, 7064,
  7031, 6998, 6965, 6932, 6899, 6866, 6833, 6799, 6766, 6732, 6698, 6664, 6630, 6595, 6561, 6526,
  6491, 6456, 6421, 6385, 6350, 6314, 6278, 6242, 6206, 6170, 6133, 6096, 6059, 6022, 5984, 5947,
  5909, 5871, 5833, 5794, 5755, 5716, 5677, 5638, 5598, 5558, 5518, 5477, 5437, 5395, 5354, 5313,
  5271, 5228, 5186, 5143, 5100, 5056, 5012, 4968, 4923, 4878, 4833, 4787, 4741, 4695, 4647, 4600,
  4552, 4504, 4455, 4405, 4355, 4305, 4254, 4202, 4150, 4097, 4043, 3989, 3934, 3878, 3822, 3765,
  3707, 3648, 3588, 3527, 3465, 3402, 3338, 3272, 3206, 3138, 3068, 2997, 2924, 2850, 2773, 2695,
  2614, 2530, 2444, 2355, 2262, 2165, 2064, 1958, 1846, 1726, 1598, 1458, 1304, 1129, 922, 652, 0
};

typedef struct {
  int32_t x;
  int32_t y;
  int32_t z;
} GPoint3;

static const uint8_t digit[810] = {	 // 10 x 9 x 9
                     // 0
  0,0,1,1,1,1,1,0,0,
  0,1,1,1,1,1,1,1,0,
  1,1,1,0,0,0,1,1,1,
  1,1,0,0,0,0,0,1,1,
  1,1,0,0,0,0,0,1,1,
  1,1,0,0,0,0,0,1,1,
  1,1,1,0,0,0,1,1,1,
  0,1,1,1,1,1,1,1,0,
  0,0,1,1,1,1,1,0,0,

  // 1
  0,0,0,1,1,1,0,0,0,
  0,0,1,1,1,1,0,0,0,
  0,1,1,0,1,1,0,0,0,
  0,0,0,0,1,1,0,0,0,
  0,0,0,0,1,1,0,0,0,
  0,0,0,0,1,1,0,0,0,
  0,0,0,0,1,1,0,0,0,
  0,1,1,1,1,1,1,1,1,
  0,1,1,1,1,1,1,1,1,

  // 2
  0,0,1,1,1,1,1,0,0,
  0,1,1,1,1,1,1,1,0,
  1,1,1,0,0,0,1,1,1,
  1,1,0,0,0,0,1,1,1,
  0,0,0,0,0,1,1,1,0,
  0,0,0,1,1,1,0,0,0,
  0,1,1,1,0,0,0,0,0,
  1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,

  // 3
  0,0,1,1,1,1,1,0,0,
  0,1,1,1,1,1,1,1,0,
  1,1,1,0,0,0,1,1,1,
  1,1,0,0,0,0,1,1,1,
  0,0,0,0,1,1,1,1,0,
  1,1,0,0,0,0,1,1,1,
  1,1,1,0,0,0,1,1,1,
  0,1,1,1,1,1,1,1,0,
  0,0,1,1,1,1,1,0,0,

  // 4
  0,0,0,0,1,1,1,0,0,
  0,0,0,1,1,1,1,0,0,
  0,0,1,1,1,1,1,0,0,
  0,1,1,1,0,1,1,0,0,
  1,1,1,0,0,1,1,0,0,
  1,1,1,1,1,1,1,1,0,
  1,1,1,1,1,1,1,1,0,
  0,0,0,0,0,1,1,0,0,
  0,0,0,0,0,1,1,0,0,

  // 5
  1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,
  1,1,0,0,0,0,0,0,0,
  1,1,1,1,1,1,1,1,0,
  1,1,1,1,1,1,1,1,1,
  0,0,0,0,0,0,0,1,1,
  1,1,0,0,0,0,0,1,1,
  1,1,1,1,1,1,1,1,1,
  0,1,1,1,1,1,1,1,0,

  // 6
  0,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,
  1,1,0,0,0,0,0,0,0,
  1,1,1,1,1,1,1,1,0,
  1,1,1,1,1,1,1,1,1,
  1,1,0,0,0,0,0,1,1,
  1,1,0,0,0,0,0,1,1,
  1,1,1,1,1,1,1,1,1,
  0,1,1,1,1,1,1,1,0,

  // 7
  1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,
  0,0,0,0,0,0,0,1,1,
  0,0,0,0,0,0,1,1,0,
  0,0,0,0,0,1,1,0,0,
  0,0,0,0,1,1,0,0,0,
  0,0,0,1,1,0,0,0,0,
  0,0,0,1,1,0,0,0,0,
  0,0,0,1,1,0,0,0,0,

  // 8
  0,1,1,1,1,1,1,1,0,
  1,1,1,1,1,1,1,1,1,
  1,1,0,0,0,0,0,1,1,
  1,1,1,1,1,1,1,1,1,
  0,1,1,1,1,1,1,1,0,
  1,1,0,0,0,0,0,1,1,
  1,1,0,0,0,0,0,1,1,
  1,1,1,1,1,1,1,1,1,
  0,1,1,1,1,1,1,1,0,

  // 9
  0,1,1,1,1,1,1,1,0,
  1,1,1,1,1,1,1,1,1,
  1,1,0,0,0,0,0,1,1,
  1,1,0,0,0,0,0,1,1,
  0,1,1,1,1,1,1,1,1,
  0,1,1,1,1,1,1,1,1,
  0,0,0,0,0,0,0,1,1,
  1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,0
};

#define DIGIT(num, x, y) (digit[(81*(num))+((y)*9)+(x)])

static const GPoint3 eye = { 0, 0, EYEZ };

static GPoint3 pointList[810]; // 10 * 9 * 9
static int numPoints[10];
#define DIGIT_OFFSET(num) (&pointList[81*(num)])

static inline uint16_t squareRoot(uint16_t x) {
  uint16_t a, b;

  b = x;
  a = x = 0x3f;
  x = b/x;
  a = x = (x+a)>>1;
  x = b/x;
  a = x = (x+a)>>1;
  x = b/x;
  x = (x+a)>>1;

  return x;
}

static inline int32_t myArccos(int16_t x) {
  if (x < -512) x = -512;
  if (x > 512) x = 512;

  return __ACOS[512+x];
}

static inline int32_t length(const GPoint3 *v) {
  return squareRoot(v->x*v->x + v->y*v->y + v->z*v->z);
}

static void angles(
    const GPoint3 *vector,
    int32_t *angleX,
    int32_t *angleY,
    int32_t *angleZ) {
  const int32_t magnitude = length(vector);

  if (magnitude <= 0) {
    *angleX = 0;
    *angleY = 0;
    *angleZ = 0;
    return;
  }

  const int16_t normalizedY =
      (int16_t)(
          ((int32_t)vector->y * 512)
          / magnitude);

  const int16_t normalizedX =
      (int16_t)(
          ((int32_t)vector->x * 512)
          / magnitude);

  *angleX =
      myArccos(normalizedY)
      - TRIG_MAX_ANGLE_BY_4;

  *angleY =
      myArccos(normalizedX)
      - TRIG_MAX_ANGLE_BY_4;

  *angleZ = 0;
}

static int32_t applyVisualTiltGain(
    int32_t angle) {
  int32_t amplified =
      angle * VISUAL_TILT_GAIN;

  if (amplified > VISUAL_TILT_LIMIT) {
    return VISUAL_TILT_LIMIT;
  }

  if (amplified < -VISUAL_TILT_LIMIT) {
    return -VISUAL_TILT_LIMIT;
  }

  return amplified;
}

static int32_t applyAngleDeadzone(
    int32_t current,
    int32_t target,
    bool *changed) {
  const int32_t difference = target - current;

  if (difference > ANGLE_DEADZONE) {
    *changed = true;
    return target - ANGLE_DEADZONE;
  }

  if (difference < -ANGLE_DEADZONE) {
    *changed = true;
    return target + ANGLE_DEADZONE;
  }

  return current;
}

static inline void transformPoint(
    const GPoint3 *P,
    GPoint3 *T) {
  GPoint3 U;

  // Move the complete geometry to the time-plane center before rotation.
  // Multiplication is defined for negative coordinates; signed left
  // shifts are not.
  U.x = P->x * FIXED_ONE;
  U.y =
      (P->y - PERSPECTIVE_PIVOT_Y)
      * FIXED_ONE;
  U.z =
      (P->z - PERSPECTIVE_PIVOT_Z)
      * FIXED_ONE;

  T->x = U.x;
  T->y =
      (U.y * cosa + U.z * sina)
      / TRIG_MAX_RATIO;
  T->z =
      (U.z * cosa - U.y * sina)
      / TRIG_MAX_RATIO;

  U.x =
      (T->x * cosb - T->z * sinb)
      / TRIG_MAX_RATIO;
  U.y = T->y;
  U.z =
      (T->x * sinb + T->z * cosb)
      / TRIG_MAX_RATIO;

  const int32_t rotatedX =
      (U.x * cosc + U.y * sinc)
      / TRIG_MAX_RATIO;

  const int32_t rotatedY =
      (U.y * cosc - U.x * sinc)
      / TRIG_MAX_RATIO;

  // Move the geometry back after rotation.
  T->x = rotatedX >> SHIFT;
  T->y =
      (rotatedY >> SHIFT)
      + PERSPECTIVE_PIVOT_Y;
  T->z =
      (U.z >> SHIFT)
      + PERSPECTIVE_PIVOT_Z;
}

static inline bool projectPoint(
    const GPoint3 *T,
    GPoint *S) {
  const int32_t denominator =
      (int32_t)eye.z + T->z;

  // A zero, negative or very small denominator means that the point is
  // behind or extremely close to the virtual camera plane. Projecting it
  // would create huge wrapped coordinates and visible streaks.
  if (denominator <= NEAR_CLIP_DISTANCE) {
    return false;
  }

  const int32_t projectedX =
      ((int32_t)eye.z * T->x) / denominator
      + EMERY_CENTER_X;

  const int32_t projectedY =
      ((int32_t)eye.z * T->y) / denominator
      + EMERY_CENTER_Y;

  // Reject off-screen points before narrowing the coordinates to int16_t.
  // This prevents integer wrapping while retaining points entering from
  // the edge during the hidden-perspective movement.
  if (projectedX < -PROJECTION_MARGIN
      || projectedX >= EMERY_WIDTH + PROJECTION_MARGIN
      || projectedY < -PROJECTION_MARGIN
      || projectedY >= EMERY_HEIGHT + PROJECTION_MARGIN) {
    return false;
  }

  S->x = (int16_t)projectedX;
  S->y = (int16_t)projectedY;
  return true;
}

static void drawPoint(GContext *ctx, const GPoint3 *P) {
  GPoint S;
  GPoint3 T;

  transformPoint(P, &T);

  if (!projectPoint(&T, &S)) {
    return;
  }

  graphics_fill_rect(ctx, GRect(S.x-1, S.y-1, 4, 4), 0, GCornerNone);
}

static int32_t divideRoundedSigned64(
    int64_t numerator,
    int32_t denominator) {
  if (numerator >= 0) {
    return (int32_t)(
        (numerator + denominator / 2)
        / denominator);
  }

  return (int32_t)(
      (numerator - denominator / 2)
      / denominator);
}

static bool projectAuxPointHighPrecision(
    const GPoint3 *point,
    GPoint *screenPoint) {
  // Keep every rotated coordinate in Q7 fixed-point form.
  const int32_t xQ =
      ((int32_t)point->x) * FIXED_ONE;

  const int32_t yQ =
      ((int32_t)point->y
          - PERSPECTIVE_PIVOT_Y)
      * FIXED_ONE;

  const int32_t zQ =
      ((int32_t)point->z
          - PERSPECTIVE_PIVOT_Z)
      * FIXED_ONE;

  const int32_t xRotationYQ =
      (int32_t)(
          ((int64_t)yQ * cosa
              + (int64_t)zQ * sina)
          / TRIG_MAX_RATIO);

  const int32_t xRotationZQ =
      (int32_t)(
          ((int64_t)zQ * cosa
              - (int64_t)yQ * sina)
          / TRIG_MAX_RATIO);

  const int32_t yRotationXQ =
      (int32_t)(
          ((int64_t)xQ * cosb
              - (int64_t)xRotationZQ * sinb)
          / TRIG_MAX_RATIO);

  const int32_t yRotationZQ =
      (int32_t)(
          ((int64_t)xQ * sinb
              + (int64_t)xRotationZQ * cosb)
          / TRIG_MAX_RATIO);

  const int32_t rotatedXQ =
      (int32_t)(
          ((int64_t)yRotationXQ * cosc
              + (int64_t)xRotationYQ * sinc)
          / TRIG_MAX_RATIO);

  const int32_t rotatedYQ =
      (int32_t)(
          ((int64_t)xRotationYQ * cosc
              - (int64_t)yRotationXQ * sinc)
          / TRIG_MAX_RATIO);

  const int32_t worldYQ =
      rotatedYQ
      + ((int32_t)PERSPECTIVE_PIVOT_Y
          * FIXED_ONE);

  const int32_t worldZQ =
      yRotationZQ
      + ((int32_t)PERSPECTIVE_PIVOT_Z
          * FIXED_ONE);

  const int32_t denominatorQ =
      ((int32_t)eye.z * FIXED_ONE)
      + worldZQ;

  if (denominatorQ
      <= (NEAR_CLIP_DISTANCE * FIXED_ONE)) {
    return false;
  }

  // Round only here, when converting the final projection to display pixels.
  const int32_t projectedX =
      divideRoundedSigned64(
          (int64_t)eye.z * rotatedXQ,
          denominatorQ)
      + EMERY_CENTER_X;

  const int32_t projectedY =
      divideRoundedSigned64(
          (int64_t)eye.z * worldYQ,
          denominatorQ)
      + EMERY_CENTER_Y;

  if (projectedX < -PROJECTION_MARGIN
      || projectedX
          >= EMERY_WIDTH + PROJECTION_MARGIN
      || projectedY < -PROJECTION_MARGIN
      || projectedY
          >= EMERY_HEIGHT + PROJECTION_MARGIN) {
    return false;
  }

  screenPoint->x = (int16_t)projectedX;
  screenPoint->y = (int16_t)projectedY;
  return true;
}

static void drawAuxProjectedPoint(
    GContext *ctx,
    const GPoint3 *point,
    int16_t dotSize) {
  GPoint screenPoint;

  if (!projectAuxPointHighPrecision(
          point,
          &screenPoint)) {
    return;
  }

  graphics_fill_rect(
      ctx,
      GRect(
          screenPoint.x - dotSize / 2,
          screenPoint.y - dotSize / 2,
          dotSize,
          dotSize),
      0,
      GCornerNone);
}

static void drawFoldedCellSized(
    GContext *ctx,
    int16_t x,
    int16_t row,
    bool topPlane,
    int16_t dotSize) {
  GPoint3 point;

  point.x = x;
  point.y = topPlane
      ? AUX_TOP_HINGE_Y
      : AUX_BOTTOM_HINGE_Y;

  const int16_t depth =
      topPlane
      ? row * AUX_TOP_DEPTH_STEP
          + AUX_TOP_DEPTH_OFFSET
      : row * AUX_BOTTOM_DEPTH_STEP;

  point.z = ZPOS
      + (topPlane
          ? AUX_TOP_DEPTH_DIRECTION * depth
          : AUX_BOTTOM_DEPTH_DIRECTION * depth);

  drawAuxProjectedPoint(
      ctx,
      &point,
      dotSize);
}

static void drawFoldedCell(
    GContext *ctx,
    int16_t x,
    int16_t row,
    bool topPlane) {
  drawFoldedCellSized(
      ctx,
      x,
      row,
      topPlane,
      topPlane
          ? AUX_DATE_DOT_SIZE
          : AUX_BATTERY_DOT_SIZE);
}

static void drawFoldedDigit(
    GContext *ctx,
    int digitValue,
    int16_t centerX,
    bool topPlane,
    bool reverseRows) {
  const int16_t xStep =
      topPlane
      ? AUX_DATE_X_DOT_STEP
      : AUX_BATTERY_X_DOT_STEP;

  for (int16_t row = 0; row < 9; ++row) {
    const int16_t sourceRow =
        reverseRows ? 8 - row : row;

    for (int16_t column = 0;
         column < 9;
         ++column) {
      if (!DIGIT(
              digitValue,
              column,
              sourceRow)) {
        continue;
      }

      const int16_t x =
          centerX
          + (column - 4) * xStep;

      drawFoldedCell(
          ctx,
          x,
          row,
          topPlane);
    }
  }
}

static bool projectDateEmblemCell(
    int16_t column,
    int16_t planeRow,
    GPoint *screenPoint) {
  const int16_t x =
      (column - AUX_DATE_EMBLEM_WIDTH / 2)
      * AUX_DATE_EMBLEM_X_STEP;

  const int16_t depth =
      AUX_TOP_DEPTH_OFFSET
      + AUX_DATE_EMBLEM_DEPTH_OFFSET
      + planeRow
          * AUX_DATE_EMBLEM_DEPTH_STEP;

  const GPoint3 point = {
    x,
    AUX_TOP_HINGE_Y,
    ZPOS
        + AUX_TOP_DEPTH_DIRECTION
            * depth
  };

  return projectAuxPointHighPrecision(
      &point,
      screenPoint);
}

static GPoint s_dateEmblemStepX = {
  1,
  0
};

static GPoint s_dateEmblemStepDepth = {
  0,
  -1
};

static bool s_dateEmblemLatticeInitialized = false;
static bool s_dateEmblemReverseRows = true;

static bool quantizeDateEmblemAxis(
    int16_t deltaX,
    int16_t deltaY,
    GPoint *step) {
  const int16_t absoluteX =
      deltaX < 0
          ? -deltaX
          : deltaX;

  const int16_t absoluteY =
      deltaY < 0
          ? -deltaY
          : deltaY;

  if (absoluteX == 0
      && absoluteY == 0) {
    return false;
  }

  const int16_t signX =
      deltaX < 0
          ? -1
          : deltaX > 0
              ? 1
              : 0;

  const int16_t signY =
      deltaY < 0
          ? -1
          : deltaY > 0
              ? 1
              : 0;

  if (absoluteX >= absoluteY * 2) {
    step->x = signX;
    step->y = 0;
  } else if (absoluteY >= absoluteX * 2) {
    step->x = 0;
    step->y = signY;
  } else {
    step->x = signX;
    step->y = signY;
  }

  return true;
}

static bool getDateEmblemLattice(
    GPoint *origin,
    GPoint *stepX,
    GPoint *stepDepth,
    bool *reverseRows) {
  GPoint firstCell;
  GPoint lastColumnCell;
  GPoint lastDepthCell;

  if (!projectDateEmblemCell(
          0,
          0,
          &firstCell)
      || !projectDateEmblemCell(
          AUX_DATE_EMBLEM_WIDTH - 1,
          0,
          &lastColumnCell)
      || !projectDateEmblemCell(
          0,
          AUX_DATE_EMBLEM_HEIGHT - 1,
          &lastDepthCell)) {
    return false;
  }

  const int16_t totalXx =
      lastColumnCell.x - firstCell.x;

  const int16_t totalXy =
      lastColumnCell.y - firstCell.y;

  const int16_t totalDepthX =
      lastDepthCell.x - firstCell.x;

  const int16_t totalDepthY =
      lastDepthCell.y - firstCell.y;

  GPoint candidateStepX;
  GPoint candidateStepDepth;

  const bool haveStepX =
      quantizeDateEmblemAxis(
          totalXx,
          totalXy,
          &candidateStepX);

  const bool haveStepDepth =
      quantizeDateEmblemAxis(
          totalDepthX,
          totalDepthY,
          &candidateStepDepth);

  if (haveStepX) {
    s_dateEmblemStepX =
        candidateStepX;
  }

  if (haveStepDepth) {
    s_dateEmblemStepDepth =
        candidateStepDepth;
  }

  /*
   * A quantized pair must span an area. If both axes collapse onto the
   * same pixel direction, choose the perpendicular direction that best
   * agrees with the full projected depth vector.
   */
  const int16_t cross =
      s_dateEmblemStepX.x
          * s_dateEmblemStepDepth.y
      - s_dateEmblemStepX.y
          * s_dateEmblemStepDepth.x;

  if (cross == 0) {
    const GPoint candidateA = {
      -s_dateEmblemStepX.y,
      s_dateEmblemStepX.x
    };

    const GPoint candidateB = {
      s_dateEmblemStepX.y,
      -s_dateEmblemStepX.x
    };

    const int32_t dotA =
        (int32_t)candidateA.x
            * totalDepthX
        + (int32_t)candidateA.y
            * totalDepthY;

    const int32_t dotB =
        (int32_t)candidateB.x
            * totalDepthX
        + (int32_t)candidateB.y
            * totalDepthY;

    s_dateEmblemStepDepth =
        dotA >= dotB
            ? candidateA
            : candidateB;
  }

  /*
   * Do not change texture orientation while the plane is almost edge-on.
   * Only switch row order after a clear two-pixel vertical displacement.
   */
  if (!s_dateEmblemLatticeInitialized) {
    s_dateEmblemReverseRows =
        totalDepthY <= 0;

    s_dateEmblemLatticeInitialized =
        true;
  } else if (totalDepthY <= -2) {
    s_dateEmblemReverseRows =
        true;
  } else if (totalDepthY >= 2) {
    s_dateEmblemReverseRows =
        false;
  }

  *origin =
      firstCell;

  *stepX =
      s_dateEmblemStepX;

  *stepDepth =
      s_dateEmblemStepDepth;

  *reverseRows =
      s_dateEmblemReverseRows;

  return true;
}

static void drawDateSeparator(
    GContext *ctx) {
  if (!showSwissEmblem) {
    /*
     * Restore the original DD.MM separator:
     * one point at row zero of the folded date plane.
     */
    drawFoldedCellSized(
        ctx,
        0,
        0,
        true,
        AUX_DATE_SEPARATOR_DOT_SIZE);

    return;
  }
  static const char *const SWISS_EMBLEM_ROWS[14] = {
    "..RRRRRRRRR..",
    ".RRRRWWWRRRR.",
    ".RRRRWWWRRRR.",
    ".RRRRWWWRRRR.",
    ".RWWWWWWWWWR.",
    ".RWWWWWWWWWR.",
    ".RWWWWWWWWWR.",
    ".RRRRWWWRRRR.",
    ".RRRRWWWRRRR.",
    "..RRRWWWRRR..",
    "..RRRRRRRRR..",
    "....RRRRR....",
    ".....RRR.....",
    "......R......"
  };

  GPoint origin;
  GPoint stepX;
  GPoint stepDepth;
  bool reverseRows;

  if (!getDateEmblemLattice(
          &origin,
          &stepX,
          &stepDepth,
          &reverseRows)) {
    return;
  }

  /*
   * Red first, white second, so the cross remains visible on top.
   */
  static const char symbols[2] = {
    'R',
    'W'
  };

  for (int16_t pass = 0;
       pass < 2;
       ++pass) {
    const char wantedSymbol =
        symbols[pass];

    graphics_context_set_fill_color(
        ctx,
        wantedSymbol == 'W'
            ? GColorWhite
            : GColorRed);

    for (int16_t planeRow = 0;
         planeRow < AUX_DATE_EMBLEM_HEIGHT;
         ++planeRow) {
      const int16_t sourceRow =
          reverseRows
              ? AUX_DATE_EMBLEM_HEIGHT
                  - 1
                  - planeRow
              : planeRow;

      for (int16_t column = 0;
           column < AUX_DATE_EMBLEM_WIDTH;
           ++column) {
        if (SWISS_EMBLEM_ROWS[sourceRow][column]
                != wantedSymbol) {
          continue;
        }

        const int16_t screenX =
            origin.x
            + column * stepX.x
            + planeRow * stepDepth.x;

        const int16_t screenY =
            origin.y
            + column * stepX.y
            + planeRow * stepDepth.y;

        graphics_fill_rect(
            ctx,
            GRect(
                screenX,
                screenY,
                AUX_DATE_EMBLEM_DOT_SIZE,
                AUX_DATE_EMBLEM_DOT_SIZE),
            0,
            GCornerNone);
      }
    }
  }

  const GColor normalColor =
      invertedTheme
          ? GColorBlack
          : GColorWhite;

  graphics_context_set_fill_color(
      ctx,
      normalColor);
}

static void drawPercentSign(
    GContext *ctx,
    int16_t centerX) {
  static const uint8_t percentPattern[7][5] = {
    { 1, 1, 0, 0, 1 },
    { 1, 1, 0, 1, 0 },
    { 0, 0, 0, 1, 0 },
    { 0, 0, 1, 0, 0 },
    { 0, 1, 0, 0, 0 },
    { 0, 1, 0, 1, 1 },
    { 1, 0, 0, 1, 1 }
  };

  for (int16_t row = 0; row < 7; ++row) {
    for (int16_t column = 0; column < 5; ++column) {
      if (!percentPattern[row][column]) {
        continue;
      }

      const int16_t x =
          centerX
          + (column - 2) * AUX_BATTERY_X_DOT_STEP;

      drawFoldedCell(
          ctx,
          x,
          row + 1,
          false);
    }
  }
}

static void drawDatePlane(
    GContext *ctx) {
  static const int16_t positions[4] = {
    -84,
    -36,
    36,
    84
  };

  for (int index = 0; index < 4; ++index) {
    drawFoldedDigit(
        ctx,
        dateDigits[index],
        positions[index],
        true,
        true);
  }

  drawDateSeparator(ctx);
}

static void drawBatteryPlane(
    GContext *ctx) {
  if (batteryPercent >= 100) {
    // Center the three digits themselves around x = 0.
    drawFoldedDigit(
        ctx,
        1,
        -20,
        false,
        false);

    drawFoldedDigit(
        ctx,
        0,
        0,
        false,
        false);

    drawFoldedDigit(
        ctx,
        0,
        20,
        false,
        false);

    // Percent sign sits to the right and does not influence centering.
    drawPercentSign(
        ctx,
        40);
  } else if (batteryPercent >= 10) {
    // Center the midpoint between the two digits around x = 0.
    drawFoldedDigit(
        ctx,
        batteryPercent / 10,
        -10,
        false,
        false);

    drawFoldedDigit(
        ctx,
        batteryPercent % 10,
        10,
        false,
        false);

    // Percent sign sits to the right and does not influence centering.
    drawPercentSign(
        ctx,
        30);
  } else {
    // Center the single digit itself around x = 0.
    drawFoldedDigit(
        ctx,
        batteryPercent,
        0,
        false,
        false);

    // Percent sign sits to the right and does not influence centering.
    drawPercentSign(
        ctx,
        20);
  }
}

static void updateLayer(Layer *layer, GContext *ctx) {
  int i, n, curd;
  GPoint3 P, *U;

  if (invertedTheme) {
    // White background, black point cloud.
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(
        ctx,
        fullScreenRect,
        0,
        GCornerNone);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorBlack);
  } else {
    // Black background, white point cloud.
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(
        ctx,
        fullScreenRect,
        0,
        GCornerNone);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_fill_color(ctx, GColorWhite);
  }

  for (n=0; n<6; n++) {
    curd = d[n];
    for (i=0, U=DIGIT_OFFSET(curd); i<numPoints[curd]; i++, U++) {
      P.x = U->x + digitOffsetX[n];
      P.y = U->y + digitOffsetY[n];
      P.z = U->z;
      drawPoint(ctx, &P);
    }
  }

  drawDatePlane(ctx);
  drawBatteryPlane(ctx);
}

static void timerCallback(void *data) {
  (void)data;

  if (haveAccel) {
    const int16_t rawX = latestAccel.x >> 5;
    const int16_t rawY = latestAccel.y >> 5;
    const int16_t rawZ = latestAccel.z >> 5;

    if (!smoothedAccelInitialized) {
      smoothedAccelX = rawX;
      smoothedAccelY = rawY;
      smoothedAccelZ = rawZ;
      smoothedAccelInitialized = true;
    } else {
      // 75% newest value, 25% previous value.
      // This removes visible stepping without recreating the old delay.
      smoothedAccelX =
          (smoothedAccelX + 3 * rawX) >> 2;
      smoothedAccelY =
          (smoothedAccelY + 3 * rawY) >> 2;
      smoothedAccelZ =
          (smoothedAccelZ + 3 * rawZ) >> 2;
    }

    GPoint3 smoothedVector = {
      smoothedAccelX,
      smoothedAccelY,
      smoothedAccelZ
    };

    int32_t targetA;
    int32_t targetB;
    int32_t targetC;

    angles(
        &smoothedVector,
        &targetA,
        &targetB,
        &targetC);

    // Amplify only the visual perspective. Raw sensor values
    // remain untouched for smoothing and shake detection.
    targetA = applyVisualTiltGain(targetA);
    targetB = applyVisualTiltGain(targetB);

    bool orientationChanged = false;

    if (!orientationInitialized) {
      a = targetA;
      b = targetB;
      c = targetC;
      orientationInitialized = true;
      orientationChanged = true;
    } else {
      a = applyAngleDeadzone(
          a,
          targetA,
          &orientationChanged);

      b = applyAngleDeadzone(
          b,
          targetB,
          &orientationChanged);

      c = targetC;
    }

    if (orientationChanged) {
      cosa = cos_lookup(a);
      cosb = cos_lookup(b);
      cosc = cos_lookup(c);
      sina = sin_lookup(a);
      sinb = sin_lookup(b);
      sinc = sin_lookup(c);

      layer_mark_dirty(layer);
    }
  }

  timer = app_timer_register(FRAME_INTERVAL_MS, timerCallback, NULL);
}

static void handleTick(struct tm *t, TimeUnits units_changed) {
  (void)units_changed;

  int h = t->tm_hour;

  if (!clock_is_24h_style()) {
    h %= 12;
    if (h==0) h = 12;
  }

  d[0] = h/10;
  d[1] = h%10;
  d[2] = t->tm_min/10;
  d[3] = t->tm_min%10;
  d[4] = t->tm_sec/10;
  d[5] = t->tm_sec%10;

  // Keep seconds current even while the deadzone holds the view still.
  if (layer) {
    layer_mark_dirty(layer);
  }

  dateDigits[0] = t->tm_mday / 10;
  dateDigits[1] = t->tm_mday % 10;
  dateDigits[2] = (t->tm_mon + 1) / 10;
  dateDigits[3] = (t->tm_mon + 1) % 10;

  BatteryChargeState battery =
      battery_state_service_peek();

  batteryPercent =
      battery.charge_percent;

  if (batteryPercent < 0) {
    batteryPercent = 0;
  } else if (batteryPercent > 100) {
    batteryPercent = 100;
  }

}

static int32_t absoluteAccelDelta(
    int16_t current,
    int16_t previous) {
  const int32_t difference =
      (int32_t)current - (int32_t)previous;

  return difference < 0
      ? -difference
      : difference;
}

static void handleAccel(
    AccelData *data,
    uint32_t num_samples) {
  if (!data || num_samples == 0) {
    return;
  }

  const AccelData newestSample =
      data[num_samples - 1];

  // Keep orientation movement independent from shake detection.
  latestAccel = newestSample;

  // Do not interpret watch vibration as a user shake.
  if (newestSample.did_vibrate) {
    havePreviousShakeAccel = false;
    return;
  }

  haveAccel = true;

  if (shakeCooldownSamples > 0) {
    --shakeCooldownSamples;
  }

  if (havePreviousShakeAccel
      && shakeCooldownSamples == 0) {
    const int32_t totalDelta =
        absoluteAccelDelta(
            newestSample.x,
            previousShakeAccel.x)
        + absoluteAccelDelta(
            newestSample.y,
            previousShakeAccel.y)
        + absoluteAccelDelta(
            newestSample.z,
            previousShakeAccel.z);

    if (totalDelta >= SHAKE_DELTA_THRESHOLD) {
      invertedTheme = !invertedTheme;
      persist_write_bool(
          CONFIG_KEY_INVERTED_THEME,
          invertedTheme);

      shakeCooldownSamples =
          SHAKE_COOLDOWN_SAMPLES;

      if (layer) {
        layer_mark_dirty(layer);
      }

      APP_LOG(
          APP_LOG_LEVEL_INFO,
          "Shake theme: %s",
          invertedTheme
              ? "white background"
              : "black background");
    }
  }

  previousShakeAccel = newestSample;
  havePreviousShakeAccel = true;
}

static void initPointList() {
  int i, j, n, px, py;
  GPoint3 *P;

  for (n=0; n<10; n++) {
    P = DIGIT_OFFSET(n);
    numPoints[n] = 0;

    for (j=0; j<9; j++) {
      py = (j-4)*(SIZE>>3);

      for (i=0; i<9; i++) {
        px = (i-4)*(SIZE>>3);

        if (DIGIT(n, i, j)) {
          P->x = px;
          P->y = py;
          P->z = ZPOS;
          P++;
          numPoints[n]++;
        }
      }
    }
  }
}

static void initDigitOffsets() {
  int n;

  for (n=0; n<6; n++) {
    digitOffsetX[n] = (2*(n%2)-1)*((SIZE>>1)+OFFSET);
    digitOffsetY[n] = ((n/2)-1)*(SIZE+2*OFFSET);
  }
}

static void handleSettingsReceived(
    DictionaryIterator *iterator,
    void *context) {
  (void)context;

  Tuple *showEmblemTuple =
      dict_find(
          iterator,
          MESSAGE_KEY_ShowEmblem);

  if (!showEmblemTuple) {
    return;
  }

  showSwissEmblem =
      showEmblemTuple->value->int32 != 0;

  persist_write_bool(
      CONFIG_KEY_SHOW_SWISS_EMBLEM,
      showSwissEmblem);

  if (layer) {
    layer_mark_dirty(layer);
  }

  APP_LOG(
      APP_LOG_LEVEL_INFO,
      "Swiss emblem: %s",
      showSwissEmblem
          ? "enabled"
          : "disabled");
}

static void init(void) {
  time_t now;

  initDigitOffsets();
  initPointList();

  if (persist_exists(CONFIG_KEY_INVERTED_THEME)) {
    invertedTheme = persist_read_bool(
        CONFIG_KEY_INVERTED_THEME);
  } else {
    invertedTheme = false;
  }

  if (persist_exists(CONFIG_KEY_SHOW_SWISS_EMBLEM)) {
    showSwissEmblem =
        persist_read_bool(
            CONFIG_KEY_SHOW_SWISS_EMBLEM);
  } else {
    showSwissEmblem = true;
  }

  time(&now);
  handleTick(localtime(&now), 0);

  window = window_create();
  window_set_background_color(window, GColorBlack);
  window_stack_push(window, true);
  Layer *rootLayer = window_get_root_layer(window);

  layer = layer_create(fullScreenRect);
  layer_set_update_proc(layer, updateLayer);
  layer_add_child(rootLayer, layer);

  tick_timer_service_subscribe(SECOND_UNIT, handleTick);

  app_message_register_inbox_received(
      handleSettingsReceived);

  app_message_open(
      64,
      64);
  
  accel_service_set_sampling_rate(SENSOR_RATE);
  accel_data_service_subscribe(1, handleAccel);
  timerCallback(NULL);
}

static void deinit(void) {
  app_timer_cancel(timer);
  accel_data_service_unsubscribe();
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  layer_destroy(layer);
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
