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
static TextLayer *s_my_score_text_layer;
static TextLayer *s_opponent_score_text_layer;
static Layer *horizontal_ruler_layer;
/**
 * Displayed when the user is REFEREE.
 */
static TextLayer *s_whole_score_text_layer;

static TopBarInfo *top_bar_info;
static Score *score;

static const int16_t MARGIN = 8;


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

static GRect init_text_layer(Layer *parent_layer, TextLayer **text_layer, int16_t y, int16_t h, int16_t additional_right_margin, char *font_key) {
  // why "-1" (and then "+2")? because for this font we need to compensate for weird white-spacing
  const int16_t font_compensator = strcmp(font_key, FONT_KEY_LECO_38_BOLD_NUMBERS) == 0 ? 3 : 1;

  const GRect frame = GRect(MARGIN - font_compensator, y, 
    layer_get_bounds(parent_layer).size.w - 2 * MARGIN + 2 * font_compensator - additional_right_margin, h);

  *text_layer = text_layer_create(frame);
  text_layer_set_background_color(*text_layer, GColorClear);
  // text_layer_set_text_color(*text_layer, PBL_IF_COLOR_ELSE(GColorWhite, GColorBlack));
  text_layer_set_text_color(*text_layer, GColorBlack);
  text_layer_set_font(*text_layer, fonts_get_system_font(font_key));
  text_layer_set_text_alignment(*text_layer, GTextAlignmentCenter);
  layer_add_child(parent_layer, text_layer_get_layer(*text_layer));

  return frame;
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);

  horizontal_ruler_layer = layer_create(GRect(MARGIN, bounds.size.h / 2 - 2, bounds.size.w - 2 * MARGIN, 4));
  layer_set_update_proc(horizontal_ruler_layer, horizontal_ruler_update_proc);
  layer_add_child(window_layer, horizontal_ruler_layer);

  const uint8_t score_text_rect_height = 36;
  init_text_layer(window_layer, &s_opponent_score_text_layer, 
    bounds.size.h / 4 - score_text_rect_height / 2, score_text_rect_height, 
    0, FONT_KEY_LECO_36_BOLD_NUMBERS);
  init_text_layer(window_layer, &s_my_score_text_layer, 
    bounds.size.h / 4 * 3 - score_text_rect_height / 2, score_text_rect_height, 
    0, FONT_KEY_LECO_36_BOLD_NUMBERS);

  text_layer_set_text(s_my_score_text_layer, score->score_1_text);
  text_layer_set_text(s_opponent_score_text_layer, score->score_2_text);

  layer_add_child(window_layer, custom_status_bar);
}

static void main_window_unload(Window *window) {
  custom_status_bar_layer_destroy(custom_status_bar);

  text_layer_destroy(s_my_score_text_layer);
  text_layer_destroy(s_opponent_score_text_layer);

  layer_destroy(horizontal_ruler_layer);

  text_layer_destroy(s_whole_score_text_layer);

  free(top_bar_info);
  free(score);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (score->score_2 < MAX_SCORE) {
    score->score_2 += 1;
    snprintf(score->score_2_text, sizeof(score->score_2_text), "%d", score->score_2);
    text_layer_set_text(s_opponent_score_text_layer, score->score_2_text);

    time(&score->timestamp);

    // send_msg_num_retries = 0;
    reset_bg_color_callback(NULL);

    send_msg(CMD_SET_SCORE_VAL);

    persist_write_int(S_SCORE_2_KEY, score->score_2);
    persist_write_int(S_TIMESTAMP_KEY, score->timestamp);
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (score->score_1 < MAX_SCORE) {
    score->score_1 += 1;
    snprintf(score->score_1_text, sizeof(score->score_1_text), "%d", score->score_1);
    text_layer_set_text(s_my_score_text_layer, score->score_1_text);

    time(&score->timestamp);

    reset_bg_color_callback(NULL);

    // send_msg_num_retries = 0;
    send_msg(CMD_SET_SCORE_VAL);

    persist_write_int(S_SCORE_1_KEY, score->score_1);
    persist_write_int(S_TIMESTAMP_KEY, score->timestamp);
  }
}

static void up_long_click_handler_down(ClickRecognizerRef recognizer, void *context) {
  if (score->score_2 > MIN_SCORE) {
    score->score_2 -= 1;
    snprintf(score->score_2_text, sizeof(score->score_2_text), "%d", score->score_2);
    text_layer_set_text(s_opponent_score_text_layer, score->score_2_text);

    time(&score->timestamp);

    reset_bg_color_callback(NULL);

    // send_msg_num_retries = 0;
    send_msg(CMD_SET_SCORE_VAL);

    persist_write_int(S_SCORE_2_KEY, score->score_2);
    persist_write_int(S_TIMESTAMP_KEY, score->timestamp);
  }
}

static void down_long_click_handler_down(ClickRecognizerRef recognizer, void *context) {
  if (score->score_1 > MIN_SCORE) {
    score->score_1 -= 1;
    snprintf(score->score_1_text, sizeof(score->score_1_text), "%d", score->score_1);
    text_layer_set_text(s_my_score_text_layer, score->score_1_text);

    // send_msg_num_retries = 0;
    time(&score->timestamp);

    reset_bg_color_callback(NULL);

    send_msg(CMD_SET_SCORE_VAL);

    persist_write_int(S_SCORE_1_KEY, score->score_1);
    persist_write_int(S_TIMESTAMP_KEY, score->timestamp);
  }
}

/**
 * Select button single click should resend last score.
 */
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  reset_bg_color_callback(NULL);
  send_msg(CMD_SET_SCORE_VAL);
}

/**
 * Select button long click should reset the score.
 */
static void select_long_click_handler_down(ClickRecognizerRef recognizer, void *context) {
  score->score_1 = 0;
  score->score_2 = 0;

  snprintf(score->score_1_text, sizeof(score->score_1_text), "%d", score->score_1);
  snprintf(score->score_2_text, sizeof(score->score_2_text), "%d", score->score_2);

  text_layer_set_text(s_my_score_text_layer, score->score_1_text);
  text_layer_set_text(s_opponent_score_text_layer, score->score_2_text);

  time(&score->timestamp);

  reset_bg_color_callback(NULL);

  // send_msg_num_retries = 0;
  send_msg(CMD_SET_SCORE_VAL);

  persist_write_int(S_SCORE_1_KEY, score->score_1);
  persist_write_int(S_SCORE_2_KEY, score->score_2);
  persist_write_int(S_TIMESTAMP_KEY, score->timestamp);
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
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  static char time_buffer[TIME_BUFF_SIZE];

  // Read time into a string buffer
  strftime(time_buffer, TIME_BUFF_SIZE, "%H:%M", tick_time);

  custom_status_bar_layer_set_text(custom_status_bar, CSB_TEXT_CENTER, time_buffer);
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

static void init_status_bar() {
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
}

static void init() {
  init_score();
  init_status_bar();

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
