import wiitools as w
import sys

# ------------------------------------------------------------------ helpers --

_pass = 0
_fail = 0
_errors = []


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
    msg = repr(err)
    print("[ERR] " + name + ": " + msg)
    _errors.append((name, msg))


def section(title):
    print("")
    print("=== " + title + " ===")


def wait_a(timeout=6000):
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


def test_filesystem(): #don't work 
    
    section("Filesystem (fatInitDefault / write_file / read_file / remove)")
    try:
        r = w.fatInitDefault()
        ok("fatInitDefault", r)
    except Exception as e:
        fail("fatInitDefault", e)
        return
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

    #w.usleep(1000000)
    loaded = False
    try:
        wh = w.png_load_embedded()
        ok("png_load_embedded", wh)
        loaded = True
    except Exception as e:
        fail("png_load_embedded (evtl. nicht gelinkt)", e)

    print("press A to continue...")
    while True:
        w.update()
        if w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0):
            break

    print("22222222222222222222222222222")
    #w.usleep(1000000)
    if not loaded:
        print("not loaded")
        #w.usleep(1000000)
        for p in ["usb:/data/test.png", "usb1:/data/test.png"]:
            print(p)
            #w.usleep(1000000)
            try:
                wh = w.png_load(p)
                ok("png_load (" + p + ")", wh)
                loaded = True
                break
            except Exception as e:
                fail("png_load (" + p + ")", e)

    if not loaded:
        print("  -> kein PNG verfuegbar, PNG-Tests uebersprungen")
        #w.usleep(1000000)
        return

    #w.usleep(1000000)
    try:
        ok("png_info", w.png_info())
    except Exception as e:
        fail("png_info", e)

    #w.usleep(1000000)
    try: 
        ok("text_length('test',2)", w.text_length("test", 2))
    except Exception as e:
        fail("text_length", e)

    
    try:
        w.png_save("usb:/wiitools_png_selftest.png")
        ok("png_save")
        # Readback-Check: Datei wieder einlesen und PNG-Header pruefen
        data = w.read_file("usb:/wiitools_png_selftest.png")
        if data and len(data) > 8 and data[:4] == b"\x89PNG":
            ok("png_save readback", str(len(data)) + "B, PNG-Header ok")
        else:
            fail("png_save readback", "Datei leer/kein PNG")
        w.remove("usb:/wiitools_png_selftest.png")
    except Exception as e:
        fail("png_save", e)

    try:
        w.png_load_named("usb:/data/test.png", "named_test")
        ok("png_load_named")
    except Exception as e:
        fail("png_load_named (SD)", e)

    #w.usleep(1000000)
    try:
        w.png_use("__default__")
        ok("png_use('__default__')")
    except Exception as e:
        fail("png_use", e)

    #w.usleep(1000000)
    try:
        info = w.png_info()
        ok("png_info nach png_use", info)
    except Exception as e:
        fail("png_info nach png_use", e)

    #w.usleep(1000000)
    try:
        w.png_unload()
        ok("png_unload")
    except Exception as e:
        fail("png_unload", e)

    #w.usleep(1000000)
    try:
        w.png_unload_all()
        ok("png_unload_all")
    except Exception as e:
        fail("png_unload_all", e)


def test_network():
    section("Netzwerk (IsNetReady / get_local_ip)")
    print("(Warte auf Netzwerk... A druecken zum Ueberspringen)")
    try:
        # Schritt 1: warten bis net_init() fertig ist
        while not w.IsNetReady():
            if w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0):
                fail("IsNetReady", "vom Benutzer uebersprungen")
                return
            w.update()
        ok("IsNetReady", 1)

        # Schritt 2: warten bis IP wirklich zugewiesen ist (wie net_gethostip() in C)
        ip = ""
        for _ in range(50):   # max 5 Sek (50 x 100ms)
            ip = w.get_local_ip()
            if ip and ip != "0.0.0.0":
                break
            w.usleep(100000)
        ok("get_local_ip", repr(ip))
    except Exception as e:
        fail("get_local_ip", e)
        return


def test_curl():
    section("curl_get / curl_post / curl_request")
    if not w.IsNetReady():
        fail("curl (Netz nicht bereit)", "IsNetReady()==0")
        return
    print("(kann 10-30 Sek. dauern...)")

    try:
        r = w.curl_get(
            "https://example.com",
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
            "https://example.com",
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
            "https://example.com",
            timeout_ms=10000,
            verify_peer=0,
            verify_host=0,
        )
        ok("curl_request HEAD ok",     r.get("ok"))
        ok("curl_request HEAD status", r.get("status"))
    except Exception as e:
        fail("curl_request HEAD", e)


_HTTP_BASE = "http://192.168.15.152:8000"

def test_curl_http():
    section("curl HTTP lokal (" + _HTTP_BASE + ")")
    if not w.IsNetReady():
        fail("curl_http (Netz nicht bereit)", "IsNetReady()==0")
        return

    try:
        r = w.curl_get(
            _HTTP_BASE + "/",
            timeout_ms=10000,
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
            _HTTP_BASE + "/",
            b"hello=world",
            timeout_ms=10000,
            verify_peer=0,
            verify_host=0,
        )
        ok("curl_post ok",     r.get("ok"))
        ok("curl_post status", r.get("status"))
        ok("curl_post body",   str(len(r.get("body", b""))) + "B")
        if r.get("error"):
            print("      error: " + str(r.get("error")))
    except Exception as e:
        fail("curl_post", e)

    try:
        r = w.curl_request(
            "HEAD",
            _HTTP_BASE + "/",
            timeout_ms=10000,
            verify_peer=0,
            verify_host=0,
        )
        ok("curl_request HEAD ok",     r.get("ok"))
        ok("curl_request HEAD status", r.get("status"))
        if r.get("error"):
            print("      error: " + str(r.get("error")))
    except Exception as e:
        fail("curl_request HEAD", e)


# ----------------------------------------------------- Schreibtest (SD / USB) --

def _test_write(device):
    """Schreibt eine Datei auf <device>:/, liest sie zurueck und loescht sie."""
    section("Schreibtest auf " + device + ":/")
    path = device + ":/wiitools_write_test.txt"
    payload = b"wiitools write test 0123456789\n"
    try:
        n = w.write_file(path, payload)
        ok("write_file", str(n) + "B")
    except Exception as e:
        fail("write_file (" + device + ")", e)
        return
    try:
        got = w.read_file(path)
        if got == payload:
            ok("read_file (Inhalt korrekt)", str(len(got)) + "B")
        else:
            fail("read_file Inhalt falsch", repr(got[:32]))
    except Exception as e:
        fail("read_file (" + device + ")", e)
    try:
        w.remove(path)
        ok("remove")
    except Exception as e:
        fail("remove (" + device + ")", e)


def test_write_sd():
    _test_write("sd")


def test_write_usb():
    _test_write("usb")


# --------------------------------------------------------- Ergebnis-Zusammenfassung --

def _print_results():
    print("")
    print("================================")
    print("  Ergebnis:")
    print("    OK:   " + str(_pass))
    print("    FAIL: " + str(_fail))
    if _errors:
        print("")
        print("  Fehler:")
        for name, msg in _errors:
            print("  [ERR] " + name + ":")
            print("        " + msg)
    print("================================")
    print("(A druecken um zurueck zum Menu)")
    wait_a()


def _run_single(fn):
    global _pass, _fail, _errors
    _pass = 0
    _fail = 0
    _errors = []
    print("")
    print("================================")
    print("  " + fn.__name__)
    print("================================")
    fn()
    _print_results()


def run_all_tests():
    global _pass, _fail, _errors
    _pass = 0
    _fail = 0
    _errors = []
    print("")
    print("================================")
    print("  Alle Tests")
    print("================================")
    test_constants()
    test_video()
    test_filesystem()
    test_wpad()
    test_pad()
    test_png()
    test_network()
    test_curl()
    test_curl_http()
    _print_results()


# ----------------------------------------------------------------------- menu --

# Hier neue Eintraege einfuegen: (Label, Funktion)
MENU = [
    ("Alle Tests",    run_all_tests),
    ("constants",     lambda: _run_single(test_constants)),
    ("video",         lambda: _run_single(test_video)),
    ("filesystem",    lambda: _run_single(test_filesystem)),
    ("wpad",          lambda: _run_single(test_wpad)),
    ("pad",           lambda: _run_single(test_pad)),
    ("png",           lambda: _run_single(test_png)),
    ("network",       lambda: _run_single(test_network)),
    ("curl (https)",  lambda: _run_single(test_curl)),
    ("curl (http)",   lambda: _run_single(test_curl_http)),
    ("write sd",      lambda: _run_single(test_write_sd)),
    ("write usb",     lambda: _run_single(test_write_usb)),
    # ("mein Test",   my_test_fn),
]


def show_menu(sel):
    print("")
    print("========= TEST MENU =========")
    for i, (label, _) in enumerate(MENU):
        prefix = " -> " if i == sel else "    "
        print(prefix + label)
    print("")
    print("  UP/DOWN  navigieren")
    print("  A        ausfuehren")
    print("  HOME     beenden")
    print("=============================")


def menu_loop():
    sel = 7
    show_menu(sel)
    while True:
        w.update()

        if w.WPAD_ButtonsDown(w.WPAD_BUTTON_HOME, 0):
            print("Beende.")
            break

        if w.WPAD_ButtonsDown(w.WPAD_BUTTON_UP, 0):
            sel = (sel - 1) % len(MENU)
            show_menu(sel)

        elif w.WPAD_ButtonsDown(w.WPAD_BUTTON_DOWN, 0):
            sel = (sel + 1) % len(MENU)
            show_menu(sel)

        elif w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0):
            label, fn = MENU[sel]
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
            show_menu(sel)


# -------------------------------------------------------------------------- start

print("test.py gestartet")
menu_loop()
print("test.py fertig")
