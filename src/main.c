
#include <pebble.h>


const int STATUS_RESET_TIME = 5000;

static Window *window = NULL;
static TextLayer *statusTextLayer = NULL;
static ActionBarLayer *actionBar = NULL;

static GBitmap* bitmapRing = NULL;
static GBitmap* bitmapSilence = NULL;

static AppTimer* statusResetTimer = NULL;

static bool callbacks_registered = false;

const char * const STATUS_READY = "Ready";
const char * const STATUS_DONE = "Done";
const char * const STATUS_RINGING = "Ringing";
const char * const STATUS_SILENCING = "Silencing";
const char * const STATUS_FAILED = "Failed :(";

enum {
    CMD_KEY = 0x0, // TUPLE_INTEGER
};

enum {
    CMD_START = 0x01,
    CMD_STOP = 0x02,
};

static void send_cmd(uint8_t cmd);
void start_reset_timer();

// Modify these common button handlers

void up_single_click_handler(ClickRecognizerRef recognizer, void *context)
{
    text_layer_set_text(statusTextLayer, STATUS_RINGING);
    send_cmd( CMD_START );
}


void down_single_click_handler(ClickRecognizerRef recognizer, void *context)
{
    text_layer_set_text(statusTextLayer, STATUS_SILENCING);
    send_cmd( CMD_STOP );
}

// App message callbacks
static void app_send_failed(DictionaryIterator *iterator, AppMessageResult reason, void *context)
{
    text_layer_set_text(statusTextLayer, STATUS_FAILED);
    start_reset_timer();
}

static void app_received_msg(DictionaryIterator* received, void* context)
{
    vibes_short_pulse();
    text_layer_set_text(statusTextLayer, STATUS_DONE);
    start_reset_timer();
}

bool register_callbacks()
{
    if (callbacks_registered) {
        app_message_deregister_callbacks();
        callbacks_registered = false;
    }

    if (!callbacks_registered)
    {
        app_message_register_outbox_failed( app_send_failed );
        app_message_register_inbox_received( app_received_msg );
        callbacks_registered = true;
    }

    return callbacks_registered;
}

void handle_timer(void *data)
{
    text_layer_set_text(statusTextLayer, STATUS_READY);
}


void cancel_reset_timer()
{
    if (statusResetTimer != NULL)
    {
        app_timer_cancel(statusResetTimer);
        statusResetTimer = NULL;
    }
}

void start_reset_timer()
{
    cancel_reset_timer();

    if (statusResetTimer == NULL)
    {
        statusResetTimer = app_timer_register(STATUS_RESET_TIME, handle_timer, NULL);
    }
}

static void send_cmd(uint8_t cmd)
{
    start_reset_timer();

    Tuplet value = TupletInteger(CMD_KEY, cmd);

    DictionaryIterator *iter = NULL;
    app_message_outbox_begin(&iter);
    if (iter == NULL)
        return;

    dict_write_tuplet(iter, &value);
    dict_write_end(iter);

    app_message_outbox_send();
    //app_message_out_release();
}


// Standard app initialisation

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_single_click_handler);
}


static void window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    GRect window_layer_frame = layer_get_frame(window_layer);

    int16_t aThird = window_layer_frame.size.h / 3;

    GRect textFrame;
    textFrame.origin.x = 0;
    textFrame.origin.y = 0;
    textFrame.size.w = window_layer_frame.size.w;
    textFrame.size.h = aThird;

    textFrame.origin.y = aThird;
    statusTextLayer = text_layer_create( textFrame );
    text_layer_set_text(statusTextLayer, STATUS_READY);
    text_layer_set_text_alignment(statusTextLayer, GTextAlignmentLeft);
    text_layer_set_font(statusTextLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    layer_add_child(window_layer, text_layer_get_layer(statusTextLayer));


    // Initialize the action bar:
    actionBar = action_bar_layer_create();
    // Associate the action bar with the window:
    action_bar_layer_add_to_window(actionBar, window);
    // Set the click config provider:
    action_bar_layer_set_click_config_provider(actionBar, click_config_provider);

    // Set the icons:
    bitmapRing = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RING_ICON);
    bitmapSilence = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SILENCE_ICON);

    action_bar_layer_set_icon(actionBar, BUTTON_ID_UP, bitmapRing);
    action_bar_layer_set_icon(actionBar, BUTTON_ID_DOWN, bitmapSilence);

    register_callbacks();

}

static void window_unload(Window *window)
{
    cancel_reset_timer();

    if (actionBar)
    {
        action_bar_layer_destroy(actionBar);
        actionBar = NULL;
    }

    if (bitmapRing)
    {
        gbitmap_destroy( bitmapRing );
        bitmapRing = NULL;
    }
    if (bitmapSilence)
    {
        gbitmap_destroy( bitmapSilence );
        bitmapSilence = NULL;
    }

    if (statusTextLayer)
    {
        text_layer_destroy(statusTextLayer);
        statusTextLayer = NULL;
    }
}


void init(void)
{
    window = window_create();

    window_set_window_handlers(window, (WindowHandlers) {
            .load = window_load,
            .unload = window_unload,
    });

    window_stack_push(window, true /* Animated */);

    const int inbound_size = 256;
    const int outbound_size = 256;
    app_message_open(inbound_size, outbound_size);
}

void deinit(void)
{
    window_destroy(window);
    window = NULL;
}

int main(void)
{
    init();

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

    app_event_loop();

    deinit();

    return 0;
}

