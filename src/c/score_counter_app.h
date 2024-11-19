/**
 * Author: Marek Jankech
 */

#pragma once

#include <pebble.h>

#define MIN_SCORE 0
#define MAX_SCORE 999

#define INBOUND_SIZE 50
#define OUTBOUND_SIZE 50

#define RESET_BG_COLOR_MS 500
#define SC_BLINK_INTERVAL 400

#define MARGIN 8

#define STATUS_BAR_HEIGHT 26
#define STATUS_BAR_ICON_WIDTH_HEIGHT 15
#define SCORE_TEXT_RECT_HEIGHT 36

#define SC_LONGER_DIMENSION 48
#define SC_SHORTER_DIMENSION 12

#define CONN_BUFF_SIZE 8
#define TIME_BUFF_SIZE 12
#define BATT_CHARGE_BUFF_SIZE 5

#define LINKED_TXT "Linked"
#define NO_LINK_TXT "No link"


/**
 * Enums
 */

/**
 * The UserRole should affect the mode how the score is displayed
 * on the smartwatch.
 * PLAYER role should make the score appear vertically, REFEREE horizontally.
 * To correctly transfer the score to the score counter, we also need to know
 * where the score counter is located: left/right edge for the PLAYER,
 * same side/opposite side for the REFEREE. See SCPositionRelativeToPlayer
 * and SCPositionRelativeToReferee for more detail.
 */
typedef enum {
    PLAYER = 1,
    REFEREE = 2
} UserRole;

/**
 * Score counter position relative to the player who controls the score.
 * It makes sense only when the user role is PLAYER.
 * It is assumed that all players of one team should be located initially
 * on one half of the court, facing to the opponents. Thus the score counter
 * should be located in the center (between the halves) either on the left edge
 * or the right edge.
 */
typedef enum {
    LEFT_EDGE = 1,
    RIGHT_EDGE = 2
} SCPositionRelativeToPlayer;

/**
 * Score counter position relative to the referee who controls the score.
 * It makes sense only when the user role is REFEREE.
 * It is assumed that the referee is located in the center of the longer side 
 * of the court on any of the shorter sides of the court. In simple words,
 * if there's a net in the center, the referee should be located on any 
 * of the sides of the net.
 * Thus the score counter is either on the same side as the referee = bottom 
 * edge on the smartwatch screen (score counter not facing to the referee) or 
 * on the opposite side = top edge on the smartwatch screen (score counter 
 * facing to the referee).
 */
typedef enum {
    SAME_SIDE = 1,
    OPPOSITE_SIDE = 2
} SCPositionRelativeToReferee;

typedef enum {
    OPPONENT_SCORE,
    MY_SCORE,
    WHOLE_SCORE
} ScoreOnSmartwatch;

typedef enum {
    NORMAL_MODE,
    SETTING_MODE
} ButtonMode;

typedef enum {
    SC_SET_LEFT,
    SC_SET_TOP,
    SC_SET_RIGHT,
    SC_SET_BOTTOM
} SettingModeSCPosition;

typedef enum {
  CMD_KEY = 10,
  SCORE_1_KEY = 11,
  SCORE_2_KEY = 12,
  TIMESTAMP_KEY = 13
} DictSendKey;

typedef enum {
  CMD_SET_SCORE_VAL = 1,
  CMD_SYNC_SCORE_VAL = 2
} DictSendCmdVal;

typedef enum {
  S_SCORE_1_KEY = 11,
  S_SCORE_2_KEY = 12,
  S_TIMESTAMP_KEY = 13,
  S_USER_ROLE_KEY = 14,
  S_SC_POS_TO_PLAYER_KEY = 15,
  S_SC_POS_TO_REFEREE_KEY = 16
} Storage;

typedef enum {
    SCORE_1 = 0x1,
    SCORE_2 = 0x2
} ScoreNumber;


/**
 * Structs
 */

typedef struct {
  char connection[CONN_BUFF_SIZE];
  char time[TIME_BUFF_SIZE];
  char battery_charge[BATT_CHARGE_BUFF_SIZE];
} TopBarInfo;

typedef struct {
  uint16_t score_1;
  uint16_t score_2;
  char score_1_text[4];
  char score_2_text[4];
  char whole_score_text[8];
  time_t timestamp;
  UserRole user_role;
  SCPositionRelativeToPlayer sc_2_player_position;
  SCPositionRelativeToReferee sc_2_referee_position;
} Score;


/**
 * Prototypes
 */

static void send_msg(DictSendCmdVal cmd_val);
static void horizontal_ruler_update_proc(Layer *layer, GContext *ctx);
// static GRect init_text_layer(Layer *parent_layer, TextLayer **text_layer, 
//   int16_t y, int16_t h, int16_t additional_right_margin, char *font_key);
static void sc_update_proc(Layer *layer, GContext *ctx);
static int16_t calc_score_text_layer_y_coord(GRect parent_layer_bounds, 
  ScoreOnSmartwatch which_score, int16_t height);
static GRect init_score_text_layer(Layer *parent_layer, TextLayer **text_layer,
  int16_t h, char *font_key, ScoreOnSmartwatch which_score);
static void init_separate_score_text_layers(Layer *window_layer);
static void init_whole_score_text_layer(Layer *window_layer);
static void init_score_text_layers(Layer *window_layer);
static inline void create_sc_layer_on_top(GRect bounds);
static inline void create_sc_layer_on_right(GRect bounds);
static inline void create_sc_layer_on_bottom(GRect bounds);
static inline void create_sc_layer_on_left(GRect bounds);
static void create_score_counter_layer(Layer *window_layer);
static void main_window_load(Window *window);
static void main_window_unload(Window *window);
static void blink_sc_timer_handler(void *context);
static void up_click_handler(ClickRecognizerRef recognizer, void *context);
static void down_click_handler(ClickRecognizerRef recognizer, void *context);
static void up_long_click_handler_down(
  ClickRecognizerRef recognizer, void *context);
static void down_long_click_handler_down(
  ClickRecognizerRef recognizer, void *context);
static void select_click_handler(ClickRecognizerRef recognizer, void *context);
static void select_long_click_handler_down(ClickRecognizerRef recognizer, void *context);
static void back_click_handler(ClickRecognizerRef recognizer, void *context);
static void set_setting_mode_cfg_from_normal_mode_cfg();
static void set_normal_mode_cfg_from_setting_mode_cfg();
static void set_score(ScoreNumber score_number);
static void outbox_sent_handler(DictionaryIterator *iterator, void *context);
// static void send_msg_timeout_handler(void *context);
static void reset_bg_color_callback(void *data);
static void set_bg_color_on_colored_screen(GColor8 color);
static void click_config_provider(void *context);
static void init_score();
static void tick_handler(struct tm *tick_time, TimeUnits changed);
static void app_connection_handler(bool connected);
static void battery_state_handler(BatteryChargeState charge);
static void init_status_bar();
static void init();
static void deinit();