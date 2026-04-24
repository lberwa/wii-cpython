print("hello world!!!!!!!!!!!!!!!!!!!!!!")

import os
DEBUG_PRINT_CURL_HEAD = False
if DEBUG_PRINT_CURL_HEAD:
    fd = os.open("sd:/curl_test.py", os.O_RDONLY)
    data = os.read(fd, 64)
    print(data.decode("utf-8", "replace"))
    os.close(fd)



import wiitools
import sys
import math
import testg

print("curl test:")
import curl_test
print("--------------------")
print("curl_test.__file__ =", getattr(curl_test, "__file__", None))
print("curl_test.__spec__.origin =", getattr(getattr(curl_test, "__spec__", None), "origin", None))
print("curl_test dir:", dir(curl_test))
print("--------------------")
print("!!!!!!!!!")
print("rendering test:")
import rendering_test
print("--------------------")
print("rendering_test.__file__ =", getattr(rendering_test, "__file__", None))
print("rendering_test.__spec__.origin =", getattr(getattr(rendering_test, "__spec__", None), "origin", None))
print("--------------------")
print("!!!!!!!!!")
print("ende")

print("hello 2")

print("--------------------")
import wiitools
print(dir(curl_test))
print("--------------------")

w = wiitools

which = 0
start = [curl_test.test_curl, rendering_test.test_rendering]
which_max = len(start)

w.update()
while True:
    if w.WPAD_ButtonsDown(w.WPAD_BUTTON_UP, 0) and which > 0:
        which -= 1
    
    if w.WPAD_ButtonsDown(w.WPAD_BUTTON_DOWN, 0) and which < which_max:
        which += 1
    
    if w.WPAD_ButtonsDown(w.WPAD_BUTTON_A, 0):
        start[which]()
    
    for i in range(which_max):
        print(("> " if i == which else "  ") + start[i].__name__)
        
    w.update()
