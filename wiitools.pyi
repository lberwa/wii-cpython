__version__ = "0.2"

WPAD_BUTTON_2 = 0
WPAD_BUTTON_1 = 0
WPAD_BUTTON_B = 0
WPAD_BUTTON_A = 0
WPAD_BUTTON_MINUS = 0
WPAD_BUTTON_HOME = 0
WPAD_BUTTON_LEFT = 0
WPAD_BUTTON_RIGHT = 0
WPAD_BUTTON_DOWN = 0
WPAD_BUTTON_UP = 0
WPAD_BUTTON_PLUS = 0

WPAD_NUNCHUK_BUTTON_Z = 0
WPAD_NUNCHUK_BUTTON_C = 0

WPAD_CLASSIC_BUTTON_UP = 0
WPAD_CLASSIC_BUTTON_LEFT = 0
WPAD_CLASSIC_BUTTON_ZR = 0
WPAD_CLASSIC_BUTTON_X = 0
WPAD_CLASSIC_BUTTON_A = 0
WPAD_CLASSIC_BUTTON_Y = 0
WPAD_CLASSIC_BUTTON_B = 0
WPAD_CLASSIC_BUTTON_ZL = 0
WPAD_CLASSIC_BUTTON_FULL_R = 0
WPAD_CLASSIC_BUTTON_PLUS = 0
WPAD_CLASSIC_BUTTON_HOME = 0
WPAD_CLASSIC_BUTTON_MINUS = 0
WPAD_CLASSIC_BUTTON_FULL_L = 0
WPAD_CLASSIC_BUTTON_DOWN = 0
WPAD_CLASSIC_BUTTON_RIGHT = 0

WPAD_GUITAR_HERO_3_BUTTON_STRUM_UP = 0
WPAD_GUITAR_HERO_3_BUTTON_YELLOW = 0
WPAD_GUITAR_HERO_3_BUTTON_GREEN = 0
WPAD_GUITAR_HERO_3_BUTTON_BLUE = 0
WPAD_GUITAR_HERO_3_BUTTON_RED = 0
WPAD_GUITAR_HERO_3_BUTTON_ORANGE = 0
WPAD_GUITAR_HERO_3_BUTTON_PLUS = 0
WPAD_GUITAR_HERO_3_BUTTON_MINUS = 0
WPAD_GUITAR_HERO_3_BUTTON_STRUM_DOWN = 0

WPAD_DATA_BUTTONS = 0
WPAD_THRESH_DEFAULT_BUTTONS = 0
WPAD_CHAN_ALL = 0
WPAD_CHAN_0 = 0
WPAD_CHAN_1 = 0
WPAD_CHAN_2 = 0
WPAD_CHAN_3 = 0
WPAD_BALANCE_BOARD = 0
WPAD_MAX_WIIMOTES = 0

def fatInitDefault():
    ...

def VIDEO_WaitVSync():
    ...

def remove(path):
    ...

def png_load(path):
    ...

def png_load_named(path, name):
    ...

def png_load_embedded():
    ...

def png_load_embedded_named(name):
    ...

def png_use(name):
    ...

def png_info():
    ...

def png_show(x, y):
    ...

def png_show_region(sx, sy, sw, sh, dx, dy):
    ...

def png_show_region_scaled(sx, sy, sw, sh, dx, dy, dw, dh):
    ...

def png_show_scaled(dx, dy, dw, dh):
    ...

def png_show_fullscreen():
    ...

def png(screen_x, screen_y, image_x, image_y, image_width, image_height, screen_width, screen_height):
    ...

def png_quad(x1, y1, x2, y2, x3, y3, x4, y4, image_x, image_y, image_width, image_height):
    ...

def png_save(path):
    ...

def png_unload():
    ...

def png_unload_all():
    ...

def draw_rect(x, y, width, height, rgba, angle_deg):
    ...

def draw_circle(x, y, radius, rgba):
    ...

def draw_oval(x, y, width, height, rgba, angle_deg):
    ...

def render_text(x, y, text, size, shadow, rgba, angle_deg):
    ...

def text_length(text, size):
    ...

def WPAD_ButtonsUp(button, chan):
    ...

def WPAD_ButtonsDown(button, chan):
    ...

def WPAD_ButtonsHeld(button, chan):
    ...

def WPAD_ControlSpeaker(chan, enable):
    ...

def WPAD_ReadEvent(chan):
    ...

def WPAD_DroppedEvents(chan):
    ...

def WPAD_Flush(chan):
    ...

def WPAD_ReadPending(chan):
    ...

def WPAD_SetDataFormat(chan, fmt):
    ...

def WPAD_SetMotionPlus(chan, enable):
    ...

def WPAD_SetVRes(chan, xres, yres):
    ...

def WPAD_GetStatus():
    ...

def WPAD_Probe(chan):
    ...

def WPAD_SetEventBufs(chan, cnt):
    ...

def WPAD_Disconnect(chan):
    ...

def WPAD_IsSpeakerEnabled(chan):
    ...

def WPAD_SendStreamData(chan, data_bytes):
    ...

def WPAD_Shutdown():
    ...

def WPAD_SetIdleTimeout(seconds):
    ...

def WPAD_SetPowerButtonCallback(enable):
    ...

def WPAD_SetBatteryDeadCallback(enable):
    ...

def WPAD_GetPowerButtonEvent():
    ...

def WPAD_GetBatteryDeadEvent():
    ...

def WPAD_Rumble(chan, status):
    ...

def WPAD_SetIdleThresholds(chan, btns, ir, accel, js, wb, mp):
    ...

def WPAD_EncodeData(flag, pcm_bytes, out_len):
    ...

def WPAD_Data(chan):
    ...

def WPAD_BatteryLevel(chan):
    ...

def WPAD_IR(chan):
    ...

def WPAD_Orientation(chan):
    ...

def WPAD_GForce(chan):
    ...

def WPAD_Accel(chan):
    ...

def WPAD_Expansion(chan):
    ...

def terminal_init():
    ...

def rendering_init():
    ...

def curl_request(method, url, data=None, headers=None, timeout_ms=30000,
                 verify_peer=0, verify_host=0, follow_redirects=1,
                 user_agent=None, ca_file=None):
    ...

def curl_get(url, headers=None, timeout_ms=30000, verify_peer=0,
             verify_host=0, follow_redirects=1, user_agent=None, ca_file=None):
    ...

def curl_post(url, data, headers=None, timeout_ms=30000, verify_peer=0,
              verify_host=0, follow_redirects=1, user_agent=None, ca_file=None):
    ...

def update():
    ...
