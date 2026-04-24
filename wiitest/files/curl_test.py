
import wiitools

w = wiitools

import sys

def test_curl():
    try:

        w.update()

        t = True

        while t:

            if w.WPAD_ButtonsDown(w.WPAD_BUTTON_A,0):
                t = False

            print(w.IsNetReady())

            w.update()

        print("IsNetReady:", wiitools.IsNetReady())

        print("")

        print("IsNetReady:", wiitools.IsNetReady())

        print("get_local_ip:", wiitools.get_local_ip())

        w.usleep(3000000)

        targets = ["https://example.com"]

        def interpret_verbose(vstr):

            if not vstr:
                return "(no verbose output)"

            res = []
            vs = vstr.lower()

            if "resolved" in vs:
                res.append("DNS ok")

            if "could not resolve" in vs:
                res.append("DNS failed")

            if "failed to connect" in vs:
                res.append("connect failed")

            if "connected" in vs:
                res.append("connect ok")

            if "ssl" in vs:
                res.append("TLS involved")

            return ", ".join(res) if res else "(no hints)"

        for t in targets:

            print("-- testing:", t)

            r = wiitools.curl_get(
                t,
                timeout_ms=10000,
                verify_peer=0,
                verify_host=0,
                verbose=1
            )

            print(
                "ok,status,curl_code:",
                r.get("ok"),
                r.get("status"),
                r.get("curl_code")
            )

            print("error:", repr(r.get("error")))

            body = r.get("body","")

            print("body length:", len(body))

            v = r.get("verbose","")

            print("verbose length:", len(v))

            print(v[:2000])

            print(
                "hint:",
                interpret_verbose(v)
            )

            w.usleep(500000)

        w.usleep(1000000)

    except Exception as e:

        wiitools.terminal_init()

        etype, eval, _tb = sys.exc_info()

        print("[ERR] curl test failed:", repr(e))
        print("[ERR] type:", etype)
        print("[ERR] str :", str(e))
        print("[ERR] args:", getattr(eval, "args", None))