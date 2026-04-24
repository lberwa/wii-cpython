import wiitools
import math
import sys

def test_rendering():
    print("=== PNG load/render test start ===")
    print("wiitools.__version__:", getattr(wiitools, "__version__", "<none>"))

    mounted = wiitools.fatInitDefault()
    print("fatInitDefault:", mounted)

    path = None

    try:
        size = wiitools.png_load_embedded()
        print("[OK ] embedded png size:", size)
        path = "<embedded>"
    except Exception as e:
        print("[INF] embedded missing/fail:", repr(e))

    paths = ["sd:/data/test.png", "usb:/test.png"]

    if path is None:
        for p in paths:
            try:
                with open(p, "rb") as f:
                    head = f.read(8)
                    f.seek(0, 2)
                    fsize = f.tell()

                print("[OK ] file open:", p, fsize, "bytes, head=", repr(head))
                path = p
                break

            except Exception as e:
                print("[INF] missing:", p, repr(e))

    if path is None:
        print("[ERR] no PNG found in", paths)
        print("=== PNG load/render test end ===")
        raise SystemExit

    try:
        if path != "<embedded>":
            print("load:", path)
            size = wiitools.png_load(path)
            print("[OK ] png size:", size)

        if path != "<embedded>":
            wiitools.png_load_named(path, "bg")
        else:
            wiitools.png_load_embedded_named("bg")

        brew_path = None
        brew_paths = ["sd:/data/brew.png", "usb:/brew.png"]

        for p in brew_paths:
            try:
                with open(p, "rb") as f:
                    f.read(1)

                brew_path = p
                print("[OK ] brew file open:", p)
                break

            except Exception as e:
                print("[INF] brew missing:", p, repr(e))

        if brew_path is not None:
            wiitools.png_load_named(brew_path, "brew")
        else:
            print("[INF] brew.png not found; skip brew draw")

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
            (4,5,6,7),
            (1,0,3,2),
            (0,4,7,3),
            (5,1,2,6),
            (3,7,6,2),
            (0,1,5,4)
        ]

        cube_cx = 320.0
        cube_cy = 240.0
        cube_scale = 120.0
        cube_dist = 4.2

        exit2 = False
        i = 0

        while True:

            if i > 1000:
                i = 0

            i += 1

            if wiitools.WPAD_ButtonsDown(wiitools.WPAD_BUTTON_HOME, 0):
                exit2 = True
                raise SystemExit

            wiitools.png_use("bg")
            wiitools.png_show_fullscreen()

            wiitools.png_show_region(
                20, 20,
                200, 40,
                100, 80
            )

            wiitools.png(
                i, 30,
                20, 10,
                80, 60,
                320, 240
            )

            if brew_path is not None:

                wiitools.png_use("brew")

                wiitools.png(
                    i, 245,
                    20, 10,
                    80, 60,
                    320, 240
                )

            wiitools.draw_rect(
                100, 100,
                100, 100,
                (0,0,100,250),
                i
            )

            wiitools.draw_circle(
                200, 200,
                i,
                (0,100,0,150)
            )

            wiitools.draw_oval(
                300, 300,
                40, 80,
                (100,0,0,250),
                i
            )

            wiitools.render_text(
                40, 40,
                "Hallo Wii",
                2, 1,
                (255,255,0,255),
                i
            )

            if wiitools.WPAD_ButtonsHeld(wiitools.WPAD_BUTTON_A, 0):

                wiitools.render_text(
                    0, 200,
                    "HELLO WORLD!!!",
                    5, 0,
                    (225,225,0,225),
                    0
                )

                wiitools.render_text(
                    wiitools.text_length("HELLO WORLD!!!",5),
                    200,
                    "++",
                    5, 0,
                    (225,225,0,225),
                    0
                )

            wiitools.png_use("bg")

            ay = i * 0.045
            ax = i * 0.027

            cy = math.cos(ay)
            sy = math.sin(ay)

            cx = math.cos(ax)
            sx = math.sin(ax)

            proj = []

            for vx,vy,vz in cube_vertices:

                rx = vx * cy + vz * sy
                rz = -vx * sy + vz * cy

                ry = vy * cx - rz * sx
                rz2 = vy * sx + rz * cx

                zz = rz2 + cube_dist

                if zz < 0.3:
                    zz = 0.3

                px = cube_cx + (rx * cube_scale) / zz
                py = cube_cy + (ry * cube_scale) / zz

                proj.append((px,py,zz))

            draw_faces = []

            for f in cube_faces:

                zavg = (
                    proj[f[0]][2]
                    + proj[f[1]][2]
                    + proj[f[2]][2]
                    + proj[f[3]][2]
                ) * 0.25

                draw_faces.append((zavg,f))

            draw_faces.sort(reverse=True)

            for _z,f in draw_faces:

                x1,y1 = proj[f[0]][0], proj[f[0]][1]
                x2,y2 = proj[f[1]][0], proj[f[1]][1]
                x3,y3 = proj[f[2]][0], proj[f[2]][1]
                x4,y4 = proj[f[3]][0], proj[f[3]][1]

                wiitools.png_quad(
                    x1,y1,
                    x2,y2,
                    x3,y3,
                    x4,y4,
                    0,0,
                    tex_w,tex_h
                )

            wiitools.update()

        rc = wiitools.png_save("sd:/test_copy.png")

        print("png_save rc:", rc)

    except Exception as e:

        if not exit2:

            wiitools.terminal_init()

            etype, eval, _tb = sys.exc_info()

            print("[ERR] png test failed:", repr(e))
            print("[ERR] type:", etype)
            print("[ERR] str :", str(e))
            print("[ERR] args:", getattr(eval, "args", None))

    wiitools.png_unload_all()

    print("=== PNG load/render test end ===")