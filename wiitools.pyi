__version__ = "0.2"

# Screen dimensions (pixels) set at module init based on detected TV mode.
width: int
height: int

# --------------- Button constants ---------------
WPAD_BUTTON_2: int
WPAD_BUTTON_1: int
WPAD_BUTTON_B: int
WPAD_BUTTON_A: int
WPAD_BUTTON_MINUS: int
WPAD_BUTTON_HOME: int
WPAD_BUTTON_LEFT: int
WPAD_BUTTON_RIGHT: int
WPAD_BUTTON_DOWN: int
WPAD_BUTTON_UP: int
WPAD_BUTTON_PLUS: int

WPAD_NUNCHUK_BUTTON_Z: int
WPAD_NUNCHUK_BUTTON_C: int

WPAD_CLASSIC_BUTTON_UP: int
WPAD_CLASSIC_BUTTON_LEFT: int
WPAD_CLASSIC_BUTTON_ZR: int
WPAD_CLASSIC_BUTTON_X: int
WPAD_CLASSIC_BUTTON_A: int
WPAD_CLASSIC_BUTTON_Y: int
WPAD_CLASSIC_BUTTON_B: int
WPAD_CLASSIC_BUTTON_ZL: int
WPAD_CLASSIC_BUTTON_FULL_R: int
WPAD_CLASSIC_BUTTON_PLUS: int
WPAD_CLASSIC_BUTTON_HOME: int
WPAD_CLASSIC_BUTTON_MINUS: int
WPAD_CLASSIC_BUTTON_FULL_L: int
WPAD_CLASSIC_BUTTON_DOWN: int
WPAD_CLASSIC_BUTTON_RIGHT: int

WPAD_GUITAR_HERO_3_BUTTON_STRUM_UP: int
WPAD_GUITAR_HERO_3_BUTTON_YELLOW: int
WPAD_GUITAR_HERO_3_BUTTON_GREEN: int
WPAD_GUITAR_HERO_3_BUTTON_BLUE: int
WPAD_GUITAR_HERO_3_BUTTON_RED: int
WPAD_GUITAR_HERO_3_BUTTON_ORANGE: int
WPAD_GUITAR_HERO_3_BUTTON_PLUS: int
WPAD_GUITAR_HERO_3_BUTTON_MINUS: int
WPAD_GUITAR_HERO_3_BUTTON_STRUM_DOWN: int

WPAD_DATA_BUTTONS: int
WPAD_THRESH_DEFAULT_BUTTONS: int
WPAD_CHAN_ALL: int
WPAD_CHAN_0: int
WPAD_CHAN_1: int
WPAD_CHAN_2: int
WPAD_CHAN_3: int
WPAD_BALANCE_BOARD: int
WPAD_MAX_WIIMOTES: int

# --------------- GameCube controller (PAD) constants ---------------
PAD_BUTTON_LEFT: int
PAD_BUTTON_RIGHT: int
PAD_BUTTON_DOWN: int
PAD_BUTTON_UP: int
PAD_TRIGGER_Z: int
PAD_TRIGGER_R: int
PAD_TRIGGER_L: int
PAD_BUTTON_A: int
PAD_BUTTON_B: int
PAD_BUTTON_X: int
PAD_BUTTON_Y: int
PAD_BUTTON_MENU: int
PAD_BUTTON_START: int
PAD_CHAN_ALL: int
PAD_CHAN0: int
PAD_CHAN1: int
PAD_CHAN2: int
PAD_CHAN3: int
PAD_CHANMAX: int

# --------------- Filesystem ---------------

def fatInitDefault() -> int:
    """Mount the SD card (and USB). Returns 0 on failure, non-zero on success."""
    ...

def remove(path: str) -> int:
    """Delete a file at *path*. Returns 0 on success, -1 on error."""
    ...

def read_file(path: str) -> bytes:
    """Read the entire file at *path* and return its contents as bytes."""
    ...

def write_file(path: str, data: bytes | bytearray | str) -> int:
    """Write *data* to *path* (binary mode). Returns the number of bytes written."""
    ...

# --------------- Video / timing ---------------

def VIDEO_WaitVSync() -> None:
    """Block until the next vertical sync interrupt (one frame, ~16.7 ms at 60 Hz)."""
    ...

def usleep(microseconds: int) -> None:
    """Sleep for the given number of microseconds."""
    ...

# --------------- Terminal / rendering init ---------------

def terminal_init() -> None:
    """Initialize the debug text console (terminal_print mode, no GX)."""
    ...

def rendering_init() -> None:
    """Initialize GX hardware rendering with triple-buffering. Required before draw_*, png_show_*, render_text."""
    ...

def rendering_adopt(mode_ptr: int, fb_ptr: int, empty_q: int = 0) -> None:
    """Adopt a host-initialized rendering context instead of calling rendering_init().

    For use when this module runs as an overlay under a host (e.g. ffCavEX) that
    has already set up VIDEO, framebuffers and GX. Sets the module's internal
    graphics globals to the host's values so drawing works without re-initializing
    hardware (which would conflict with the host).

    Args:
        mode_ptr: pointer value of the host's GXRModeObj* (host global: screenMode).
        fb_ptr:   pointer value of the host's current back buffer (host global: frame).
                  Must be non-zero.
        empty_q:  host's frame_empty message-queue handle, or 0 (default) to let the
                  host perform the flip -- recommended, avoids VSync conflicts.

    Note:
        Do NOT also call rendering_init() -- that would re-run GX_Init and break the
        host's FIFO. Use either rendering_init() (standalone) or rendering_adopt()
        (as overlay), never both.
    """
    ...

# --------------- Main loop ---------------

def WPAD_ScanPads() -> None:
    """Read all Wiimote button states into the internal buffer for this frame.
    Must be called before WPAD_ButtonsDown/Up/Held return current values."""
    ...

def set_screen_size(w: int, h: int) -> None:
    """Set a logical screen resolution with automatic letterboxing.

    All subsequent draw_rect / draw_circle / png_show / render_text coordinates
    are interpreted in the logical w×h space and scaled to the physical display
    (802×460 or whatever the TV reports) with black bars to preserve aspect ratio.

    Example — snake field 480×480 centred on the Wii screen::

        wiitools.set_screen_size(480, 480)
        # now (0,0)-(480,480) fills a 460×460 region centred horizontally

    Pass (0, 0) to revert to the native physical resolution."""
    ...

def render_update() -> None:
    """Flush the GX framebuffer and wait for VSync — without scanning pads.
    Use when you handle WPAD_ScanPads() yourself at a different point in the loop."""
    ...

def update() -> None:
    """Call WPAD_ScanPads() then render_update() — the standard one-call-per-frame shortcut."""
    ...

# --------------- PNG image management ---------------

def png_load(path: str) -> tuple[int, int]:
    """Load a PNG from the SD card into the slot named '__default__'. Returns (width, height)."""
    ...

def png_load_named(path: str, name: str) -> tuple[int, int]:
    """Load a PNG from the SD card and store it under *name*. Returns (width, height)."""
    ...

def png_load_embedded() -> tuple[int, int]:
    """Decode the PNG linked as *test_png* symbol into '__default__'. Returns (width, height)."""
    ...

def png_load_embedded_named(name: str) -> tuple[int, int]:
    """Decode the PNG linked as *test_png* symbol and store it under *name*. Returns (width, height)."""
    ...

def png_use(name: str) -> None:
    """Select a previously loaded PNG by *name* as the active image for subsequent png_* calls."""
    ...

def png_info() -> tuple[int, int] | None:
    """Return (width, height) of the active PNG, or None if none is loaded."""
    ...

def png_show(x: int, y: int) -> None:
    """Blit the active PNG at screen position (x, y) at its original size."""
    ...

def png_show_region(sx: int, sy: int, sw: int, sh: int, dx: int, dy: int) -> None:
    """Blit a sub-region (sx, sy, sw, sh) of the active PNG to screen position (dx, dy)."""
    ...

def png_show_region_scaled(sx: int, sy: int, sw: int, sh: int,
                           dx: int, dy: int, dw: int, dh: int) -> None:
    """Blit a sub-region of the active PNG to a scaled destination rectangle on screen."""
    ...

def png_show_scaled(dx: int, dy: int, dw: int, dh: int) -> None:
    """Blit the entire active PNG scaled to the destination rectangle (dx, dy, dw, dh)."""
    ...

def png_show_fullscreen() -> None:
    """Stretch the active PNG to fill the entire framebuffer."""
    ...

def png(screen_x: int, screen_y: int,
        image_x: int, image_y: int, image_width: int, image_height: int,
        screen_width: int, screen_height: int) -> None:
    """Generic blit: draw a region of the active PNG scaled to an arbitrary screen rectangle."""
    ...

def png_quad(x1: float, y1: float, x2: float, y2: float,
             x3: float, y3: float, x4: float, y4: float,
             image_x: int, image_y: int, image_width: int, image_height: int) -> None:
    """Draw a region of the active PNG mapped onto an arbitrary quadrilateral (GX mode only)."""
    ...

def png_save(path: str) -> int:
    """Encode the active PNG as a PNG file and write it to *path*. Returns 0 on success."""
    ...

def png_unload() -> None:
    """Free the active PNG from memory and deselect it."""
    ...

def png_unload_all() -> None:
    """Free all loaded PNG images from memory."""
    ...

# --------------- Shape drawing (GX, requires rendering_init) ---------------

def draw_rect(x: int, y: int, width: int, height: int,
              rgba: tuple[int, int, int, int], angle_deg: float) -> None:
    """Draw a filled, optionally rotated rectangle. *rgba* is (r, g, b, a) 0-255 each."""
    ...

def draw_circle(x: int, y: int, radius: int,
                rgba: tuple[int, int, int, int]) -> None:
    """Draw a filled circle centred at (x, y). *rgba* is (r, g, b, a) 0-255 each."""
    ...

def draw_oval(x: int, y: int, width: int, height: int,
              rgba: tuple[int, int, int, int], angle_deg: float) -> None:
    """Draw a filled, optionally rotated ellipse inside the bounding box (x, y, width, height)."""
    ...

def render_text(x: int, y: int, text: str, size: int, shadow: int,
                rgba: tuple[int, int, int, int], angle_deg: float) -> None:
    """Render bitmap text using the 8×8 font. *size* is the pixel scale factor; *shadow* adds a drop shadow."""
    ...

def text_length(text: str, size: int) -> int:
    """Return the pixel width of *text* rendered at the given scale factor."""
    ...

# --------------- Wiimote / Wii Remote (WPAD) ---------------

def WPAD_Init() -> int:
    """Initialize the Wiimote subsystem. Called automatically at module import."""
    ...

def WPAD_ButtonsDown(button: int, chan: int) -> int:
    """Return 1 if *button* was just pressed this frame on *chan* (use WPAD_CHAN_ALL for any controller)."""
    ...

def WPAD_ButtonsUp(button: int, chan: int) -> int:
    """Return 1 if *button* was just released this frame on *chan*."""
    ...

def WPAD_ButtonsHeld(button: int, chan: int) -> int:
    """Return 1 if *button* is currently held on *chan*."""
    ...

class WPADState:
    """Named-tuple snapshot of all button states. Fields are True/False.
    Returned by WPAD_ButtonsDown_all / WPAD_ButtonsUp_all / WPAD_ButtonsHeld_all.

    Example:
        s = WPAD_ButtonsDown_all(WPAD_CHAN_0)
        if s.WPAD_BUTTON_A: ...
    """
    WPAD_BUTTON_2:             bool
    WPAD_BUTTON_1:             bool
    WPAD_BUTTON_B:             bool
    WPAD_BUTTON_A:             bool
    WPAD_BUTTON_MINUS:         bool
    WPAD_BUTTON_HOME:          bool
    WPAD_BUTTON_LEFT:          bool
    WPAD_BUTTON_RIGHT:         bool
    WPAD_BUTTON_DOWN:          bool
    WPAD_BUTTON_UP:            bool
    WPAD_BUTTON_PLUS:          bool
    WPAD_NUNCHUK_BUTTON_Z:     bool
    WPAD_NUNCHUK_BUTTON_C:     bool
    WPAD_CLASSIC_BUTTON_UP:    bool
    WPAD_CLASSIC_BUTTON_LEFT:  bool
    WPAD_CLASSIC_BUTTON_ZR:    bool
    WPAD_CLASSIC_BUTTON_X:     bool
    WPAD_CLASSIC_BUTTON_A:     bool
    WPAD_CLASSIC_BUTTON_Y:     bool
    WPAD_CLASSIC_BUTTON_B:     bool
    WPAD_CLASSIC_BUTTON_ZL:    bool
    WPAD_CLASSIC_BUTTON_FULL_R:bool
    WPAD_CLASSIC_BUTTON_PLUS:  bool
    WPAD_CLASSIC_BUTTON_HOME:  bool
    WPAD_CLASSIC_BUTTON_MINUS: bool
    WPAD_CLASSIC_BUTTON_FULL_L:bool
    WPAD_CLASSIC_BUTTON_DOWN:  bool
    WPAD_CLASSIC_BUTTON_RIGHT: bool
    buttons_raw:               int

def WPAD_ButtonsDown_all(chan: int) -> WPADState:
    """Return a WPADState with True for every button just pressed this frame on *chan*."""
    ...

def WPAD_ButtonsUp_all(chan: int) -> WPADState:
    """Return a WPADState with True for every button just released this frame on *chan*."""
    ...

def WPAD_ButtonsHeld_all(chan: int) -> WPADState:
    """Return a WPADState with True for every button currently held on *chan*."""
    ...

def WPAD_ControlSpeaker(chan: int, enable: int) -> int:
    """Enable (1) or disable (0) the Wiimote speaker on *chan*."""
    ...

def WPAD_ReadEvent(chan: int) -> tuple[int, bytes]:
    """Read one queued event from *chan*. Returns (ret_code, raw_WPADData_bytes)."""
    ...

def WPAD_DroppedEvents(chan: int) -> int:
    """Return the number of dropped events on *chan* since the last call."""
    ...

def WPAD_Flush(chan: int) -> int:
    """Flush the event queue for *chan*."""
    ...

def WPAD_ReadPending(chan: int) -> int:
    """Return the number of pending events for *chan* (no Python callback support)."""
    ...

def WPAD_SetDataFormat(chan: int, fmt: int) -> int:
    """Set the data format for *chan* (e.g. WPAD_FMT_BTNS_ACC_IR)."""
    ...

def WPAD_SetMotionPlus(chan: int, enable: int) -> int:
    """Enable (1) or disable (0) the MotionPlus extension on *chan*."""
    ...

def WPAD_SetVRes(chan: int, xres: int, yres: int) -> int:
    """Set the IR virtual screen resolution for *chan*."""
    ...

def WPAD_GetStatus() -> int:
    """Return the global WPAD status bitmask."""
    ...

def WPAD_Probe(chan: int) -> tuple[int, int]:
    """Probe *chan* for a connected controller. Returns (ret_code, controller_type)."""
    ...

def WPAD_SetEventBufs(chan: int, cnt: int) -> int:
    """Assign *cnt* internal event buffers (1..8) to *chan*."""
    ...

def WPAD_Disconnect(chan: int) -> int:
    """Disconnect the Wiimote on *chan*."""
    ...

def WPAD_IsSpeakerEnabled(chan: int) -> int:
    """Return 1 if the speaker is enabled on *chan*, 0 otherwise."""
    ...

def WPAD_SendStreamData(chan: int, data_bytes: bytes) -> int:
    """Send raw PCM stream data to the Wiimote speaker on *chan*."""
    ...

def WPAD_Shutdown() -> None:
    """Shut down the WPAD subsystem and disconnect all controllers."""
    ...

def WPAD_SetIdleTimeout(seconds: int) -> None:
    """Set the idle timeout in seconds before a Wiimote auto-disconnects."""
    ...

def WPAD_SetPowerButtonCallback(enable: int) -> None:
    """Enable (1) or disable (0) the internal power-button callback."""
    ...

def WPAD_SetBatteryDeadCallback(enable: int) -> None:
    """Enable (1) or disable (0) the internal battery-dead callback."""
    ...

def WPAD_GetPowerButtonEvent() -> int:
    """Return and clear the channel number of the last power-button event (-999 = none)."""
    ...

def WPAD_GetBatteryDeadEvent() -> int:
    """Return and clear the channel number of the last battery-dead event (-999 = none)."""
    ...

def WPAD_Rumble(chan: int, status: int) -> int:
    """Start (1) or stop (0) rumble on *chan*."""
    ...

def WPAD_SetIdleThresholds(chan: int, btns: int, ir: int,
                           accel: int, js: int, wb: int, mp: int) -> int:
    """Set per-channel idle-detection thresholds for buttons, IR, accel, joystick, balance board, and MotionPlus."""
    ...

def WPAD_EncodeData(flag: int, pcm_bytes: bytes, out_len: int) -> tuple[bytes, bytes]:
    """Encode raw s16 PCM samples for the Wiimote speaker. Returns (status_bytes, encoded_bytes)."""
    ...

def WPAD_Data(chan: int) -> bytes | None:
    """Return the raw WPADData struct bytes for *chan*, or None if unavailable."""
    ...

def WPAD_BatteryLevel(chan: int) -> int:
    """Return the battery level (0-4) for *chan*."""
    ...

def WPAD_IR(chan: int) -> bytes:
    """Return the raw ir_t struct bytes for *chan* (pointer position, validity, etc.)."""
    ...

def WPAD_Orientation(chan: int) -> bytes:
    """Return the raw orient_t struct bytes for *chan* (pitch, roll, yaw in degrees)."""
    ...

def WPAD_GForce(chan: int) -> bytes:
    """Return the raw gforce_t struct bytes for *chan* (x, y, z g-force as floats)."""
    ...

def WPAD_Accel(chan: int) -> bytes:
    """Return the raw vec3w_t struct bytes for *chan* (raw accelerometer x/y/z)."""
    ...

def WPAD_Expansion(chan: int) -> bytes:
    """Return the raw expansion_t struct bytes for *chan* (Nunchuk, Classic, etc.)."""
    ...

# --------------- GameCube controller (PAD) ---------------

def PAD_Init() -> int:
    """Initialize the GameCube controller subsystem."""
    ...

def PAD_Sync() -> int:
    """Wait for the PAD driver to synchronize with the hardware."""
    ...

def PAD_ScanPads() -> int:
    """Read all connected GameCube controllers into an internal buffer."""
    ...

def PAD_Read() -> tuple[int, bytes]:
    """Read raw PADStatus data for all channels. Returns (connected_mask, raw_status_bytes)."""
    ...

def PAD_Reset(mask: int) -> int:
    """Reset the controllers indicated by the channel bitmask *mask*."""
    ...

def PAD_Recalibrate(mask: int) -> int:
    """Recalibrate the controllers indicated by the channel bitmask *mask*."""
    ...

def PAD_Clamp() -> bytes:
    """Clamp all PADStatus values to valid ranges and return the raw status bytes."""
    ...

def PAD_ControlMotor(chan: int, cmd: int) -> None:
    """Control the rumble motor on GameCube controller *chan*."""
    ...

def PAD_SetSpec(spec: int) -> None:
    """Set the PAD hardware specification version."""
    ...

def PAD_ButtonsUp(button: int, chan: int) -> int:
    """Return 1 if *button* was just released on *chan* (chan < 0 = any controller)."""
    ...

def PAD_ButtonsDown(button: int, chan: int) -> int:
    """Return 1 if *button* was just pressed on *chan* (chan < 0 = any controller)."""
    ...

def PAD_ButtonsHeld(button: int, chan: int) -> int:
    """Return 1 if *button* is currently held on *chan* (chan < 0 = any controller)."""
    ...

def PAD_StickX(pad: int) -> int:
    """Return the main analog stick X value (-128..127) for *pad*."""
    ...

def PAD_StickY(pad: int) -> int:
    """Return the main analog stick Y value (-128..127) for *pad*."""
    ...

def PAD_SubStickX(pad: int) -> int:
    """Return the C-stick X value (-128..127) for *pad*."""
    ...

def PAD_SubStickY(pad: int) -> int:
    """Return the C-stick Y value (-128..127) for *pad*."""
    ...

def PAD_TriggerL(pad: int) -> int:
    """Return the left analog trigger value (0..255) for *pad*."""
    ...

def PAD_TriggerR(pad: int) -> int:
    """Return the right analog trigger value (0..255) for *pad*."""
    ...

# --------------- Off-screen surfaces ---------------
#
# Workflow (pygame analogy):
#   surf = pygame.Surface((w, h))        → surface_new("name", w, h)
#   surf.fill((r,g,b,a))                 → surface_fill("name", (r,g,b,a))
#   pygame.draw.rect(surf, ...)          → surface_set_target("name"); draw_rect(...)
#   font.render → surf.blit(text, pos)   → render_text(x, y, ...)  [while target active]
#   surf.blit(other_surf, pos)           → surface_blit("other", "name", x, y)
#   surface_clear_target()               # back to screen
#   screen.blit(surf, (x, y))           → blit("name", x, y)
#
# All draw_*, render_text, and png_show* calls are automatically redirected
# into the active surface when surface_set_target() is active.

def surface_new(name: str, width: int, height: int) -> tuple[int, int]:
    """Create a blank transparent RGBA surface.
    *width* and *height* are the size in pixels.
    *name* is the key used to refer to this surface in all other surface_* and blit() calls.
    Returns (width, height) on success."""
    ...

def surface_set_target(name: str) -> None:
    """Redirect all subsequent draw_*, render_text, and png_show* calls into the named surface.
    Call surface_clear_target() to go back to rendering on the screen."""
    ...

def surface_clear_target() -> None:
    """Stop rendering into a surface and restore normal screen rendering."""
    ...

def surface_fill(name: str, rgba: tuple[int, int, int, int]) -> None:
    """Fill the entire surface with a solid colour. *rgba* is (r, g, b, a) 0-255 each."""
    ...

def surface_get_size(name: str) -> tuple[int, int] | None:
    """Return (width, height) of the named surface, or None if it does not exist."""
    ...

def blit(name: str, x: int, y: int) -> None:
    """Draw the named surface to the screen at (x, y).
    Always renders to the screen regardless of any active render target.
    Rebuilds the GX texture automatically so all recent changes are visible."""
    ...

def surface_blit(src: str, dst: str | None, x: int, y: int,
                 sx: int = 0, sy: int = 0,
                 sw: int = -1, sh: int = -1) -> None:
    """Copy (a region of) surface *src* into surface *dst* at position (x, y).
    Pass dst=None to blit into the currently active render target.
    *sx*, *sy*, *sw*, *sh* define the source region (defaults to the full surface)."""
    ...

# --------------- Network / HTTP ---------------

def IsNetReady() -> int:
    """Return 1 when the background net_init() thread has finished, 0 while still initializing."""
    ...

def get_local_ip() -> str:
    """Return the primary local IPv4 address as a string, or '' if unavailable."""
    ...

def curl_request(method: str, url: str, data: bytes | str | None = None,
                 headers: list[str] | tuple[str, ...] | None = None,
                 timeout_ms: int = 30000, verify_peer: int = 0,
                 verify_host: int = 0, follow_redirects: int = 1,
                 user_agent: str | None = None,
                 ca_file: str | None = None) -> dict:
    """Perform an HTTP request with the given *method*.
    Returns a dict with keys: ok, status, body (bytes), headers (bytes), error, url, curl_code."""
    ...

def curl_get(url: str, headers: list[str] | tuple[str, ...] | None = None,
             timeout_ms: int = 30000, verify_peer: int = 0,
             verify_host: int = 0, follow_redirects: int = 1,
             user_agent: str | None = None,
             ca_file: str | None = None) -> dict:
    """Perform an HTTP GET request. Returns the same dict as curl_request."""
    ...

def curl_post(url: str, data: bytes | str,
              headers: list[str] | tuple[str, ...] | None = None,
              timeout_ms: int = 30000, verify_peer: int = 0,
              verify_host: int = 0, follow_redirects: int = 1,
              user_agent: str | None = None,
              ca_file: str | None = None) -> dict:
    """Perform an HTTP POST request with *data* as the body. Returns the same dict as curl_request."""
    ...
