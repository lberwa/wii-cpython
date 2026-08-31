import wiitools as w
import sys
import types

# keyword, operator, copyreg, reprlib, warnings, copy sind jetzt frozen
# in libpython3.15.a — kein Stub mehr noetig.

# datetime: C-Modul _datetime unter dem Namen 'datetime' verfuegbar machen
# (wird von _zoneinfo via PyCapsule gebraucht)

w.init()

try:
    import _datetime
    sys.modules.setdefault('datetime', _datetime)
except ImportError:
    pass

import runpy
import warnings

# ------------------------------------------------------------------ helpers --

_pass = 0
_fail = 0
_errors = []

# Die WPAD-Roh-Event-API (WPAD_SetEventBufs/Flush/ReadPending/ReadEvent) ist
# jetzt mit Vorbedingungs-Checks abgesichert: ohne initialisiertes WPAD bzw.
# ohne verbundenen Controller gibt es eine saubere Python-Exception statt DSI.
# ABER: bei tatsaechlich verbundenem Controller ruft sie den fragilen libogc-
# Event-Pfad (__wpad_calc_data) auf, der intern crashen kann. Daher hier aus;
# auf echter Hardware mit Wiimote bewusst aktivierbar.
_WPAD_RAW_EVENTS = False


def ok(name, info=""):
    global _pass
    _pass += 1
    if info != "":
        print("[OK ] " + name + ": " + str(info))
    else:
        print("[OK ] " + name)


def _error_hint(name, msg):
    text = (name + " " + msg).lower()

    if "cannot release un-acquired lock" in text:
        return ("Lock-Freigabe ohne Besitz. Das passiert typischerweise in "
                "threading.Condition/Event/RLock und deutet auf einen Fehler "
                "im Wii-Threading-Pfad hin.")
    if "cannot wait on un-acquired lock" in text:
        return ("Condition.wait() glaubt, dass der aktuelle Thread das Lock "
                "nicht mehr besitzt. Das ist meist ein Besitz-/RLock-Problem.")
    if "timeout waiting for workers" in text:
        return ("Die Worker sind nicht sauber bis zum done-Event gekommen. "
                "Bitte auf die letzten [OK]-Zeilen vor dem Fehler achten.")
    if "tls mismatch" in text:
        return ("threading.local() hat pro Thread keinen stabilen eigenen "
                "Speicher geliefert.")
    if "worker ident == main ident" in text or "nicht eindeutig" in text:
        return ("Die Thread-IDs sehen nicht nach echten, getrennten Threads aus.")
    if "threading import" in text:
        return ("Ein Basis-Modul fuer threading fehlt noch oder wurde nicht "
                "korrekt frozen eingebaut.")
    return ""


def fail(name, err, detail=""):
    global _fail
    _fail += 1
    msg = repr(err)
    print("[ERR] " + name + ": " + msg)
    hint = _error_hint(name, msg)
    if detail:
        print("      " + detail)
    if hint:
        print("      Hinweis: " + hint)
    _errors.append({
        "name": name,
        "msg": msg,
        "detail": detail,
        "hint": hint,
    })


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

wait_a()
# --------------------------------------------------------------- test groups --
def test_builtin_modules():
    modules = [
        "_asyncio",
        "atexit",
        "array",
        "binascii",
        "_bisect",
        "_bz2",
        "cmath",
        "_codecs",
        "_collections",
        "_csv",
        "_datetime",
        "_dbm",
        "_elementtree",
        "errno",
        "faulthandler",
        "fcntl",
        "_functools",
        "gc",
        "_gdbm",
        "grp",
        "hashlib",
        "_heapq",
        "hmac",
        "_interpchannels",
        "_interpqueues",
        "_interpreters",
        "itertools",
        "_json",
        "_locale",
        "_lsprof",
        "_lzma",
        "math",
        "md5",
        "mmap",
        "_opcode",
        "_operator",
        "_pickle",
        "posix",
        "_posixsubprocess",
        "pwd",
        "pyexpat",
        "_queue",
        "_random",
        "readline",
        "_scproxy",
        "select",
        "sha1",
        "sha2",
        "sha3",
        "signal",
        "socket",
        "_ssl",
        "_stat",
        "_statistics",
        "_struct",
        "_suggestions",
        "symtable",
        "_sysconfig",
        "syslog",
        "termios",
        "_thread",
        "time",
        "_tkinter",
        "_tracemalloc",
        "_types",
        "_typing",
        "unicodedata",
        "_uuid",
        "_weakref",
        "_winapi",
        "xxlimited",
        "xxlimited_35",
        "xxsubtype",
        "zlib",
        "_zoneinfo"
    ]

    
    f = open("sd:/out_builtin.txt", "w")

    for mod in modules:
        try:
            __import__(mod)
            f.write("OK: %s\n" % mod)
        except Exception as e:
            f.write("FEHLT: %s - %s\n" % (mod, e))

    f.close()

    """for mod in modules:
        try:
            __import__(mod)
        except Exception as e:
            print("FEHLT:", mod, "-", e)
"""

def test_frozen_modules():
    import _imp

    frozen = set(_imp._frozen_module_names())

    modules = [
        "importlib._bootstrap",
        "importlib._bootstrap_external",
        "zipimport",
        "abc",
        "codecs",
        "io",
        "_collections_abc",
        "_sitebuiltins",
        "_genericpath",
        "_stat",
        "_osx_support",
        "genericpath",
        "posixpath",
        "ntpath",
        "os",
        "site",
        "stat",
        "importlib.util",
        "importlib.machinery",
        "runpy",
        "__hello__",
        "__hello_alias__",
        "__phello__",
        "__phello__.ham",
        "__phello__.spam",
        "__phello__.spam.ham",
    ]

    for module in modules:
        if module not in frozen:
            print("FEHLT:", module)


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
        ok("WPAD_DroppedEvents(0)", w.WPAD_DroppedEvents(0))
    except Exception as e:
        fail("WPAD_DroppedEvents", e)

    if _WPAD_RAW_EVENTS:
        try:
            ok("WPAD_SetEventBufs(0, 4)", w.WPAD_SetEventBufs(0, 4))
        except Exception as e:
            fail("WPAD_SetEventBufs", e)
        try:
            ok("WPAD_Flush(0)", w.WPAD_Flush(0))
        except Exception as e:
            fail("WPAD_Flush", e)
        try:
            ok("WPAD_ReadPending(0)", w.WPAD_ReadPending(0))
        except Exception as e:
            fail("WPAD_ReadPending", e)
    else:
        print("[--] WPAD Roh-Event-API uebersprungen (_WPAD_RAW_EVENTS=False)")

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

    if _WPAD_RAW_EVENTS:
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

    # Init/Scan erneut (idempotent) + *_all-Zustaende
    try:
        ok("WPAD_Init", w.WPAD_Init())
        ok("WPAD_ScanPads", w.WPAD_ScanPads())
    except Exception as e:
        fail("WPAD_Init/ScanPads", e)
    try:
        st = w.WPAD_ButtonsDown_all(0)
        ok("WPAD_ButtonsDown_all(0)", "WPADState mit " + str(len(st)) + " Feldern")
        w.WPAD_ButtonsUp_all(0)
        w.WPAD_ButtonsHeld_all(0)
        ok("WPAD_ButtonsUp_all/Held_all(0)")
    except Exception as e:
        fail("WPAD_Buttons*_all", e)
    # SendStreamData nur bei verbundenem Controller (sonst sauberer Fehler)
    try:
        r, t = w.WPAD_Probe(0)
        if r == 0:
            ok("WPAD_SendStreamData", w.WPAD_SendStreamData(0, bytes(20)))
        else:
            print("[--] WPAD_SendStreamData uebersprungen (kein Controller)")
    except Exception as e:
        fail("WPAD_SendStreamData", e)


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


def test_sys_power_reset():
    section("SYS Power / Reset (Ereignis-Abfrage)")

    # Direkte Zustandsabfrage - kein Callback noetig
    try:
        ok("SYS_ResetButtonDown", w.SYS_ResetButtonDown())
    except Exception as e:
        fail("SYS_ResetButtonDown", e)

    try:
        ok("SYS_Time", w.SYS_Time())
    except Exception as e:
        fail("SYS_Time", e)

    # Callback-Trampoline aktivieren
    try:
        w.SYS_SetPowerCallback(True)
        ok("SYS_SetPowerCallback(True)")
    except Exception as e:
        fail("SYS_SetPowerCallback", e)
    try:
        w.SYS_SetResetCallback(True)
        ok("SYS_SetResetCallback(True)")
    except Exception as e:
        fail("SYS_SetResetCallback", e)

    # Zaehler frisch abholen (sollten 0 sein)
    try:
        ok("SYS_GetPowerEvent (init)", w.SYS_GetPowerEvent())
        ok("SYS_GetResetEvent (init)", w.SYS_GetResetEvent())
    except Exception as e:
        fail("SYS_Get*Event", e)

    # Interaktiv: ~8s auf POWER/RESET lauschen.
    # Es wird NICHT ausgeschaltet/resettet - nur erkannt und gemeldet.
    print("")
    print("Druecke am Wii: POWER oder RESET")
    print("(Test schaltet NICHT ab)   A = beenden")
    seen_power = 0
    seen_reset = 0
    for _ in range(480):          # ~8 s bei 60 fps
        w.update()
        p = w.SYS_GetPowerEvent()
        r = w.SYS_GetResetEvent()
        if p:
            seen_power += p
            print("  -> POWER erkannt (" + str(seen_power) + ")")
        if r:
            seen_reset += r
            print("  -> RESET erkannt (" + str(seen_reset) + ")")
        if w.SYS_ResetButtonDown():
            print("  -> RESET-Knopf wird gehalten")
        if w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0):
            break

    ok("Power-Ereignisse gesamt", seen_power)
    ok("Reset-Ereignisse gesamt", seen_reset)

    # Callbacks wieder deaktivieren
    try:
        w.SYS_SetPowerCallback(False)
        w.SYS_SetResetCallback(False)
        ok("Callbacks deaktiviert")
    except Exception as e:
        fail("SYS_Set*Callback(False)", e)


def test_conf():
    section("CONF (Wii-Systemeinstellungen, nur lesend)")
    try:
        ok("CONF_Init", w.CONF_Init())
    except Exception as e:
        fail("CONF_Init", e)
    getters = [
        ("CONF_GetShutdownMode",     w.CONF_GetShutdownMode),
        ("CONF_GetIdleLedMode",      w.CONF_GetIdleLedMode),
        ("CONF_GetProgressiveScan",  w.CONF_GetProgressiveScan),
        ("CONF_GetEuRGB60",          w.CONF_GetEuRGB60),
        ("CONF_GetIRSensitivity",    w.CONF_GetIRSensitivity),
        ("CONF_GetSensorBarPosition",w.CONF_GetSensorBarPosition),
        ("CONF_GetPadSpeakerVolume", w.CONF_GetPadSpeakerVolume),
        ("CONF_GetPadMotorMode",     w.CONF_GetPadMotorMode),
        ("CONF_GetSoundMode",        w.CONF_GetSoundMode),
        ("CONF_GetLanguage",         w.CONF_GetLanguage),
        ("CONF_GetScreenSaverMode",  w.CONF_GetScreenSaverMode),
        ("CONF_GetAspectRatio",      w.CONF_GetAspectRatio),
        ("CONF_GetEULA",             w.CONF_GetEULA),
        ("CONF_GetWiiConnect24",     w.CONF_GetWiiConnect24),
        ("CONF_GetRegion",           w.CONF_GetRegion),
        ("CONF_GetArea",             w.CONF_GetArea),
        ("CONF_GetVideo",            w.CONF_GetVideo),
        ("CONF_GetCounterBias",      w.CONF_GetCounterBias),
        ("CONF_GetDisplayOffsetH",   w.CONF_GetDisplayOffsetH),
        ("CONF_GetPadDevices",       w.CONF_GetPadDevices),
    ]
    for name, fn in getters:
        try:
            ok(name, fn())
        except Exception as e:
            fail(name, e)
    try:
        ok("CONF_GetNickName", repr(w.CONF_GetNickName()))
    except Exception as e:
        fail("CONF_GetNickName", e)
    try:
        ok("CONF_GetParentalPassword", repr(w.CONF_GetParentalPassword()))
    except Exception as e:
        fail("CONF_GetParentalPassword", e)
    try:
        ok("CONF_GetParentalAnswer", repr(w.CONF_GetParentalAnswer()))
    except Exception as e:
        fail("CONF_GetParentalAnswer", e)
    # generischer Zugriff auf einen SYSCONF-Eintrag
    try:
        ok("CONF_GetLength(IPL.LNG)", w.CONF_GetLength("IPL.LNG"))
        ok("CONF_GetType(IPL.LNG)",   w.CONF_GetType("IPL.LNG"))
        ok("CONF_Get(IPL.LNG)",       str(len(w.CONF_Get("IPL.LNG"))) + "B")
    except Exception as e:
        fail("CONF_Get*", e)


def test_sys():
    section("SYS (Info + Einstellungen, ohne Reset/Poweroff)")
    # --- reine Getter ---
    for name, fn in [
        ("SYS_Time",                 w.SYS_Time),
        ("SYS_ResetButtonDown",      w.SYS_ResetButtonDown),
        ("SYS_GetHollywoodRevision", w.SYS_GetHollywoodRevision),
        ("SYS_GetCounterBias",       w.SYS_GetCounterBias),
        ("SYS_GetDisplayOffsetH",    w.SYS_GetDisplayOffsetH),
        ("SYS_GetEuRGB60",           w.SYS_GetEuRGB60),
        ("SYS_GetLanguage",          w.SYS_GetLanguage),
        ("SYS_GetProgressiveScan",   w.SYS_GetProgressiveScan),
        ("SYS_GetSoundMode",         w.SYS_GetSoundMode),
        ("SYS_GetVideoMode",         w.SYS_GetVideoMode),
        ("SYS_GetGBSMode",           w.SYS_GetGBSMode),
        ("SYS_GetFontEncoding",      w.SYS_GetFontEncoding),
        ("SYS_GetArena1Size",        w.SYS_GetArena1Size),
        ("SYS_GetArena2Size",        w.SYS_GetArena2Size),
        ("SYS_GetWirelessID(0)",     lambda: w.SYS_GetWirelessID(0)),
    ]:
        try:
            ok(name, fn())
        except Exception as e:
            fail(name, e)

    # --- Setter NUR als Identitaet: liest Wert und schreibt ihn zurueck ---
    #     -> aendert nichts an der Konsole
    print("  (Setter schreiben nur den gelesenen Wert zurueck)")
    try:
        w.SYS_SetCounterBias(w.SYS_GetCounterBias())
        w.SYS_SetDisplayOffsetH(w.SYS_GetDisplayOffsetH())
        w.SYS_SetEuRGB60(w.SYS_GetEuRGB60())
        w.SYS_SetLanguage(w.SYS_GetLanguage())
        w.SYS_SetProgressiveScan(w.SYS_GetProgressiveScan())
        w.SYS_SetSoundMode(w.SYS_GetSoundMode())
        w.SYS_SetVideoMode(w.SYS_GetVideoMode())
        w.SYS_SetGBSMode(w.SYS_GetGBSMode())
        w.SYS_SetWirelessID(0, w.SYS_GetWirelessID(0))
        ok("SYS_Set* (identity, keine Aenderung)")
    except Exception as e:
        fail("SYS_Set* (identity)", e)

    try:
        w.SYS_Report("wiitools test_sys: SYS_Report ok")
        ok("SYS_Report")
    except Exception as e:
        fail("SYS_Report", e)

    # --- Alarm: 0.2s einmalig, auf Ausloesung warten ---
    try:
        h = w.SYS_CreateAlarm()
        ok("SYS_CreateAlarm", h)
        w.SYS_GetAlarmEvent()          # Zaehler leeren
        w.SYS_SetAlarm(h, 0.2)
        fired = 0
        for _ in range(180):           # ~3 s
            w.update()
            fired += w.SYS_GetAlarmEvent()
            if fired:
                break
        ok("SYS_SetAlarm ausgeloest", fired)
        ok("SYS_RemoveAlarm", w.SYS_RemoveAlarm(h))
    except Exception as e:
        fail("SYS_Alarm", e)

    # periodischer Alarm: kurz laufen lassen, dann abbrechen + entfernen
    try:
        h2 = w.SYS_CreateAlarm()
        w.SYS_GetAlarmEvent()
        w.SYS_SetPeriodicAlarm(h2, 0.1, 0.1)
        pfired = 0
        for _ in range(120):        # ~2 s
            w.update()
            pfired += w.SYS_GetAlarmEvent()
            if pfired >= 2:
                break
        ok("SYS_SetPeriodicAlarm", pfired)
        ok("SYS_CancelAlarm", w.SYS_CancelAlarm(h2))
        ok("SYS_RemoveAlarm(2)", w.SYS_RemoveAlarm(h2))
    except Exception as e:
        fail("SYS_PeriodicAlarm", e)

    try:
        w.SYS_STDIO_Report(False)
        ok("SYS_STDIO_Report(False)")
    except Exception as e:
        fail("SYS_STDIO_Report", e)


def test_audio():
    section("AUDIO (DMA/DSP - kein Ton, nur Smoke-Test)")
    # StartDMA/InitDMA werden bewusst NICHT ausgefuehrt:
    # das wuerde den Speicherinhalt als Rauschen ausgeben.
    try:
        w.AUDIO_Init()
        ok("AUDIO_Init")
    except Exception as e:
        fail("AUDIO_Init", e)
        return
    for name, fn in [
        ("AUDIO_GetDSPSampleRate", w.AUDIO_GetDSPSampleRate),
        ("AUDIO_GetDMAEnableFlag", w.AUDIO_GetDMAEnableFlag),
        ("AUDIO_GetDMABytesLeft",  w.AUDIO_GetDMABytesLeft),
        ("AUDIO_GetDMALength",     w.AUDIO_GetDMALength),
        ("AUDIO_GetDMAStartAddr",  w.AUDIO_GetDMAStartAddr),
    ]:
        try:
            ok(name, fn())
        except Exception as e:
            fail(name, e)
    try:
        w.AUDIO_SetDSPSampleRate(w.AUDIO_GetDSPSampleRate())  # identity
        ok("AUDIO_SetDSPSampleRate (identity)")
    except Exception as e:
        fail("AUDIO_SetDSPSampleRate", e)
    try:
        w.AUDIO_RegisterDMACallback(True)
        w.AUDIO_GetDMAEvent()
        ok("AUDIO_RegisterDMACallback(True)")
        w.AUDIO_RegisterDMACallback(False)
        ok("AUDIO_GetDMAEvent", w.AUDIO_GetDMAEvent())
    except Exception as e:
        fail("AUDIO DMA-Callback", e)
    # StopDMA/StartDMA (leerer/gestoppter DMA -> kein Ton). InitDMA-Guard pruefen.
    try:
        w.AUDIO_StopDMA()
        w.AUDIO_StartDMA()
        w.AUDIO_StopDMA()
        ok("AUDIO_Start/StopDMA")
    except Exception as e:
        fail("AUDIO_Start/StopDMA", e)
    try:
        # ungueltige Adresse -> muss ValueError werfen (Guard), kein DSI
        raised = False
        try:
            w.AUDIO_InitDMA(0, 32)
        except ValueError:
            raised = True
        ok("AUDIO_InitDMA lehnt ungueltige Adresse ab", raised)
    except Exception as e:
        fail("AUDIO_InitDMA-Guard", e)


def test_audio_play():
    section("AUDIO abspielen (Sinuston ueber DMA)")
    print("Achtung: gibt echten Ton aus (Lautstaerke pruefen)")
    # kurze Tonleiter A4 - E5 - A5
    for freq in (440, 659, 880):
        try:
            print("  Ton " + str(freq) + " Hz ...")
            w.audio_play_tone(freq, 300, 8000)   # blockierend ~0,3 s
            ok("audio_play_tone(" + str(freq) + " Hz)")
        except Exception as e:
            fail("audio_play_tone(" + str(freq) + ")", e)
            break
    # kurzer leiserer Doppel-Piep
    try:
        w.audio_play_tone(1000, 120, 4000)
        w.audio_play_tone(1500, 120, 4000)
        ok("Doppel-Piep")
    except Exception as e:
        fail("audio_play_tone piep", e)


def test_graphics():
    section("Grafik: surface / draw / png_show / render_text (CPU)")
    WHITE = (255, 255, 255, 255)
    RED   = (255, 0, 0, 255)
    GRN   = (0, 255, 0, 255)
    BLU   = (0, 0, 64, 255)

    # --- Surface-API (CPU-RGBA-Puffer) ---
    try:
        w.surface_new("t_surf", 64, 48)
        ok("surface_new")
        ok("surface_get_size", w.surface_get_size("t_surf"))
        w.surface_fill("t_surf", BLU);        ok("surface_fill")
        w.surface_set_target("t_surf");       ok("surface_set_target")
        w.draw_rect(2, 2, 20, 10, RED, 0.0);  ok("draw_rect")
        w.draw_circle(40, 24, 8, GRN);        ok("draw_circle")
        w.draw_oval(10, 30, 30, 12, WHITE, 0.0); ok("draw_oval")
        try:
            w.render_text(2, 2, "hi", 1, 0, WHITE, 0.0); ok("render_text")
        except Exception as e:
            fail("render_text", e)
        w.surface_clear_target();             ok("surface_clear_target")
        w.surface_blit("t_surf", None, 20, 20); ok("surface_blit")
        w.blit("t_surf", 110, 20);            ok("blit")
    except Exception as e:
        fail("surface/draw", e)

    # --- PNG auf den Bildschirm (CPU-Blit) ---
    try:
        w.png_load_embedded()
        w.png_show(0, 0);                              ok("png_show")
        w.png_show_scaled(0, 0, 100, 60);              ok("png_show_scaled")
        w.png_show_region(0, 0, 32, 32, 10, 10);       ok("png_show_region")
        w.png_show_region_scaled(0, 0, 32, 32, 10, 10, 64, 64); ok("png_show_region_scaled")
        w.png(0, 0, 0, 0, 32, 32, 64, 64);             ok("png (png_draw)")
        w.png_show_fullscreen();                       ok("png_show_fullscreen")
    except Exception as e:
        fail("png_show*", e)
    try:
        w.png_load_embedded_named("test_png"); ok("png_load_embedded_named")
    except Exception as e:
        fail("png_load_embedded_named (optional)", e)
    # png_quad ist GX-Pfad: ohne rendering_init sauber abgelehnt (kein DSI)
    try:
        w.png_quad(0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0, 0, 64, 64)
        ok("png_quad")
    except RuntimeError:
        ok("png_quad (ohne GX korrekt abgelehnt)")
    except Exception as e:
        fail("png_quad", e)

    # --- set_screen_size (mit Reset) + render_update ---
    try:
        w.set_screen_size(640, 480)
        w.set_screen_size(0, 0)              # zuruecksetzen auf physisch
        ok("set_screen_size (+reset)")
    except Exception as e:
        fail("set_screen_size", e)
    try:
        w.render_update(); ok("render_update")
    except Exception as e:
        fail("render_update", e)


def test_con():
    section("CON (Textkonsole)")
    try:
        cols, rows = w.CON_GetMetrics()
        ok("CON_GetMetrics", str(cols) + "x" + str(rows))
    except Exception as e:
        fail("CON_GetMetrics", e)
    try:
        col, row = w.CON_GetPosition()
        ok("CON_GetPosition", "(" + str(col) + "," + str(row) + ")")
    except Exception as e:
        fail("CON_GetPosition", e)
    # CON_EnableGecko NICHT aufrufen: wuerde die Ausgabe auf USB-Gecko umleiten
    # (Bildschirm bliebe leer). Nur Vorhandensein pruefen.
    ok("CON_EnableGecko vorhanden", hasattr(w, "CON_EnableGecko"))


def test_pad_sampling():
    section("PAD Sampling-Callback")
    try:
        w.PAD_Init()
    except Exception:
        pass
    try:
        w.PAD_SetSamplingCallback(True)
        w.PAD_GetSamplingEvent()       # Zaehler leeren
        n = 0
        for _ in range(60):            # ~1 s
            w.update()
            n += w.PAD_GetSamplingEvent()
        w.PAD_SetSamplingCallback(False)
        ok("PAD_SetSamplingCallback/GetSamplingEvent", n)
    except Exception as e:
        fail("PAD_Sampling", e)


def test_netapi():
    section("Netzwerk-API (Sockets, MAC, inet_*)")
    # --- reine Umwandlungen, kein Netz noetig ---
    try:
        a = w.inet_addr("192.168.1.10")
        ok("inet_addr", a)
        b = w.inet_aton("192.168.1.10")
        ok("inet_aton", b)
        ok("inet_ntoa", w.inet_ntoa(b if b is not None else a))
    except Exception as e:
        fail("inet_*", e)

    if not w.IsNetReady():
        print("(Netz nicht bereit - Socket-Teil uebersprungen)")
        return

    try:
        ok("net_get_status",      w.net_get_status())
        ok("net_gethostip",       repr(w.net_gethostip()))
        ok("net_get_mac_address", w.net_get_mac_address())
    except Exception as e:
        fail("net_get*", e)

    # --- Socket erstellen / binden / lauschen / schliessen ---
    s = -1
    try:
        s = w.net_socket(w.AF_INET, w.SOCK_STREAM, 0)
        ok("net_socket", s)
        if s >= 0:
            ok("net_setsockopt SO_REUSEADDR",
               w.net_setsockopt(s, w.SOL_SOCKET, w.SO_REUSEADDR, 1))
            ok("net_bind :8099",    w.net_bind(s, "", 8099))
            ok("net_listen",        w.net_listen(s, 1))
            ok("net_getsockname",   w.net_getsockname(s))
            ok("net_fcntl(F_GETFL)", w.net_fcntl(s, 3, 0))
            ok("net_poll",          w.net_poll([(s, w.POLLIN)], 0))
    except Exception as e:
        fail("net socket", e)
    finally:
        if s >= 0:
            try:
                ok("net_close", w.net_close(s))
            except Exception as e:
                fail("net_close", e)

    # --- Non-Blocking-Socket: sonst blockierende Ops sicher testen ---
    # FIONBIO=1 macht accept/connect/recv/read/recvfrom sofort zurueckkehren.
    s2 = -1
    try:
        s2 = w.net_socket(w.AF_INET, w.SOCK_STREAM, 0)
        if s2 >= 0:
            ok("net_ioctl FIONBIO", w.net_ioctl(s2, w.FIONBIO, 1))
            ok("net_bind :8098",   w.net_bind(s2, "", 8098))
            ok("net_listen",       w.net_listen(s2, 1))
            ok("net_accept (nb)",  w.net_accept(s2)[0])            # sofort < 0
            ok("net_connect (nb)", w.net_connect(s2, "127.0.0.1", 9))
            ok("net_send",         w.net_send(s2, b"x"))
            ok("net_write",        w.net_write(s2, b"y"))
            ok("net_recv (nb)",    w.net_recv(s2, 8))
            ok("net_read (nb)",    w.net_read(s2, 8))
            ok("net_shutdown",     w.net_shutdown(s2, 2))
    except Exception as e:
        fail("net non-blocking", e)
    finally:
        if s2 >= 0:
            w.net_close(s2)

    # UDP: sendto / recvfrom (non-blocking)
    s3 = -1
    try:
        s3 = w.net_socket(w.AF_INET, w.SOCK_DGRAM, 0)
        if s3 >= 0:
            w.net_ioctl(s3, w.FIONBIO, 1)
            ok("net_sendto",       w.net_sendto(s3, b"z", "127.0.0.1", 9))
            ok("net_recvfrom (nb)", w.net_recvfrom(s3, 8))
    except Exception as e:
        fail("net udp", e)
    finally:
        if s3 >= 0:
            w.net_close(s3)

    # --- DNS (kann offline fehlschlagen) ---
    try:
        ok("net_gethostbyname", repr(w.net_gethostbyname("example.com")))
    except Exception as e:
        fail("net_gethostbyname", e)


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

import threading
import time

THREADS = 4
WORK_TIME = 2.0

barrier = threading.Barrier(THREADS)

start_times = [0] * THREADS
end_times = [0] * THREADS


def worker2(i):
    print(f"Thread {i} bereit")

    # Alle Threads warten hier.
    barrier.wait()

    start_times[i] = time.perf_counter()
    print(f"Thread {i} gestartet")

    # "Arbeit"
    time.sleep(WORK_TIME)

    end_times[i] = time.perf_counter()
    print(f"Thread {i} beendet")


def test_threading2():
    threads = []

    t0 = time.perf_counter()

    for i in range(THREADS):
        t = threading.Thread(target=worker2, args=(i,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    total = time.perf_counter() - t0

    print()
    print(f"Gesamtzeit: {total:.3f} s")

    for i in range(THREADS):
        print(
            f"Thread {i}: "
            f"Start={start_times[i]-t0:.3f}s  "
            f"Ende={end_times[i]-t0:.3f}s"
        )

    if total < WORK_TIME * 1.5:
        print("\n✓ Threads liefen parallel.")
    else:
        print("\n✗ Threads liefen vermutlich nacheinander.")
    wait_a()


def test_threading():
    print("test 1")
    test_threading2()
    section("threading")
    phase = "import"

    try:
        import _thread
        import threading
        ok("import threading", threading.__name__)
        ok("import _thread", _thread.__name__)
    except Exception as e:
        fail("threading import", e)
        return

    try:
        main_ident = threading.get_ident()
        ok("main thread ident", main_ident)
    except Exception as e:
        fail("threading.get_ident", e)
        return

    try:
        rlock = threading.RLock()
        ok("RLock implementation", rlock.__class__.__name__ + " aus " + rlock.__class__.__module__)
    except Exception as e:
        fail("threading.RLock", e)
        return

    ready = threading.Event()
    done = threading.Event()
    gate = threading.Lock()
    local = threading.local()
    errors = []
    results = []
    workers_done = [0]
    worker_count = 3
    phase = "thread setup"

    def worker(idx):
        try:
            local.worker_idx = idx
            ident_before = threading.get_ident()
            ready.wait(2.0)
            ident_after_wait = threading.get_ident()

            subtotal = 0
            for value in range(2000):
                subtotal += idx + value

            with gate:
                if getattr(local, "worker_idx", None) != idx:
                    errors.append("TLS mismatch in worker " + str(idx))
                ident_final = threading.get_ident()
                if ident_before != ident_after_wait or ident_before != ident_final:
                    errors.append(
                        "worker " + str(idx) + " ident instabil: "
                        + str((ident_before, ident_after_wait, ident_final))
                    )
                results.append((idx, ident_final, subtotal))
                workers_done[0] += 1
                if workers_done[0] == worker_count:
                    done.set()
        except Exception as e:
            with gate:
                errors.append("worker " + str(idx) + ": " + repr(e))
                done.set()
    print("[DBG] threading: vor thread start")
    #wait_a()
    print("[DBG] threading: nach wait, starte threads")

    threads = []
    try:
        for idx in range(worker_count):
            print("[DBG] threading: baue thread " + str(idx))
            t = threading.Thread(target=worker, args=(idx,), name="wii-worker-" + str(idx))
            threads.append(t)
            print("[DBG] threading: start thread " + str(idx))
            t.start()
            print("[DBG] threading: thread " + str(idx) + " gestartet")
        ok("threads started", len(threads))
    except Exception as e:
        fail("thread start", e, "Phase=" + phase + " gestartet=" + str(len(threads)))
        return
    
    print("[DBG] threading: alle start() fertig")
    #wait_a()
    print("[DBG] threading: setze ready event")

    phase = "worker run"
    ready.set()
    if not done.wait(5.0):
        fail("thread completion",
             "timeout waiting for workers",
             "Phase=" + phase + " fertig=" + str(workers_done[0]) + "/" + str(worker_count))
    else:
        ok("thread completion", workers_done[0])
    
    print("[DBG] threading: wait(done) vorbei")
    #wait_a()
    print("[DBG] threading: beginne join")

    phase = "join"
    for t in threads:
        t.join(1.0)
    #wait_a()

    alive = [t.name for t in threads if t.is_alive()]
    if alive:
        fail("thread join", ", ".join(alive), "Noch aktiv: " + ", ".join(alive))
    else:
        ok("thread join")

    #wait_a()

    if errors:
        fail("thread worker errors",
             "; ".join(errors),
             "Phase=" + phase + " Ergebnisse=" + str(len(results)) + "/" + str(worker_count))
        return
    
    #wait_a()

    phase = "verification"
    try:
        idents = [ident for _, ident, _ in results]
        if len(results) != worker_count:
            fail("thread results",
                 "nur " + str(len(results)) + "/" + str(worker_count),
                 "Worker fertig=" + str(workers_done[0]) + " Resultate=" + str(results))
        elif any(ident == main_ident for ident in idents):
            fail("thread idents",
                 "worker ident == main ident",
                 "main=" + str(main_ident) + " worker=" + str(idents))
        elif len(set(idents)) != worker_count:
            fail("thread idents", "nicht eindeutig", "worker=" + str(idents))
        else:
            ok("thread idents", str(idents))
        #wait_a()

        expected = [sum(idx + value for value in range(2000)) for idx in range(worker_count)]
        got = sorted(subtotal for _, _, subtotal in results)
        if got != sorted(expected):
            fail("thread workload",
                 "unerwartete Summen",
                 "erwartet=" + str(sorted(expected)) + " bekommen=" + str(got))
        else:
            ok("thread workload", str(got))
        #wait_a()
    except Exception as e:
        fail("thread verification", e, "Phase=" + phase + " results=" + str(results))


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
        for item in _errors:
            print("  [ERR] " + item["name"] + ":")
            print("        " + item["msg"])
            if item["detail"]:
                print("        Detail: " + item["detail"])
            if item["hint"]:
                print("        Hinweis: " + item["hint"])
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
    test_conf()
    test_sys()
    test_audio()
    test_con()
    test_png()
    test_network()
    test_curl()
    test_curl_http()
    test_threading()
    _print_results()
    test_frozen_modules()
    test_builtin_modules()

# ======================================================= Modul-Tests (✅) ===

def _mok(m, info=""): ok(m, info)
def _mfail(m, e): fail(m, e)

def test_mod_array():
    section("array")
    try:
        import array
        a = array.array('i', [1, 2, 3])
        a.append(4)
        assert a[3] == 4 and len(a) == 4
        _mok("array", "len=" + str(len(a)))
    except Exception as e: _mfail("array", e)

def test_mod_binascii():
    section("binascii")
    try:
        import binascii
        h = binascii.hexlify(b"hello")
        assert h == b"68656c6c6f"
        assert binascii.unhexlify(h) == b"hello"
        b64 = binascii.b2a_base64(b"wii")
        assert binascii.a2b_base64(b64) == b"wii"
        _mok("binascii", repr(h))
    except Exception as e: _mfail("binascii", e)

def test_mod_bisect():
    section("_bisect")
    try:
        import _bisect
        a = [1, 3, 5, 7]
        assert _bisect.bisect_left(a, 4) == 2
        _bisect.insort_right(a, 4)
        assert a == [1, 3, 4, 5, 7]
        _mok("_bisect", str(a))
    except Exception as e: _mfail("_bisect", e)

def test_mod_bz2():
    section("_bz2")
    try:
        import _bz2
        data = b"hello wii " * 20
        comp = _bz2.BZ2Compressor()
        c = comp.compress(data) + comp.flush()
        dec = _bz2.BZ2Decompressor()
        assert dec.decompress(c) == data
        _mok("_bz2", str(len(data)) + "->" + str(len(c)) + "B")
    except Exception as e: _mfail("_bz2", e)

def test_mod_cmath():
    section("cmath")
    try:
        import cmath
        z = cmath.sqrt(-1)
        assert abs(z - 1j) < 1e-9
        _mok("cmath", "sqrt(-1)=" + str(z))
    except Exception as e: _mfail("cmath", e)

def test_mod_csv():
    section("_csv")
    try:
        import _csv, io
        buf = io.StringIO("a,b,c\n1,2,3\n")
        rows = list(_csv.reader(buf))
        assert rows == [["a","b","c"],["1","2","3"]]
        out = io.StringIO()
        _csv.writer(out).writerow(["x","y"])
        _mok("_csv", str(rows))
    except Exception as e: _mfail("_csv", e)

def test_mod_dbm():
    section("_dbm (ndbm)")
    try:
        import _dbm
        _mok("_dbm import", str(_dbm.open))
        path = "sd:/wiitest_dbm"
        try:
            db = _dbm.open(path, "n")
            db[b"key"] = b"val"
            assert db[b"key"] == b"val"
            db.close()
            _mok("_dbm open/write/read", "ok")
            import os
            try: os.remove(path + ".db")
            except: pass
        except OSError as e2:
            ok("_dbm file-op (SD ok?)", str(e2))
    except Exception as e: _mfail("_dbm", e)

def test_mod_elementtree():
    section("_elementtree")
    try:
        import _elementtree
        # SubElement benoetigt copy.py; nur Element direkt testen
        root = _elementtree.Element("root")
        root.set("val", "1")
        assert root.get("val") == "1"
        assert root.tag == "root"
        _mok("_elementtree", root.tag + " val=" + root.get("val"))
    except Exception as e: _mfail("_elementtree", e)

def test_mod_gdbm():
    section("_gdbm")
    try:
        import _gdbm
        _mok("_gdbm import", str(_gdbm.open))
        path = "sd:/wiitest_gdbm.db"
        try:
            db = _gdbm.open(path, "n")
            db["hello"] = "wii"
            assert db["hello"] == "wii"
            db.close()
            _mok("_gdbm open/write/read", "ok")
            import os
            try: os.remove(path)
            except: pass
        except Exception as e2:
            ok("_gdbm file-op", str(e2))
    except Exception as e: _mfail("_gdbm", e)

def test_mod_hashlib():
    section("hashlib (_md5/_sha2)")
    try:
        import _md5, _sha2
        h = _md5.md5(b"hello").hexdigest()
        assert len(h) == 32          # MD5 = 16 Bytes = 32 Hex-Zeichen
        h2 = _sha2.sha256(b"wii").hexdigest()
        assert len(h2) == 64         # SHA256 = 32 Bytes = 64 Hex-Zeichen
        _mok("hashlib", "md5=" + h[:8] + "... sha256ok")
    except Exception as e: _mfail("hashlib", e)

def test_mod_heapq():
    section("_heapq")
    try:
        import _heapq
        h = [5, 3, 1, 4, 2]
        _heapq.heapify(h)
        assert _heapq.heappop(h) == 1
        _heapq.heappush(h, 0)
        assert _heapq.heappop(h) == 0
        _mok("_heapq", str(h))
    except Exception as e: _mfail("_heapq", e)

def test_mod_hmac():
    section("_hmac")
    try:
        import _hmac
        # digestmod muss ein String-Name sein ("sha2_256" oder "sha256")
        h = _hmac.new(b"key", b"message", "sha2_256")
        dig = h.hexdigest()
        assert len(dig) == 64
        _mok("_hmac", dig[:16] + "...")
    except Exception as e: _mfail("_hmac", e)

def test_mod_json():
    section("_json")
    try:
        import _json
        # _json exposes encode_basestring and make_scanner/make_encoder
        s = _json.encode_basestring("hello \"wii\"")
        assert s == '"hello \\"wii\\""'
        _mok("_json", s)
    except Exception as e: _mfail("_json", e)

def test_mod_lsprof():
    section("_lsprof")
    try:
        import _lsprof
        prof = _lsprof.Profiler()
        prof.enable()
        x = sum(range(100))
        prof.disable()
        stats = prof.getstats()
        _mok("_lsprof", "entries=" + str(len(stats)) + " sum=" + str(x))
    except Exception as e: _mfail("_lsprof", e)

def test_mod_lzma():
    section("_lzma")
    try:
        import _lzma
        data = b"hello wii " * 5   # wenig Daten -> weniger RAM
        # FORMAT_ALONE=2, preset=0 braucht ~1MB statt >10MB
        comp = _lzma.LZMACompressor(format=_lzma.FORMAT_ALONE, preset=0)
        c = comp.compress(data) + comp.flush()
        dec = _lzma.LZMADecompressor(format=_lzma.FORMAT_ALONE)
        assert dec.decompress(c) == data
        _mok("_lzma", str(len(data)) + "->" + str(len(c)) + "B")
    except Exception as e: _mfail("_lzma", e)

def test_mod_md5():
    section("_md5")
    try:
        import _md5
        h = _md5.md5(b"hello").hexdigest()
        assert len(h) == 32
        h2 = _md5.md5(b"hello").digest()
        assert len(h2) == 16
        _mok("_md5", h)
    except Exception as e: _mfail("_md5", e)

def test_mod_pickle():
    section("_pickle")
    try:
        import _pickle, io
        data = {"wii": [1, 2, 3], "ok": True}
        buf = io.BytesIO()
        p = _pickle.Pickler(buf)
        p.dump(data)
        buf.seek(0)
        d = _pickle.Unpickler(buf).load()
        assert d == data
        _mok("_pickle", str(buf.tell()) + "B")
    except Exception as e: _mfail("_pickle", e)

def test_mod_pyexpat():
    section("pyexpat")
    try:
        import pyexpat
        p = pyexpat.ParserCreate()
        tags = []
        p.StartElementHandler = lambda name, _: tags.append(name)
        p.Parse("<root><a/><b/></root>", True)
        assert tags == ["root", "a", "b"]
        _mok("pyexpat", str(tags))
    except Exception as e: _mfail("pyexpat", e)

def test_mod_random():
    section("_random")
    try:
        import _random
        rng = _random.Random()
        r = int(rng.random() * 1000)
        rng.seed(42)
        v = rng.random()
        assert 0.0 <= v < 1.0
        _mok("_random", "val=" + str(round(v, 4)))
    except Exception as e: _mfail("_random", e)

def test_mod_sha1():
    section("_sha1")
    try:
        import _sha1
        h = _sha1.sha1(b"hello").hexdigest()
        assert len(h) == 40          # SHA1 = 20 Bytes = 40 Hex-Zeichen
        _mok("_sha1", h[:16] + "...")
    except Exception as e: _mfail("_sha1", e)

def test_mod_sha2():
    section("_sha2")
    try:
        import _sha2
        h256 = _sha2.sha256(b"hello").hexdigest()
        h512 = _sha2.sha512(b"hello").hexdigest()
        assert len(h256) == 64 and len(h512) == 128
        _mok("_sha2", h256[:16] + "...")
    except Exception as e: _mfail("_sha2", e)

def test_mod_sha3():
    section("_sha3")
    try:
        import _sha3
        h = _sha3.sha3_256(b"hello").hexdigest()
        assert len(h) == 64
        _mok("_sha3", h[:16] + "...")
    except Exception as e: _mfail("_sha3", e)

def test_mod_signal():
    section("_signal")
    try:
        import _signal   # C-Modul; signal.py-Wrapper nicht verfuegbar
        old = _signal.getsignal(_signal.SIGINT)
        _signal.signal(_signal.SIGINT, _signal.SIG_IGN)
        _signal.signal(_signal.SIGINT, old)
        _mok("_signal", "SIGINT=" + str(_signal.SIGINT))
    except Exception as e: _mfail("_signal", e)

def test_mod_socket():
    section("_socket")
    try:
        import _socket
        s = _socket.socket(_socket.AF_INET, _socket.SOCK_STREAM)
        s.close()
        _mok("_socket", "AF_INET=" + str(_socket.AF_INET))
        try:
            ip = _socket.gethostbyname("example.com")
            _mok("gethostbyname", ip)
        except Exception as e2:
            ok("gethostbyname (kein Netz ok)", str(e2))
    except Exception as e: _mfail("_socket", e)

def test_mod_ssl():
    section("_ssl")
    try:
        import _ssl
        b = _ssl.RAND_bytes(16)
        assert len(b) == 16
        _mok("_ssl", "RAND_bytes ok, ver=" + _ssl.OPENSSL_VERSION)
        ctx = _ssl._SSLContext(_ssl.PROTOCOL_TLS_CLIENT)
        ok("_SSLContext erstellt")
    except NotImplementedError:
        ok("_SSLContext wirft NotImplementedError (erwartet)")
    except Exception as e: _mfail("_ssl", e)

def test_mod_statistics():
    section("_statistics")
    try:
        import _statistics
        # _statistics exportiert nur _normal_dist_inv_cdf (C-Beschleuniger)
        # NormalDist ist in statistics.py (nicht verfuegbar auf Wii)
        fn = _statistics._normal_dist_inv_cdf
        v = fn(0.5, 0.0, 1.0)   # Median der Normalverteilung = 0
        assert abs(v) < 1e-6
        _mok("_statistics", "_normal_dist_inv_cdf(0.5)=" + str(round(v, 6)))
    except Exception as e: _mfail("_statistics", e)

def test_mod_struct():
    section("_struct")
    try:
        import _struct
        b = _struct.pack(">HI", 0xDEAD, 0xBEEFCAFE)
        h, i = _struct.unpack(">HI", b)
        assert h == 0xDEAD and i == 0xBEEFCAFE
        _mok("_struct", hex(h) + " " + hex(i))
    except Exception as e: _mfail("_struct", e)

def test_mod_symtable():
    section("_symtable")
    try:
        import _symtable   # C-Modul; symtable.py-Wrapper nicht verfuegbar
        t = _symtable.symtable("x = 1 + 2", "<test>", "exec")
        _mok("_symtable", str(t))
    except Exception as e: _mfail("_symtable", e)

def test_mod_unicodedata():
    section("unicodedata")
    try:
        import unicodedata
        name = unicodedata.name("A")
        assert name == "LATIN CAPITAL LETTER A"
        cat = unicodedata.category("A")
        _mok("unicodedata", name + " cat=" + cat)
    except Exception as e: _mfail("unicodedata", e)

def test_mod_uuid():
    section("_uuid")
    try:
        import _uuid
        # _uuid.generate_time_safe() gibt (bytes, int) zurueck
        result = _uuid.generate_time_safe()
        assert len(result[0]) == 16
        _mok("_uuid", repr(result[0].hex()))
    except Exception as e: _mfail("_uuid", e)

def test_mod_zlib():
    section("zlib")
    try:
        import zlib
        data = b"hello wii zlib " * 30
        c = zlib.compress(data)
        assert zlib.decompress(c) == data
        crc = zlib.crc32(data)
        _mok("zlib", str(len(data)) + "->" + str(len(c)) + "B crc=" + hex(crc))
    except Exception as e: _mfail("zlib", e)

def test_mod_zoneinfo():
    section("_zoneinfo")
    try:
        # datetime-Stub muss gesetzt sein (oben im File)
        import _zoneinfo
        _mok("_zoneinfo", "ZoneInfo=" + str(_zoneinfo.ZoneInfo))
    except Exception as e: _mfail("_zoneinfo", e)


MODULE_TESTS = [
    ("array",        test_mod_array),
    ("binascii",     test_mod_binascii),
    ("_bisect",      test_mod_bisect),
    ("_bz2",         test_mod_bz2),
    ("cmath",        test_mod_cmath),
    ("_csv",         test_mod_csv),
    ("_dbm",         test_mod_dbm),
    ("_elementtree", test_mod_elementtree),
    ("_gdbm",        test_mod_gdbm),
    ("hashlib",      test_mod_hashlib),
    ("_heapq",       test_mod_heapq),
    ("hmac/_hmac",   test_mod_hmac),
    ("_json",        test_mod_json),
    ("_lsprof",      test_mod_lsprof),
    ("_lzma",        test_mod_lzma),
    ("_md5",         test_mod_md5),
    ("_pickle",      test_mod_pickle),
    ("pyexpat",      test_mod_pyexpat),
    ("_random",      test_mod_random),
    ("_sha1",        test_mod_sha1),
    ("_sha2",        test_mod_sha2),
    ("_sha3",        test_mod_sha3),
    ("signal",       test_mod_signal),
    ("socket",       test_mod_socket),
    ("_ssl",         test_mod_ssl),
    ("_statistics",  test_mod_statistics),
    ("_struct",      test_mod_struct),
    ("symtable",     test_mod_symtable),
    ("unicodedata",  test_mod_unicodedata),
    ("_uuid",        test_mod_uuid),
    ("zlib",         test_mod_zlib),
    ("_zoneinfo",    test_mod_zoneinfo),
]


def run_all_module_tests():
    global _pass, _fail, _errors
    _pass = 0
    _fail = 0
    _errors = []
    print("")
    print("================================")
    print("  Alle Modul-Tests (" + str(len(MODULE_TESTS)) + ")")
    print("================================")
    for label, fn in MODULE_TESTS:
        try:
            fn()
        except Exception as e:
            fail(label, e)
    _print_results()


def show_narrow_menu(items, sel, hint="A=run  HOME=zurueck"):
    """Zeigt nur die Zeile darueber, die aktuelle (->), und die darunter."""
    n = len(items)
    above = ("    " + items[sel - 1][0]) if sel > 0     else ""
    curr  = " -> " + items[sel][0]
    below = ("    " + items[sel + 1][0]) if sel < n - 1 else ""
    print("")
    if above: print(above)
    print(curr)
    if below: print(below)
    print("  [" + str(sel + 1) + "/" + str(n) + "]  UP/DOWN  " + hint)


def module_test_menu():
    global _pass, _fail, _errors
    # Eintrag 0 = "Alle 32 testen", danach die einzelnen Module
    items = [(">> Alle 32 testen <<", run_all_module_tests)] + MODULE_TESTS
    sel = 0
    show_narrow_menu(items, sel)
    while True:
        w.update()
        if w.WPAD_ButtonsDown(w.WPAD_BUTTON_HOME, 0):
            break
        if w.WPAD_ButtonsDown(w.WPAD_BUTTON_UP, 0):
            sel = (sel - 1) % len(items)
            show_narrow_menu(items, sel)
        elif w.WPAD_ButtonsDown(w.WPAD_BUTTON_DOWN, 0):
            sel = (sel + 1) % len(items)
            show_narrow_menu(items, sel)
        elif w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0):
            label, fn = items[sel]
            if sel == 0:
                fn()   # run_all_module_tests hat eigene Ausgabe
            else:
                _run_single(fn)
            show_narrow_menu(items, sel)


# ----------------------------------------------------------------------- menu --
def snake():
  try:
    import snake
  except Exception as e:
    w.terminal_init()
    import io as _io
    _buf = _io.StringIO()
    _buf.write(type(e).__name__ + ": " + str(e) + "\n")
    _tb = e.__traceback__
    while _tb:
      _buf.write("  File " + str(_tb.tb_frame.f_code.co_filename)
                 + " line " + str(_tb.tb_lineno)
                 + " in " + str(_tb.tb_frame.f_code.co_name) + "\n")
      _tb = _tb.tb_next
    _msg = _buf.getvalue()
    try:
      with open("sd:/out.snake.txt", "a") as _f:
        _f.write(_msg)
    except Exception:
      pass
    raise
  finally:
    w.terminal_init()


# Hier neue Eintraege einfuegen: (Label, Funktion)
def _exists(p):
    try:
        f = open(p, "rb"); f.close(); return True
    except Exception:
        return False


def test_dlopen_so():
    """Laedt zur Laufzeit eine C-Extension (sd:/spam.so) via `import spam`.
    Voraussetzung: sd:/spam.so und sd:/symbols.map liegen auf sd:/."""
    import sys, _imp
    # kompakte Diagnose (kurze Zeilen, kein Umbruch auf schmalem Schirm)
    print("so-suffix: " + ("ja" if ".so" in _imp.extension_suffixes() else "NEIN"))
    # sind die externen Importer ueberhaupt installiert?
    print("meta_path: " + str(len(sys.meta_path)) +
          " hooks: " + str(len(sys.path_hooks)))
    print("spam.so: " + ("da" if _exists("sd:/spam.so") else "FEHLT"))
    # Findet der FileFinder sd:/ per listdir? (open() allein reicht ihm nicht)
    try:
        import os
        names = os.listdir("sd:/")
        print("ls sd:/: " + str(len(names)) + " eintr, spam.so " +
              ("DA" if "spam.so" in names else "NICHT"))
    except Exception as e:
        print("listdir ERR: " + repr(e)[:40])
    # Was liefert der Import-Finder direkt?
    try:
        import importlib.util as _iu
        sp = _iu.find_spec("spam")
        print("find_spec: " + (repr(sp)[:55] if sp else "None"))
    except Exception as e:
        print("find_spec ERR: " + repr(e)[:45])
    try:
        import spam
    except Exception as e:
        fail("import spam", e)
        return
    ok("import")
    try:
        ok("add(2,3)", spam.add(2, 3))
    except Exception as e:
        fail("add", e)
    try:
        ok("greet", spam.greet())
    except Exception as e:
        fail("greet", e)


def menu_dlopen_so():
    _run_single(test_dlopen_so)
    print("A = zurueck")
    wait_a()


MENU = [
    ("Alle Tests",    run_all_tests),
    ("dlopen .so",    menu_dlopen_so),
    ("constants",     lambda: _run_single(test_constants)),
    ("video",         lambda: _run_single(test_video)),
    ("filesystem",    lambda: _run_single(test_filesystem)),
    ("wpad",          lambda: _run_single(test_wpad)),
    ("pad",           lambda: _run_single(test_pad)),
    ("pad sampling",  lambda: _run_single(test_pad_sampling)),
    ("sys power/reset", lambda: _run_single(test_sys_power_reset)),
    ("conf",          lambda: _run_single(test_conf)),
    ("sys info",      lambda: _run_single(test_sys)),
    ("audio (dma)",   lambda: _run_single(test_audio)),
    ("audio play",    lambda: _run_single(test_audio_play)),
    ("graphics",      lambda: _run_single(test_graphics)),
    ("con",           lambda: _run_single(test_con)),
    ("net api",       lambda: _run_single(test_netapi)),
    ("png",           lambda: _run_single(test_png)),
    ("network",       lambda: _run_single(test_network)),
    ("curl (https)",  lambda: _run_single(test_curl)),
    ("curl (http)",   lambda: _run_single(test_curl_http)),
    ("threading",     lambda: _run_single(test_threading)),
    ("write sd",      lambda: _run_single(test_write_sd)),
    ("write usb",     lambda: _run_single(test_write_usb)),
    ("frozen modules", lambda: _run_single(test_frozen_modules)),
    ("builtin modules", lambda: _run_single(test_builtin_modules)),
    ("module tests",   module_test_menu),
    ("snake",          snake),

    # ("mein Test",   my_test_fn),
]


def show_menu(sel):
    show_narrow_menu(MENU, sel, "A=run  HOME=beenden")


def menu_loop():
    sel = 0
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
