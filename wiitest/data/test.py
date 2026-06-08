import wiitools as w
import sys

# ------------------------------------------------------------------ helpers --

_pass = 0
_fail = 0


def ok(name, info=""):
    global _pass
    _pass += 1
    if info != "":
        print("[OK ] " + name + ": " + str(info))
    else:
        print("[OK ] " + name)


def fail(name, err):
    global _fail
    _fail += 1
    print("[ERR] " + name + ": " + repr(err))


def section(title):
    print("")
    print("=== " + title + " ===")


def wait_a(timeout=600):
    """Wartet auf A-Knopf (max timeout Frames). Gibt True bei Druck zurueck."""
    for _ in range(timeout):
        w.update()
        if w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0):
            return True
    return False


# --------------------------------------------------------------- test groups --

def test_constants():
    section("Konstanten & Version")
    try:
        ok("__version__", w.__version__)
    except Exception as e:
        fail("__version__", e)
    try:
        ok("width",  w.width)
        ok("height", w.height)
    except Exception as e:
        fail("width/height", e)
    pairs = [
        ("WPAD_BUTTON_A",     w.WPAD_BUTTON_A),
        ("WPAD_BUTTON_B",     w.WPAD_BUTTON_B),
        ("WPAD_BUTTON_1",     w.WPAD_BUTTON_1),
        ("WPAD_BUTTON_2",     w.WPAD_BUTTON_2),
        ("WPAD_BUTTON_HOME",  w.WPAD_BUTTON_HOME),
        ("WPAD_BUTTON_PLUS",  w.WPAD_BUTTON_PLUS),
        ("WPAD_BUTTON_MINUS", w.WPAD_BUTTON_MINUS),
        ("WPAD_BUTTON_UP",    w.WPAD_BUTTON_UP),
        ("WPAD_BUTTON_DOWN",  w.WPAD_BUTTON_DOWN),
        ("WPAD_BUTTON_LEFT",  w.WPAD_BUTTON_LEFT),
        ("WPAD_BUTTON_RIGHT", w.WPAD_BUTTON_RIGHT),
        ("WPAD_NUNCHUK_BUTTON_Z", w.WPAD_NUNCHUK_BUTTON_Z),
        ("WPAD_NUNCHUK_BUTTON_C", w.WPAD_NUNCHUK_BUTTON_C),
        ("WPAD_CLASSIC_BUTTON_A", w.WPAD_CLASSIC_BUTTON_A),
        ("WPAD_CLASSIC_BUTTON_B", w.WPAD_CLASSIC_BUTTON_B),
        ("WPAD_CHAN_0",        w.WPAD_CHAN_0),
        ("WPAD_CHAN_ALL",      w.WPAD_CHAN_ALL),
        ("WPAD_MAX_WIIMOTES", w.WPAD_MAX_WIIMOTES),
    ]
    for name, val in pairs:
        ok(name, val)


def test_video():
    section("Video / Timing")
    try:
        w.VIDEO_WaitVSync()
        ok("VIDEO_WaitVSync")
    except Exception as e:
        fail("VIDEO_WaitVSync", e)
    try:
        w.usleep(10000)
        ok("usleep(10000)")
    except Exception as e:
        fail("usleep", e)


def test_filesystem():
    section("Filesystem (fatInitDefault / write_file / read_file / remove)")
    try:
        r = w.fatInitDefault()
        ok("fatInitDefault", r)
    except Exception as e:
        fail("fatInitDefault", e)
        return

    path = "sd:/wiitools_selftest_tmp.bin"
    data = b"wiitools test 0123456789\n"

    try:
        n = w.write_file(path, data)
        ok("write_file", n)
    except Exception as e:
        fail("write_file", e)
        return

    try:
        got = w.read_file(path)
        if got == data:
            ok("read_file (Inhalt korrekt)", len(got))
        else:
            fail("read_file Inhalt falsch", repr(got[:32]))
    except Exception as e:
        fail("read_file", e)

    try:
        r = w.remove(path)
        ok("remove", r)
    except Exception as e:
        fail("remove", e)

    try:
        w.read_file(path)
        fail("read_file nach remove (sollte Exception)", "kein Fehler")
    except OSError:
        ok("read_file nach remove wirft OSError")
    except Exception as e:
        fail("read_file nach remove unerwarteter Fehler", e)


def test_wpad():
    section("WPAD (Wiimote)")

    try:
        r = w.WPAD_GetStatus()
        ok("WPAD_GetStatus", r)
    except Exception as e:
        fail("WPAD_GetStatus", e)

    try:
        r, t = w.WPAD_Probe(0)
        ok("WPAD_Probe(0)", "ret=" + str(r) + " type=" + str(t))
    except Exception as e:
        fail("WPAD_Probe", e)

    try:
        ok("WPAD_BatteryLevel(0)", w.WPAD_BatteryLevel(0))
    except Exception as e:
        fail("WPAD_BatteryLevel", e)

    try:
        w.WPAD_SetIdleTimeout(300)
        ok("WPAD_SetIdleTimeout(300)")
    except Exception as e:
        fail("WPAD_SetIdleTimeout", e)

    try:
        w.WPAD_SetPowerButtonCallback(1)
        ok("WPAD_SetPowerButtonCallback(1)")
    except Exception as e:
        fail("WPAD_SetPowerButtonCallback", e)

    try:
        w.WPAD_SetBatteryDeadCallback(1)
        ok("WPAD_SetBatteryDeadCallback(1)")
    except Exception as e:
        fail("WPAD_SetBatteryDeadCallback", e)

    try:
        ok("WPAD_GetPowerButtonEvent", w.WPAD_GetPowerButtonEvent())
    except Exception as e:
        fail("WPAD_GetPowerButtonEvent", e)

    try:
        ok("WPAD_GetBatteryDeadEvent", w.WPAD_GetBatteryDeadEvent())
    except Exception as e:
        fail("WPAD_GetBatteryDeadEvent", e)

    try:
        ok("WPAD_SetEventBufs(0, 4)", w.WPAD_SetEventBufs(0, 4))
    except Exception as e:
        fail("WPAD_SetEventBufs", e)

    try:
        ok("WPAD_DroppedEvents(0)", w.WPAD_DroppedEvents(0))
    except Exception as e:
        fail("WPAD_DroppedEvents", e)

    try:
        ok("WPAD_Flush(0)", w.WPAD_Flush(0))
    except Exception as e:
        fail("WPAD_Flush", e)

    try:
        ok("WPAD_ReadPending(0)", w.WPAD_ReadPending(0))
    except Exception as e:
        fail("WPAD_ReadPending", e)

    try:
        ok("WPAD_SetDataFormat(0,0)", w.WPAD_SetDataFormat(0, 0))
    except Exception as e:
        fail("WPAD_SetDataFormat", e)

    try:
        ok("WPAD_SetVRes(0,640,480)", w.WPAD_SetVRes(0, 640, 480))
    except Exception as e:
        fail("WPAD_SetVRes", e)

    try:
        ok("WPAD_SetIdleThresholds", w.WPAD_SetIdleThresholds(0, 40, 10, 10, 10, 10, 10))
    except Exception as e:
        fail("WPAD_SetIdleThresholds", e)

    try:
        ok("WPAD_IsSpeakerEnabled(0)", w.WPAD_IsSpeakerEnabled(0))
    except Exception as e:
        fail("WPAD_IsSpeakerEnabled", e)

    try:
        ok("WPAD_ControlSpeaker(0,0)", w.WPAD_ControlSpeaker(0, 0))
    except Exception as e:
        fail("WPAD_ControlSpeaker", e)

    try:
        ret, raw = w.WPAD_ReadEvent(0)
        ok("WPAD_ReadEvent(0)", "ret=" + str(ret) + " raw=" + str(len(raw)) + "B")
    except Exception as e:
        fail("WPAD_ReadEvent", e)

    try:
        raw = w.WPAD_IR(0)
        ok("WPAD_IR(0)", str(len(raw)) + "B")
    except Exception as e:
        fail("WPAD_IR", e)

    try:
        raw = w.WPAD_Orientation(0)
        ok("WPAD_Orientation(0)", str(len(raw)) + "B")
    except Exception as e:
        fail("WPAD_Orientation", e)

    try:
        raw = w.WPAD_GForce(0)
        ok("WPAD_GForce(0)", str(len(raw)) + "B")
    except Exception as e:
        fail("WPAD_GForce", e)

    try:
        raw = w.WPAD_Accel(0)
        ok("WPAD_Accel(0)", str(len(raw)) + "B")
    except Exception as e:
        fail("WPAD_Accel", e)

    try:
        raw = w.WPAD_Expansion(0)
        ok("WPAD_Expansion(0)", str(len(raw)) + "B")
    except Exception as e:
        fail("WPAD_Expansion", e)

    try:
        d = w.WPAD_Data(0)
        if d is None:
            ok("WPAD_Data(0)", "None (kein Controller)")
        else:
            ok("WPAD_Data(0)", str(len(d)) + "B")
    except Exception as e:
        fail("WPAD_Data", e)

    try:
        w.update()
        ok("WPAD_ButtonsDown(A,0)", w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0))
        ok("WPAD_ButtonsUp(A,0)",   w.WPAD_ButtonsUp(w.WPAD_BUTTON_A,   0))
        ok("WPAD_ButtonsHeld(A,0)", w.WPAD_ButtonsHeld(w.WPAD_BUTTON_A, 0))
    except Exception as e:
        fail("WPAD_Buttons*", e)

    try:
        ok("WPAD_Rumble(0,1)", w.WPAD_Rumble(0, 1))
        w.usleep(200000)
        ok("WPAD_Rumble(0,0)", w.WPAD_Rumble(0, 0))
    except Exception as e:
        fail("WPAD_Rumble", e)

    try:
        ok("WPAD_SetMotionPlus(0,0)", w.WPAD_SetMotionPlus(0, 0))
    except Exception as e:
        fail("WPAD_SetMotionPlus", e)

    try:
        pcm = bytes(40)  # 20 x s16 Nullen
        status, enc = w.WPAD_EncodeData(0, pcm, 20)
        ok("WPAD_EncodeData", "status=" + str(len(status)) + "B enc=" + str(len(enc)) + "B")
    except Exception as e:
        fail("WPAD_EncodeData", e)


def test_pad():
    section("PAD (GameCube-Controller)")
    try:
        ok("PAD_Init", w.PAD_Init())
    except Exception as e:
        fail("PAD_Init", e)
        return

    try:
        ok("PAD_Sync",     w.PAD_Sync())
        ok("PAD_ScanPads", w.PAD_ScanPads())
    except Exception as e:
        fail("PAD_Sync/ScanPads", e)

    try:
        ret, raw = w.PAD_Read()
        ok("PAD_Read", "ret=" + str(ret) + " raw=" + str(len(raw)) + "B")
    except Exception as e:
        fail("PAD_Read", e)

    try:
        raw = w.PAD_Clamp()
        ok("PAD_Clamp", str(len(raw)) + "B")
    except Exception as e:
        fail("PAD_Clamp", e)

    try:
        ok("PAD_StickX(0)",    w.PAD_StickX(0))
        ok("PAD_StickY(0)",    w.PAD_StickY(0))
        ok("PAD_SubStickX(0)", w.PAD_SubStickX(0))
        ok("PAD_SubStickY(0)", w.PAD_SubStickY(0))
        ok("PAD_TriggerL(0)",  w.PAD_TriggerL(0))
        ok("PAD_TriggerR(0)",  w.PAD_TriggerR(0))
    except Exception as e:
        fail("PAD_Stick*/Trigger*", e)

    try:
        ok("PAD_ButtonsDown(1,0)",  w.PAD_ButtonsDown(0x0001, 0))
        ok("PAD_ButtonsUp(1,0)",    w.PAD_ButtonsUp(0x0001, 0))
        ok("PAD_ButtonsHeld(1,0)",  w.PAD_ButtonsHeld(0x0001, 0))
    except Exception as e:
        fail("PAD_Buttons*", e)

    try:
        ok("PAD_Reset(0)",          w.PAD_Reset(0))
        ok("PAD_Recalibrate(mask)", w.PAD_Recalibrate(0xF0000000))
    except Exception as e:
        fail("PAD_Reset/Recalibrate", e)

    try:
        w.PAD_SetSpec(0)
        ok("PAD_SetSpec(0)")
    except Exception as e:
        fail("PAD_SetSpec", e)

    try:
        w.PAD_ControlMotor(0, 0)
        ok("PAD_ControlMotor(0,0)")
    except Exception as e:
        fail("PAD_ControlMotor", e)


def test_png():
    section("PNG (load / info / save / unload)")

    loaded = False
    try:
        wh = w.png_load_embedded()
        ok("png_load_embedded", wh)
        loaded = True
    except Exception as e:
        fail("png_load_embedded (evtl. nicht gelinkt)", e)

    if not loaded:
        for p in ["sd:/data/test.png", "sd:/test.png"]:
            try:
                wh = w.png_load(p)
                ok("png_load (" + p + ")", wh)
                loaded = True
                break
            except Exception as e:
                fail("png_load (" + p + ")", e)

    if not loaded:
        print("  -> kein PNG verfuegbar, PNG-Tests uebersprungen")
        return

    try:
        ok("png_info", w.png_info())
    except Exception as e:
        fail("png_info", e)

    try:
        ok("text_length('test',2)", w.text_length("test", 2))
    except Exception as e:
        fail("text_length", e)

    try:
        w.png_save("sd:/wiitools_png_selftest.png")
        ok("png_save")
        w.remove("sd:/wiitools_png_selftest.png")
    except Exception as e:
        fail("png_save", e)

    try:
        w.png_load_named("sd:/data/test.png", "named_test")
        ok("png_load_named")
    except Exception as e:
        fail("png_load_named (SD)", e)

    try:
        w.png_use("__default__")
        ok("png_use('__default__')")
    except Exception as e:
        fail("png_use", e)

    try:
        info = w.png_info()
        ok("png_info nach png_use", info)
    except Exception as e:
        fail("png_info nach png_use", e)

    try:
        w.png_unload()
        ok("png_unload")
    except Exception as e:
        fail("png_unload", e)

    try:
        w.png_unload_all()
        ok("png_unload_all")
    except Exception as e:
        fail("png_unload_all", e)


def test_network():
    section("Netzwerk (IsNetReady / get_local_ip)")
    try:
        r = w.IsNetReady()
        ok("IsNetReady", r)
    except Exception as e:
        fail("IsNetReady", e)
        return

    try:
        ip = w.get_local_ip()
        ok("get_local_ip", repr(ip))
    except Exception as e:
        fail("get_local_ip", e)


def test_curl():
    section("curl_get / curl_post / curl_request")
    print("(kann 10-30 Sek. dauern...)")

    try:
        r = w.curl_get(
            "http://example.com",
            timeout_ms=15000,
            verify_peer=0,
            verify_host=0,
        )
        ok("curl_get ok",     r.get("ok"))
        ok("curl_get status", r.get("status"))
        ok("curl_get body",   str(len(r.get("body", b""))) + "B")
        if r.get("error"):
            print("      error: " + str(r.get("error")))
    except Exception as e:
        fail("curl_get", e)

    try:
        r = w.curl_post(
            "http://httpbin.org/post",
            b"hello=world",
            timeout_ms=15000,
            verify_peer=0,
            verify_host=0,
        )
        ok("curl_post ok",     r.get("ok"))
        ok("curl_post status", r.get("status"))
        ok("curl_post body",   str(len(r.get("body", b""))) + "B")
    except Exception as e:
        fail("curl_post", e)

    try:
        r = w.curl_request(
            "HEAD",
            "http://example.com",
            timeout_ms=10000,
            verify_peer=0,
            verify_host=0,
        )
        ok("curl_request HEAD ok",     r.get("ok"))
        ok("curl_request HEAD status", r.get("status"))
    except Exception as e:
        fail("curl_request HEAD", e)


# --------------------------------------------------------- wiitools Gesamttest --

def run_wiitools_tests():
    global _pass, _fail
    _pass = 0
    _fail = 0

    print("")
    print("================================")
    print("  wiitools Test-Suite")
    print("================================")

    test_constants()
    test_video()
    test_filesystem()
    test_wpad()
    test_pad()
    test_png()
    test_network()
    test_curl()

    print("")
    print("================================")
    print("  Ergebnis:")
    print("    OK:   " + str(_pass))
    print("    FAIL: " + str(_fail))
    print("================================")
    print("(A druecken um zurueck zum Menu)")
    wait_a()


# ----------------------------------------------------------------------- menu --

# Hier neue Eintraege einfuegen: (Label, Funktion)
MENU = [
    ("wiitools testen",  run_wiitools_tests),
    # ("mein naechster Test", my_test_fn),
]

MENU_BUTTONS = [
    w.WPAD_BUTTON_A,
    w.WPAD_BUTTON_B,
    w.WPAD_BUTTON_PLUS,
    w.WPAD_BUTTON_MINUS,
    w.WPAD_BUTTON_1,
    w.WPAD_BUTTON_2,
]


def show_menu():
    print("")
    print("========= TEST MENU =========")
    for i, (label, _) in enumerate(MENU):
        if i < len(MENU_BUTTONS):
            btn = ["A", "B", "+", "-", "1", "2"][i]
            print("  " + btn + "  ->  " + label)
    print("  HOME -> Beenden")
    print("=============================")


def menu_loop():
    show_menu()
    while True:
        w.update()

        if w.WPAD_ButtonsDown(w.WPAD_BUTTON_HOME, 0):
            print("Beende.")
            break

        for i, (label, fn) in enumerate(MENU):
            if i >= len(MENU_BUTTONS):
                break
            if w.WPAD_ButtonsDown(MENU_BUTTONS[i], 0):
                print("")
                print("> " + label)
                try:
                    fn()
                except Exception as e:
                    t, v, _tb = sys.exc_info()
                    print("[CRASH] " + repr(e))
                    print("  type: " + str(t))
                    print("  str:  " + str(v))
                    print("(A druecken um weiter)")
                    wait_a()
                show_menu()
                break


# -------------------------------------------------------------------------- start

print("test.py gestartet")
menu_loop()
print("test.py fertig")
