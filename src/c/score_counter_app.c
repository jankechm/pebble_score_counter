/**
 * Author: Marek Jankech
 */

#include <pebble.h>
#include "score_counter_app.h"
#include "custom_status_bar.h"


static Window *s_main_window;
static CustomStatusBarLayer *custom_status_bar;
/**
 * Displayed when the user role is PLAYER.
 */
static TextLayer *s_my_score_text_layer = NULL;
static TextLayer *s_opponent_score_text_layer = NULL;
/**
 * Displayed when the user is REFEREE.
 */
static TextLayer *s_whole_score_text_layer = NULL;

static Layer *horizontal_ruler_layer = NULL;
static Layer *score_counter_layer = NULL;

static TopBarInfo *top_bar_info;
static Score *score;

static ButtonMode btn_mode = NORMAL_MODE;
static SettingModeSCPosition setting_mode_sc_position;

static AppTimer *blink_sc_timer = NULL;


static void send_msg(DictSendCmdVal cmd_val) {
  // If not connected, do not continue.
  if (!connection_service_peek_pebble_app_connection()) {
    set_bg_color_on_colored_screen(GColorPurple);
    return;
  }

  uint8_t score_1_to_transfer;
  uint8_t score_2_to_transfer;
  if ((score->user_role == PLAYER && score->sc_2_player_position == LEFT_EDGE) ||
      (score->user_role == REFEREE && score->sc_2_referee_position == OPPOSITE_SIDE)) {
    score_1_to_transfer = score->score_1;
    score_2_to_transfer = score->score_2;
  } else {
    score_1_to_transfer = score->score_2;
    score_2_to_transfer = score->score_1;
  }

  Tuplet cmd_tuplet = TupletInteger(CMD_KEY, (uint8_t)cmd_val);
  Tuplet score1_tuplet = TupletInteger(SCORE_1_KEY, score_1_to_transfer);
  Tuplet score2_tuplet = TupletInteger(SCORE_2_KEY, score_2_to_transfer);

  DictionaryIterator *iter;
  AppMessageResult result_code = app_message_outbox_begin(&iter);

  if (result_code == APP_MSG_OK) {
    dict_write_tuplet(iter, &cmd_tuplet);
    dict_write_tuplet(iter, &score1_tuplet);
    dict_write_tuplet(iter, &score2_tuplet);

    // When syncing, timestamp is needed
    if (cmd_val == CMD_SYNC_SCORE_VAL) {
      Tuplet timestamp_tuplet = TupletInteger(TIMESTAMP_KEY, (unsigned int)score->timestamp);
      dict_write_tuplet(iter, &timestamp_tuplet);
    }

    dict_write_end(iter);

    result_code = app_message_outbox_send();

    if (result_code != APP_MSG_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result_code);
      set_bg_color_on_colored_screen(GColorOrange);
    } 
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result_code);
  }
}

static void horizontal_ruler_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h / 2), GPoint(bounds.size.w, bounds.size.h / 2));
}

static void sc_update_proc(Layer *layer, GContext *ctx) {
  const GRect bounds = layer_get_bounds(layer);

  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 4);
  graphics_context_set_fill_color(ctx, GColorBlack);

  graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), 4, GCornersAll);

  // graphics_context_set_stroke_width(ctx, 2);

  // if (score->user_role == PLAYER) {
  //   graphics_draw_line(ctx, GPoint(0, bounds.size.h / 2), GPoint(bounds.size.w, bounds.size.h / 2));
  // } else {
  //   graphics_draw_line(ctx, GPoint(bounds.size.w / 2, 0), GPoint(bounds.size.w / 2, bounds.size.h));
  // }
}

// static GRect init_text_layer(Layer *parent_layer, TextLayer **text_layer, int16_t y, int16_t h, int16_t additional_right_margin, char *font_key) {
//   // why "-1" (and then "+2")? because for this font we need to compensate for weird white-spacing
//   const int16_t font_compensator = strcmp(font_key, FONT_KEY_LECO_38_BOLD_NUMBERS) == 0 ? 3 : 1;

//   const GRect frame = GRect(MARGIN - font_compensator, y, 
//     layer_get_bounds(parent_layer).size.w - 2 * MARGIN + 2 * font_compensator - additional_right_margin, h);

//   *text_layer = text_layer_create(frame);
//   text_layer_set_background_color(*text_layer, GColorClear);
//   // text_layer_set_text_color(*text_layer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
//   text_layer_set_text_color(*text_layer, GColorBlack);
//   text_layer_set_font(*text_layer, fonts_get_system_font(font_key));
//   text_layer_set_text_alignment(*text_layer, GTextAlignmentCenter);
//   layer_add_child(parent_layer, text_layer_get_layer(*text_layer));

//   return frame;
// }

static int16_t calc_score_text_layer_y_coord(GRect parent_layer_bounds, 
  ScoreOnSmartwatch which_score, int16_t height) {
  
  int16_t y;

  switch (which_score) {
    case OPPONENT_SCORE:
      y = (parent_layer_bounds.size.h - STATUS_BAR_HEIGHT) 
        / 4 + STATUS_BAR_HEIGHT- height / 2;
      break;
    case MY_SCORE:
      y = (parent_layer_bounds.size.h - STATUS_BAR_HEIGHT) 
        / 4 * 3 + STATUS_BAR_HEIGHT - height / 2;
      break;
    default:
      y = (parent_layer_bounds.size.h - STATUS_BAR_HEIGHT) 
        / 2 + STATUS_BAR_HEIGHT - height / 2;
      break;
  }

  return y;
}

static GRect init_score_text_layer(Layer *parent_layer, TextLayer **text_layer,
  int16_t h, char *font_key, ScoreOnSmartwatch which_score) {

  const GRect bounds = layer_get_bounds(parent_layer);
  int16_t y = calc_score_text_layer_y_coord(bounds, which_score, h);

  GRect frame;
  if (which_score == WHOLE_SCORE) {
    frame = GRect(0, y, bounds.size.w, h);
  } else {
    frame = GRect(MARGIN + SC_SHORTER_DIMENSION, y, 
      bounds.size.w - 2 * (MARGIN + SC_SHORTER_DIMENSION), h);
  }

  *text_layer = text_layer_create(frame);
  text_layer_set_text_color(*text_layer, GColorBlack);
  text_layer_set_font(*text_layer, fonts_get_system_font(font_key));
  text_layer_set_text_alignment(*text_layer, GTextAlignmentCenter);
  layer_add_child(parent_layer, text_layer_get_layer(*text_layer));

  return frame;
}

static void init_separate_score_text_layers(Layer *window_layer) {
  init_score_text_layer(window_layer, &s_opponent_score_text_layer, 
    SCORE_TEXT_RECT_HEIGHT, FONT_KEY_LECO_36_BOLD_NUMBERS, OPPONENT_SCORE);
  init_score_text_layer(window_layer, &s_my_score_text_layer, 
    SCORE_TEXT_RECT_HEIGHT, FONT_KEY_LECO_36_BOLD_NUMBERS, MY_SCORE);

  text_layer_set_text(s_my_score_text_layer, score->score_1_text);
  text_layer_set_text(s_opponent_score_text_layer, score->score_2_text);      
}

static void init_whole_score_text_layer(Layer *window_layer) {
  init_score_text_layer(window_layer, &s_whole_score_text_layer, 
        SCORE_TEXT_RECT_HEIGHT, FONT_KEY_LECO_32_BOLD_NUMBERS, WHOLE_SCORE);

  text_layer_set_text(s_whole_score_text_layer, score->whole_score_text);
}

static void init_score_text_layers(Layer *window_layer) {
  // SETTING_MODE has priority
  if (btn_mode == SETTING_MODE) {
    if (setting_mode_sc_position == SC_SET_LEFT 
      || setting_mode_sc_position == SC_SET_RIGHT) {
        init_separate_score_text_layers(window_layer);

        if (s_whole_score_text_layer != NULL) {
          text_layer_destroy(s_whole_score_text_layer);
          s_whole_score_text_layer = NULL;
        }
    } else {
        init_whole_score_text_layer(window_layer);

        if (s_opponent_score_text_layer != NULL) {
          text_layer_destroy(s_opponent_score_text_layer);
          s_opponent_score_text_layer = NULL;
        }
        if (s_my_score_text_layer != NULL) {
          text_layer_destroy(s_my_score_text_layer);
          s_my_score_text_layer = NULL;
        }
    }
  } else { // NORMAL_MODE
    if (score->user_role == PLAYER) {
      init_separate_score_text_layers(window_layer);

      if (s_whole_score_text_layer != NULL) {
        text_layer_destroy(s_whole_score_text_layer);
        s_whole_score_text_layer = NULL;
      }
    } else {
      init_whole_score_text_layer(window_layer);

      if (s_opponent_score_text_layer != NULL) {
        text_layer_destroy(s_opponent_score_text_layer);
        s_opponent_score_text_layer = NULL;
      }
      if (s_my_score_text_layer != NULL) {
        text_layer_destroy(s_my_score_text_layer);
        s_my_score_text_layer = NULL;
      }
    }
  }
}

static inline void create_sc_layer_on_top(GRect bounds) {
  score_counter_layer = layer_create(GRect(bounds.size.w / 2 - SC_LONGER_DIMENSION / 2,
      STATUS_BAR_HEIGHT + MARGIN, SC_LONGER_DIMENSION, SC_SHORTER_DIMENSION));
}

static inline void create_sc_layer_on_right(GRect bounds) {
  score_counter_layer = layer_create(GRect(bounds.size.w - MARGIN - SC_SHORTER_DIMENSION,
      (bounds.size.h - STATUS_BAR_HEIGHT) / 2 + STATUS_BAR_HEIGHT - SC_LONGER_DIMENSION / 2,
      SC_SHORTER_DIMENSION, SC_LONGER_DIMENSION));  
}

static inline void create_sc_layer_on_bottom(GRect bounds) {
  score_counter_layer = layer_create(GRect(bounds.size.w / 2 - SC_LONGER_DIMENSION / 2,
      bounds.size.h - MARGIN - SC_SHORTER_DIMENSION, 
      SC_LONGER_DIMENSION, SC_SHORTER_DIMENSION)); 
}

static inline void create_sc_layer_on_left(GRect bounds) {
  score_counter_layer = layer_create(GRect(MARGIN, 
      (bounds.size.h - STATUS_BAR_HEIGHT) / 2 + STATUS_BAR_HEIGHT - SC_LONGER_DIMENSION / 2,
      SC_SHORTER_DIMENSION, SC_LONGER_DIMENSION));
}

static void create_score_counter_layer(Layer *window_layer) {
  if (score_counter_layer != NULL) {
    layer_destroy(score_counter_layer);
  }

  const GRect bounds = layer_get_bounds(window_layer);

  // SETTING_MODE has priority
  if (btn_mode == SETTING_MODE) {
    switch (setting_mode_sc_position) {
      case SC_SET_TOP:
        create_sc_layer_on_top(bounds);
        break;
      case SC_SET_RIGHT:
        create_sc_layer_on_right(bounds);
        break;
      case SC_SET_BOTTOM:
        create_sc_layer_on_bottom(bounds);
        break;
      default:
        create_sc_layer_on_left(bounds);
        break;
    }
  // NORMAL_MODE
  } else {
    if (score->user_role == PLAYER) {
      if (score->sc_2_player_position == LEFT_EDGE) {
        create_sc_layer_on_left(bounds);
      } else {
        create_sc_layer_on_right(bounds);
      }
    } else {
      if (score->sc_2_referee_position == SAME_SIDE) {
        create_sc_layer_on_bottom(bounds);
      } else {
        create_sc_layer_on_top(bounds);
      }
    }
  }
}

static void init_score_counter_layer(Layer *window_layer) {
  create_score_counter_layer(window_layer);
  layer_set_update_proc(score_counter_layer, sc_update_proc);
  layer_add_child(window_layer, score_counter_layer);
}

static void init_ruler_layer(Layer *window_layer) {
  if ((btn_mode == SETTING_MODE && 
    (setting_mode_sc_position == SC_SET_LEFT || setting_mode_sc_position == SC_SET_RIGHT)) 
      || (btn_mode == NORMAL_MODE && score->user_role == PLAYER)) {
        
    const GRect bounds = layer_get_bounds(window_layer);

    horizontal_ruler_layer = layer_create(GRect(MARGIN + SC_SHORTER_DIMENSION + MARGIN, 
      (bounds.size.h - STATUS_BAR_HEIGHT) / 2 + STATUS_BAR_HEIGHT - 2, 
      bounds.size.w - 2 * (MARGIN + SC_SHORTER_DIMENSION + MARGIN), 4));
    layer_set_update_proc(horizontal_ruler_layer, horizontal_ruler_update_proc);
    layer_add_child(window_layer, horizontal_ruler_layer);
  } else {
    if (horizontal_ruler_layer != NULL) {
      layer_destroy(horizontal_ruler_layer);
    }
  }
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  init_status_bar(window_layer);
  init_ruler_layer(window_layer);
  init_score_counter_layer(window_layer);
  init_score_text_layers(window_layer);
}

static void main_window_unload(Window *window) {
  custom_status_bar_layer_destroy(custom_status_bar);

  if (s_my_score_text_layer != NULL) {
    text_layer_destroy(s_my_score_text_layer);
    text_layer_destroy(s_opponent_score_text_layer);
  }
  if (s_whole_score_text_layer != NULL) {
    text_layer_destroy(s_whole_score_text_layer);
  }
  
  if (score->user_role == PLAYER && horizontal_ruler_layer != NULL) {
    layer_destroy(horizontal_ruler_layer);
  }

  free(top_bar_info);
  free(score);
}

static void blink_sc_timer_handler(void *context) {
  layer_set_hidden(score_counter_layer, !layer_get_hidden(score_counter_layer));
  blink_sc_timer = app_timer_register(SC_BLINK_INTERVAL, blink_sc_timer_handler, NULL);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (btn_mode == NORMAL_MODE) {
    if (score->score_2 < MAX_SCORE) {
      score->score_2 += 1;
    } else {
      score->score_2 = MIN_SCORE;
    }

    set_score(SCORE_2);
  } else {
    switch (setting_mode_sc_position) {
      case SC_SET_LEFT:
        setting_mode_sc_position = SC_SET_TOP;
        break;
      case SC_SET_TOP:
        setting_mode_sc_position = SC_SET_RIGHT;
        break;
      case SC_SET_RIGHT:
        setting_mode_sc_position = SC_SET_BOTTOM;
        break;
      default:
        setting_mode_sc_position = SC_SET_LEFT;
        break;
    }

    Layer *window_layer = window_get_root_layer(s_main_window);

    init_score_counter_layer(window_layer);
    init_score_text_layers(window_layer);
    init_ruler_layer(window_layer);
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (btn_mode == NORMAL_MODE) {
    if (score->score_1 < MAX_SCORE) {
      score->score_1 += 1;
    } else {
      score->score_1 = MIN_SCORE;
    }

    set_score(SCORE_1);
  }
}

static void up_long_click_handler_down(ClickRecognizerRef recognizer, void *context) {
  if (btn_mode == NORMAL_MODE) {
    if (score->score_2 > MIN_SCORE) {
      score->score_2 -= 1;
    } else {
      score->score_2 = MAX_SCORE;
    }

    set_score(SCORE_2);
  }
}

static void down_long_click_handler_down(ClickRecognizerRef recognizer, void *context) {
  if (btn_mode == NORMAL_MODE) {
    if (score->score_1 > MIN_SCORE) {
      score->score_1 -= 1;
    } else {
      score->score_1 = MAX_SCORE;
    }

    set_score(SCORE_1);
  }
}

/**
 * Select button single click should re-send last score.
 */
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  reset_bg_color_callback(NULL);
  send_msg(CMD_SET_SCORE_VAL);
}

/**
 * Select button long click should reset the score.
 */
static void select_long_click_handler_down(ClickRecognizerRef recognizer, void *context) {
  if (btn_mode == NORMAL_MODE) {
    score->score_1 = 0;
    score->score_2 = 0;

    set_score(SCORE_1 | SCORE_2);
  }
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (btn_mode == NORMAL_MODE) {
    set_setting_mode_cfg_from_normal_mode_cfg();
    // tick_timer_service_subscribe(SECOND_UNIT, blink_score_tick_handler);
    // TODO use AppTimer instead of tick_timer service
    blink_sc_timer = app_timer_register(SC_BLINK_INTERVAL, blink_sc_timer_handler, NULL);

    btn_mode = SETTING_MODE;
  } else {
    // Stop SC blinking, restore SC position & last score if swapped.
    // tick_timer_service_unsubscribe();
    Layer *window_layer = window_get_root_layer(s_main_window);

    init_score_counter_layer(window_layer);
    init_score_text_layers(window_layer);
    init_ruler_layer(window_layer);

    // snprintf(score->score_1_text, sizeof(score->score_1_text), "%d", score->score_1);
    // snprintf(score->score_2_text, sizeof(score->score_2_text), "%d", score->score_2);

    app_timer_cancel(blink_sc_timer);

    btn_mode = NORMAL_MODE;
  }
}

/**
 * score_number: a bitmask of all the score parts that have changed
 */
static void set_score(ScoreNumber score_number) {
  if (score_number & SCORE_1) {
    snprintf(score->score_1_text, sizeof(score->score_1_text), "%d", score->score_1);
    persist_write_int(S_SCORE_1_KEY, score->score_1);

    if (score->user_role == PLAYER) {
      text_layer_set_text(s_my_score_text_layer, score->score_1_text);
    }
  }
  if (score_number & SCORE_2) {
    snprintf(score->score_2_text, sizeof(score->score_2_text), "%d", score->score_2);
    persist_write_int(S_SCORE_2_KEY, score->score_2);

    if (score->user_role == PLAYER) {
      text_layer_set_text(s_opponent_score_text_layer, score->score_2_text);
    }
  }

  snprintf(score->whole_score_text, sizeof(score->whole_score_text), "%s:%s", 
      score->score_1_text, score->score_2_text);

  if (score->user_role == REFEREE) {
    text_layer_set_text(s_whole_score_text_layer, score->whole_score_text);
  }

  time(&score->timestamp);
  reset_bg_color_callback(NULL);
  send_msg(CMD_SET_SCORE_VAL);
  persist_write_int(S_TIMESTAMP_KEY, score->timestamp);
}

static void set_setting_mode_cfg_from_normal_mode_cfg() {
  if (score->user_role == PLAYER) {
    if (score->sc_2_player_position == LEFT_EDGE) {
      setting_mode_sc_position = SC_SET_LEFT;
    } else {
      setting_mode_sc_position = SC_SET_RIGHT;
    }
  } else {
    if (score->sc_2_referee_position == SAME_SIDE) {
      setting_mode_sc_position = SC_SET_BOTTOM;
    } else {
      setting_mode_sc_position = SC_SET_TOP;
    }
  }
}

static void set_normal_mode_cfg_from_setting_mode_cfg() {
  switch (setting_mode_sc_position) {
    case SC_SET_LEFT:
      score->user_role = PLAYER;
      score->sc_2_player_position = LEFT_EDGE;
      break;
    case SC_SET_RIGHT:
      score->user_role = PLAYER;
      score->sc_2_player_position = RIGHT_EDGE;
      break;
    case SC_SET_TOP:
      score->user_role = REFEREE;
      score->sc_2_player_position = OPPOSITE_SIDE;
      break;
    case SC_SET_BOTTOM:
      score->user_role = REFEREE;
      score->sc_2_player_position = SAME_SIDE;
      break;
  }
}

static void outbox_sent_handler(DictionaryIterator *iterator, void *context) {
  // Message was succesfully sent, cancel retry timer.
  // app_timer_cancel(send_msg_timout_timer);
  // send_msg_num_retries = 0;

  set_bg_color_on_colored_screen(GColorGreen);
}

static void outbox_failed_handler(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  set_bg_color_on_colored_screen(GColorRed);
}

static void reset_bg_color_callback(void *data) {
  window_set_background_color(s_main_window, GColorWhite);
}

static void set_bg_color_on_colored_screen(GColor8 color) {
  #ifdef PBL_COLOR
    window_set_background_color(s_main_window, color);
  #endif
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 400, up_long_click_handler_down, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 400, down_long_click_handler_down, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 1000, select_long_click_handler_down, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

static void init_score() {
  score = (Score *)malloc(sizeof(Score));

  if (persist_exists(S_SCORE_1_KEY)) {
    score->score_1 = persist_read_int(S_SCORE_1_KEY);
  } else {
    score->score_1 = 0;
  }
  if (persist_exists(S_SCORE_2_KEY)) {
    score->score_2 = persist_read_int(S_SCORE_2_KEY);
  } else {
    score->score_2 = 0;
  }
  if (persist_exists(S_TIMESTAMP_KEY)) {
    score->timestamp = persist_read_int(S_TIMESTAMP_KEY);
  } else {
    score->timestamp = 0;
  }
  if (persist_exists(S_USER_ROLE_KEY)) {
    score->user_role = persist_read_int(S_USER_ROLE_KEY);
  } else {
    score->user_role = PLAYER;
  }
  if (persist_exists(S_SC_POS_TO_PLAYER_KEY)) {
    score->sc_2_player_position = persist_read_int(S_SC_POS_TO_PLAYER_KEY);
  } else {
    score->sc_2_player_position = LEFT_EDGE;
  }
  if (persist_exists(S_SC_POS_TO_REFEREE_KEY)) {
    score->sc_2_referee_position = persist_read_int(S_SC_POS_TO_REFEREE_KEY);
  } else {
    score->sc_2_referee_position = SAME_SIDE;
  }

  snprintf(score->score_1_text, sizeof(score->score_1_text), "%d", score->score_1);
  snprintf(score->score_2_text, sizeof(score->score_2_text), "%d", score->score_2);
  snprintf(score->whole_score_text, sizeof(score->whole_score_text), "%s:%s", 
      score->score_1_text, score->score_2_text);
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  // Read time into a string buffer
  strftime(top_bar_info->time, TIME_BUFF_SIZE, "%H:%M", tick_time);

  custom_status_bar_layer_set_text(custom_status_bar, CSB_TEXT_CENTER, top_bar_info->time);
}

static void app_connection_handler(bool connected) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Pebble app %sconnected", connected ? "" : "dis");

  custom_status_bar_layer_set_text(custom_status_bar, CSB_TEXT_LEFT, 
    connected ? LINKED_TXT : NO_LINK_TXT);
}

static void battery_state_handler(BatteryChargeState charge) {
  snprintf(top_bar_info->battery_charge, BATT_CHARGE_BUFF_SIZE, "%d%%", charge.charge_percent);

  custom_status_bar_layer_set_text(custom_status_bar, CSB_TEXT_RIGHT, top_bar_info->battery_charge);
}

static void init_status_bar(Layer *window_layer) {
  top_bar_info = (TopBarInfo *)malloc(sizeof(TopBarInfo));

  custom_status_bar = custom_status_bar_layer_create(
    STATUS_BAR_HEIGHT, GColorBlack, STATUS_BAR_ICON_WIDTH_HEIGHT);

  if (connection_service_peek_pebble_app_connection()) {
    strncpy(top_bar_info->connection, LINKED_TXT, CONN_BUFF_SIZE);
  } else {
    strncpy(top_bar_info->connection, NO_LINK_TXT, CONN_BUFF_SIZE);
  }
  
  clock_copy_time_string(top_bar_info->time, TIME_BUFF_SIZE);

  BatteryChargeState state = battery_state_service_peek();
  snprintf(top_bar_info->battery_charge, BATT_CHARGE_BUFF_SIZE, "%d%%", state.charge_percent);

  custom_status_bar_layer_set_text(custom_status_bar, CSB_TEXT_LEFT, top_bar_info->connection);
  custom_status_bar_layer_set_text(custom_status_bar, CSB_TEXT_CENTER, top_bar_info->time);
  custom_status_bar_layer_set_text(custom_status_bar, CSB_TEXT_RIGHT, top_bar_info->battery_charge);

  custom_status_bar_layer_set_text_font(
    custom_status_bar, CSB_TEXT_LEFT, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  custom_status_bar_layer_set_text_font(
    custom_status_bar, CSB_TEXT_CENTER, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  custom_status_bar_layer_set_text_font(
    custom_status_bar, CSB_TEXT_RIGHT, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));

  layer_add_child(window_layer, custom_status_bar);
}

static void init() {
  init_score();

  // Get updates when the current minute changes
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Get the updates when the connection to the Pebble app on the phone changes.
  connection_service_subscribe((ConnectionHandlers) {
  .pebble_app_connection_handler = app_connection_handler
  });

  // Get battery state updates
  battery_state_service_subscribe(battery_state_handler);

  s_main_window = window_create();
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_set_user_data(s_main_window, score);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });

  app_message_register_outbox_sent(outbox_sent_handler);
  app_message_register_outbox_failed(outbox_failed_handler);
  app_message_open(INBOUND_SIZE, OUTBOUND_SIZE);

  window_stack_push(s_main_window, true);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
