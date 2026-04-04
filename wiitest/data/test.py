"""
print "hello world!!!!!!!!!!!!!!!!!!!!!!"

import wiitools
import sys
import os
import math

print "hello 2"

w = wiitools

True  = 1
False = 0
true  = 1
false = 0

which = 0

# Simple interactive selection using WPAD buttons
w.update()
while which == 0:
    if w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0):
        which = 1
    if w.WPAD_ButtonsDown(w.WPAD_BUTTON_B, 0):
        which = 2
    w.update()

if which == 1:
    # Keep the original PNG test minimal and robust
    print "=== PNG load/render test start ==="
    print "wiitools.__version__:", getattr(wiitools, "__version__", "<none>")

    mounted = wiitools.fatInitDefault()
    print "fatInitDefault:", mounted

    try:
        size = wiitools.png_load_embedded()
        print "[OK ] embedded png size:", size
    except Exception, e:
        print "[INF] embedded missing/fail:", repr(e)

    # Exit after the PNG test for simplicity
    try:
        wiitools.rendering_init()
        wiitools.png_use("__default__")
        wiitools.update()
    except Exception, e:
        wiitools.terminal_init()
        etype, eval, _tb = sys.exc_info()
        print "[ERR] png test failed:", repr(e)
        print "[ERR] type:", etype
        print "[ERR] str :", str(e)
        print "[ERR] args:", getattr(eval, "args", None)

    print "=== PNG load/render test end ==="

else:
    # Network test path
    try:
        # small wait for network thread
        import time
        time.sleep(1)
        print "IsNetReady:", wiitools.IsNetReady()

        # Simple HTTP/HTTPS tests
        r = wiitools.curl_get("https://example.com", timeout_ms=10000, verify_peer=0, verify_host=0, verbose=0)
        print "ok, status, curl_code:", r.get("ok"), r.get("status"), r.get("curl_code")
        print "error:", r.get("error")
        print "body[:200]:", r.get("body","")[:200]

        r2 = wiitools.curl_get("https://example.com", timeout_ms=10000, verify_peer=0, verify_host=0, verbose=1)
        print "ok, status, curl_code:", r2.get("ok"), r2.get("status"), r2.get("curl_code")
        print "error:", r2.get("error")
        print "---- libcurl verbose (first 2000 chars) ----"
        print r2.get("verbose","")[:2000]

        w.usleep(1000000)
    except Exception, e:
        wiitools.terminal_init()
        etype, eval, _tb = sys.exc_info()
        print "[ERR] network test failed:", repr(e)
        print "[ERR] type:", etype
        print "[ERR] str :", str(e)
        print "[ERR] args:", getattr(eval, "args", None)
"""


print("hello world!!!!!!!!!!!!!!!!!!!!!!")
print("3/2 =", 3/2)

import wiitools as w
for i in range(1):
    w.VIDEO_WaitVSync()


w.fatInitDefault()



############################################################################################################
############################################################################################################
############################################################################################################

import sys
import os

# Custom loader for Wii that doesn't cache bytecode (which blocks)
import importlib.abc
import importlib.machinery
import importlib.util
import types

class WiiSourceLoader(importlib.abc.Loader):
    def __init__(self, name, path):
        self.name = name
        self.path = path
    
    def create_module(self, spec):
        return None  # Use default module creation
    
    def exec_module(self, module):
        with open(self.path, 'r', encoding='utf-8') as f:
            code = compile(f.read(), self.path, 'exec')
        exec(code, module.__dict__)

class WiiSourceFinder(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path=None, target=None):
        if '.' in fullname:
            return None
        for base_path in sys.path:
            if not (base_path.startswith('sd') or base_path.startswith('usb')):
                continue
            if not base_path.endswith('/'):
                base_path = base_path + '/'
            mod_file = base_path + fullname + '.py'
            try:
                if os.path.isfile(mod_file):
                    loader = WiiSourceLoader(fullname, mod_file)
                    spec = importlib.util.spec_from_file_location(fullname, mod_file, loader=loader)
                    if spec:
                        return spec
            except Exception:
                pass
        return None

sys.meta_path.insert(0, WiiSourceFinder())
for p in sys.path:
    print("  ", p)

for i in range(1):
    w.VIDEO_WaitVSync()



############################################################################################################
############################################################################################################
############################################################################################################

print("[DBG] importlib path debug end")
import testg  # DISABLED FOR TESTING
print("test3 printed (testg import skipped)")

print("-----------------------------------")



"""
file_path = "sd:/hello/test2.py"

try:
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()
    print("Dateiinhalt von", file_path)
    print("--------------------")
    print(content)
except FileNotFoundError:
    print(f"Die Datei {file_path} wurde nicht gefunden.")
except IOError as e:
    print(f"Fehler beim Lesen der Datei: {e}")

for i in range(200):
    w.VIDEO_WaitVSync()

import sys

for i, p in enumerate(sys.path):
    print(f"sys.path[{i}] = {p}")

print("import test")

import test2
"""
"""
print("import os")
import os
print("os imported successfully!")
import wiitools
print("wiitools imported successfully!")

for i in range(300):
    wiitools.VIDEO_WaitVSync()
print("3000")

def printf(str):
    print(str)
    for i in range(50):
        wiitools.VIDEO_WaitVSync()

# Testet, ob die Konsole reagiert (gibt 0 bei Erfolg zurück)
print("status")
printf("--- Status test ---")
status = os.system("echo 'Test bestanden'") 
print(status) 
print("ende")
print("test.py: === End of test.py ===")

"""
"""
import wiitools
import sys
import math

print "hello 2"

w = wiitools

True  = 1
False = 0
true  = 1
false = 0

which = 0

w.update()
while which == 0:
    if w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0):
        which = 1
    if w.WPAD_ButtonsDown(w.WPAD_BUTTON_B, 0):
        which = 2
    w.update()

if which == 1:
    print "=== PNG load/render test start ==="
    print "wiitools.__version__:", getattr(wiitools, "__version__", "<none>")

    mounted = wiitools.fatInitDefault()
    print "fatInitDefault:", mounted


    path = None

    try:
        size = wiitools.png_load_embedded()
        print "[OK ] embedded png size:", size
        path = "<embedded>"
    except Exception, e:
        print "[INF] embedded missing/fail:", repr(e)

    paths = ["sd:/data/test.png", "usb:/test.png"]

    if path is None:
        for p in paths:
            try:
                f = open(p, "rb")
                head = f.read(8)
                f.seek(0, 2)
                fsize = f.tell()
                f.close()
                print "[OK ] file open:", p, fsize, "bytes, head=", repr(head)
                path = p
                break
            except Exception, e:
                print "[INF] missing:", p, repr(e)

    if path is None:
        print "[ERR] no PNG found in", paths
        print "=== PNG load/render test end ==="
        raise SystemExit

    try:
        if path != "<embedded>":
            print "load:", path
            size = wiitools.png_load(path)
            print "[OK ] png size:", size
        # Same image under explicit name for multi-png API demo.
        if path != "<embedded>":
            wiitools.png_load_named(path, "bg")
        else:
            wiitools.png_load_embedded_named("bg")

        brew_path = None
        brew_paths = ["sd:/data/brew.png", "usb:/brew.png"]
        for p in brew_paths:
            try:
                f = open(p, "rb")
                f.read(1)
                f.close()
                brew_path = p
                print "[OK ] brew file open:", p
                break
            except Exception, e:
                print "[INF] brew missing:", p, repr(e)
        if brew_path is not None:
            wiitools.png_load_named(brew_path, "brew")
        else:
            print "[INF] brew.png not found; skip brew draw"

        if brew_path is not None:
            for i in range(10):
                print "jes"
        else:
            for i in range(10):
                print "no"


        wiitools.rendering_init()
        wiitools.png_use("bg")
        bg_info = wiitools.png_info()
        tex_w = bg_info[0]
        tex_h = bg_info[1]

        cube_vertices = [
            (-1.0, -1.0, -1.0), (1.0, -1.0, -1.0),
            (1.0,  1.0, -1.0), (-1.0,  1.0, -1.0),
            (-1.0, -1.0,  1.0), (1.0, -1.0,  1.0),
            (1.0,  1.0,  1.0), (-1.0,  1.0,  1.0)
        ]
        cube_faces = [
            (4, 5, 6, 7),
            (1, 0, 3, 2),
            (0, 4, 7, 3),
            (5, 1, 2, 6),
            (3, 7, 6, 2),
            (0, 1, 5, 4)
        ]
        cube_cx = 320.0
        cube_cy = 240.0
        cube_scale = 120.0
        cube_dist = 4.2

        exit2 = false

        i = 0
        # Draw once into backbuffer
        while True: # gameloop
            if i > 1000:
                i = 0
            i = i+1

            if wiitools.WPAD_ButtonsDown(wiitools.WPAD_BUTTON_HOME, 0):
                exit2 = true
                raise SystemExit

            wiitools.png_use("bg")
            wiitools.png_show_fullscreen()
            wiitools.png_show_region(20, 20, 200, 40, 100, 80)
            wiitools.png(i, 30, 20, 10, 80, 60, 320, 240)

            if brew_path is not None:
                wiitools.png_use("brew")
                wiitools.png(i, 245, 20, 10, 80, 60, 320, 240)

            wiitools.draw_rect(100, 100, 100, 100, (0, 0, 100, 250), i)
            wiitools.draw_circle(200, 200, i, (0, 100, 0, 150))
            wiitools.draw_oval(300, 300, 40, 80, (100, 0, 0, 250), i)
            wiitools.render_text(40, 40, "Hallo Wii", 2, 1, (255, 255, 0, 255), i)

            if wiitools.WPAD_ButtonsHeld(wiitools.WPAD_BUTTON_A, 0):
                wiitools.render_text(0, 200, "HELLO WORLD!!!", 5, 0, (225, 225, 0, 225), 0)
                wiitools.render_text(0 + wiitools.text_length("HELLO WORLD!!!", 5),
                                      200, "++", 5, 0, (225, 225, 0, 225), 0)

            wiitools.png_use("bg")
            ay = i * 0.045
            ax = i * 0.027
            cy = math.cos(ay)
            sy = math.sin(ay)
            cx = math.cos(ax)
            sx = math.sin(ax)

            proj = []
            for vx, vy, vz in cube_vertices:
                # rotate around Y then X
                rx = vx * cy + vz * sy
                rz = -vx * sy + vz * cy
                ry = vy * cx - rz * sx
                rz2 = vy * sx + rz * cx

                zz = rz2 + cube_dist
                if zz < 0.3:
                    zz = 0.3

                px = cube_cx + (rx * cube_scale) / zz
                py = cube_cy + (ry * cube_scale) / zz
                proj.append((px, py, zz))

            draw_faces = []
            for f in cube_faces:
                zavg = (proj[f[0]][2] + proj[f[1]][2] + proj[f[2]][2] + proj[f[3]][2]) * 0.25
                draw_faces.append((zavg, f))
            draw_faces.sort()
            draw_faces.reverse()  # far -> near

            for _z, f in draw_faces:
                x1, y1 = proj[f[0]][0], proj[f[0]][1]
                x2, y2 = proj[f[1]][0], proj[f[1]][1]
                x3, y3 = proj[f[2]][0], proj[f[2]][1]
                x4, y4 = proj[f[3]][0], proj[f[3]][1]
                wiitools.png_quad(x1, y1, x2, y2, x3, y3, x4, y4, 0, 0, tex_w, tex_h)
            
            # Present via update() every frame
        #for i in range(300):
            wiitools.update()
        # Avoid print while rendering mode is active (terminal renderer uses another framebuffer)

        # optional: save a copy
        rc = wiitools.png_save("sd:/test_copy.png")
        print "png_save rc:", rc

    except Exception, e:
        if not exit2:
            wiitools.terminal_init()
            etype, eval, _tb = sys.exc_info()
            print "[ERR] png test failed:", repr(e)
            print "[ERR] type:", etype
            print "[ERR] str :", str(e)
            print "[ERR] args:", getattr(eval, "args", None)

    wiitools.png_unload_all()

    print "=== PNG load/render test end ==="


else:
    try:
        w.update()
        t = true
        while t:
            if w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0):
                t = false
            print w.IsNetReady()
            w.update()
        print "IsNetReady:", wiitools.IsNetReady()
"""
        #print ""
"""
        r = wiitools.curl_get("http://localhost:8000", timeout_ms=10000, verify_peer=0, verify_host=0, verbose=1)
        print "ok,status,curl_code:", r.get("ok"), r.get("status"), r.get("curl_code")
        print "error:", r.get("error")
        print r.get("verbose","")[:2000]    
        w.usleep(5000000)

        # Einfacher Test mit verbose=0 (kein TLS verify)
        r = wiitools.curl_get("https://example.com", timeout_ms=10000, verify_peer=0, verify_host=0, verbose=0)
        print "ok, status, curl_code:", r["ok"], r["status"], r["curl_code"]
        print "error:", r["error"]
        print "body[:200]:", r["body"][:200]

        # Ausführliches Debug (libcurl verbose output)
        r2 = wiitools.curl_get("https://example.com", timeout_ms=10000, verify_peer=0, verify_host=0, verbose=1)
        print "ok, status, curl_code:", r2["ok"], r2["status"], r2["curl_code"]
        print "error:", r2["error"]
        print "---- libcurl verbose (first 2000 chars) ----"
        print r2.get("verbose", "")[:2000]
"""
"""
        r = wiitools.curl_get("https://example.com", timeout_ms=10000, verify_peer=0, verify_host=0)
        print r["ok"], r["status"]
        print r["body"][:200]
"""
"""
        r = wiitools.curl_get(
            "https://example.com",
            timeout_ms=10000,
            verify_peer=1,
            verify_host=1,
            ca_file="sd:/data/cacert.pem"  # Stelle sicher, dass diese Datei existiert
        )
        print(r["ok"], r["status"])
        print(r["body"][:200])



        if 1==1:
                print ""
                print "IsNetReady:", wiitools.IsNetReady()
                print "get_local_ip:", wiitools.get_local_ip()
                w.usleep(3000000)


                # Targets to probe: local loopback, local server (127.0.0.1), and public example.com
                targets = ["https://example.com"]
                # regex module may not be available on the Wii; use simple substring checks
                def interpret_verbose(vstr):
                    if not vstr:
                        return "(no verbose output)"
                    res = []
                    vs = vstr.lower()
                    # DNS resolution hints
                    if vs.find("was resolved") != -1 or vs.find("resolving") != -1 or vs.find("resolved") != -1:
                        res.append("DNS: resolved (look for addresses in verbose)")
                    if vs.find("could not resolve") != -1 or vs.find("name lookup timed out") != -1 or vs.find("couldn't resolve") != -1:
                        res.append("DNS: resolution failed")
                    # Connect/connect errors
                    if vs.find("failed to connect") != -1 or vs.find("could not connect") != -1 or vs.find("connection refused") != -1 or vs.find("no route to host") != -1:
                        res.append("Connect: failed (see verbose for details)")
                    if vs.find("connected to") != -1 or vs.find("connection established") != -1 or vs.find("connected (") != -1:
                        res.append("Connect: succeeded")
                    # TLS handshake hints
                    if vs.find("ssl") != -1 or vs.find("handshake failed") != -1 or vs.find("ssl_connect") != -1 or vs.find("tls") != -1:
                        res.append("TLS: handshake/SSL messages present")
                    if res:
                        return ", ".join(res)
                    return "(no quick hints found)"

                for t in targets:
                    try:
                        print "-- testing:", t
                        r = wiitools.curl_get(t, timeout_ms=10000, verify_peer=0, verify_host=0, verbose=1)
                    except Exception, e:
                        wiitools.terminal_init()
                        print "curl_get raised:", repr(e)
                        continue
                    # Basic fields
                    try:
                        print "ok,status,curl_code:", r.get("ok"), r.get("status"), r.get("curl_code")
                    except Exception:
                        print "(missing ok/status/curl_code)"
                    err = r.get("error")
                    print "error:", repr(err)
                    body = r.get("body", "")
                    try:
                        print "body length:", len(body)
                    except Exception:
                        print "body: (unprintable)"
                    v = r.get("verbose", "")
                    if v is not None:
                        try:
                            print "verbose length:", len(v)
                        except Exception:
                            print "verbose length: (unmeasurable)"
                    else:
                        print "verbose length: 0"
                    print "---- libcurl verbose (first 2000 chars) ----"
                    try:
                        print v[:2000]
                    except Exception, e:
                        print "  (verbose print failed):", repr(e)

                    # Quick interpretation from verbose
                    hint = interpret_verbose(v)
                    print "== quick hint: ", hint

                    # Helpful actionable hints
                    if r.get("curl_code") == 7 or (err and "could not connect" in str(err)):
                        print "ACTION: Connect failed. Check emulator/host firewall, Dolphin network forwarding, or server listening on target."
                    if ( (v or "").lower().find("was resolved") == -1 ) and ("curl_code" in r and r.get("curl_code") != 0):
                        print "ACTION: DNS may not have resolved — try curl_get against an IP address next."

                    # small pause to allow output to be visible
                    w.usleep(500000)
        

        w.usleep(1000000)
    except Exception, e:
        wiitools.terminal_init()
        etype, eval, _tb = sys.exc_info()
        print "[ERR] curl test failed:", repr(e)
        print "[ERR] type:", etype
        print "[ERR] str :", str(e)
        print "[ERR] args:", getattr(eval, "args", None)
"""
"""
else:
    try:
        
        w.update()
        t = true
        while t:
            if w.WPAD_ButtonsDown(w.WPAD_BUTTON_A):
                t = false
            print str(IsNetReady())
            w.update()

        r = wiitools.curl_get("https://example.com", timeout_ms=10000, verify_peer=0, verify_host=0)
        print r["ok"], r["status"]
        print r["body"][:200]
        
        w.usleep(1000000)
    except Exception, e:
        if 1==1:
            wiitools.terminal_init()
            etype, eval, _tb = sys.exc_info()
            print "[ERR] png test failed:", repr(e)
            print "[ERR] type:", etype
            print "[ERR] str :", str(e)
            print "[ERR] args:", getattr(eval, "args", None)
"""




