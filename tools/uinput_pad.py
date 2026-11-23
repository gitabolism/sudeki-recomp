#!/usr/bin/env python3
"""tools/uinput_pad.py — virtual Xbox-360 gamepad via /dev/uinput (pure ctypes).

xemu/SDL hot-plugs gamepads and polls them regardless of window focus, so
this is the scripted-START channel for the reference game (host-side only;
touches no guest code).

usage:
  tools/uinput_pad.py create      # create pad, keep running, cycle START every N s
  tools/uinput_pad.py create N    # press START for 0.5 s every N seconds (0=never)
  tools/uinput_pad.py destroy     # destroy the pad and exit
"""
import ctypes
import ctypes.util
import os
import struct
import sys
import time

libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)

UI_SET_EVBIT = 0x40045564
UI_SET_KEYBIT = 0x40045565
UI_SET_ABSBIT = 0x40045566
UI_DEV_CREATE = 0x5501
UI_DEV_DESTROY = 0x5502
ABS_CNT = 64
EV_SYN, EV_KEY, EV_ABS = 0x00, 0x01, 0x03
BTN_START, BTN_A = 315, 304
ABS_X, ABS_Y, ABS_RX, ABS_RY = 0, 1, 3, 4
# kernel 7.3.5 rejects UI_SET_ABSBIT >= 16 (no HAT axes); 360 d-pad maps to buttons
ABS_ALL = [ABS_X, ABS_Y, ABS_RX, ABS_RY]
KEYS = [300, 301, 302, 303, 304, 305, 310, 311, 312, 313, 314, 315, 316, 317, 318]


class UinputUserDev(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char * 80),
                ("bustype", ctypes.c_uint16), ("vendor", ctypes.c_uint16),
                ("product", ctypes.c_uint16), ("version", ctypes.c_uint16),
                ("ff_effects_max", ctypes.c_uint32),
                ("absmax", ctypes.c_int32 * ABS_CNT),
                ("absmin", ctypes.c_int32 * ABS_CNT),
                ("absfuzz", ctypes.c_int32 * ABS_CNT),
                ("absflat", ctypes.c_int32 * ABS_CNT)]


def create(fd):
    dev = UinputUserDev()
    dev.name = b"Microsoft X-Box 360 pad"
    dev.bustype, dev.vendor, dev.product, dev.version = 0, 0x045E, 0x028E, 0x0110
    for a in ABS_ALL:
        dev.absmin[a], dev.absmax[a] = -32767, 32767
    os.write(fd, bytes(dev))
    ioctl(fd, UI_SET_EVBIT, EV_KEY)
    ioctl(fd, UI_SET_EVBIT, EV_ABS)
    for k in KEYS:
        ioctl(fd, UI_SET_KEYBIT, k)
    for a in ABS_ALL:
        ioctl(fd, UI_SET_ABSBIT, a)
    ioctl(fd, UI_DEV_CREATE)


def ioctl(fd, req, arg=0):
    if libc.ioctl(fd, ctypes.c_ulong(req), ctypes.c_int(arg)) < 0:
        raise OSError(ctypes.get_errno(), "ioctl %x" % req)


def event(fd, typ, code, value):
    # struct input_event { timeval; __u16 type, code; __s32 value }
    buf = struct.pack("=QQHHi", 0, 0, typ, code, value)
    os.write(fd, buf)


def syn(fd):
    event(fd, EV_SYN, 0, 0)


def press(fd, btn, on):
    event(fd, EV_KEY, btn, 1 if on else 0)
    syn(fd)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "create"
    period = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    fd = os.open("/dev/uinput", os.O_RDWR | os.O_NONBLOCK)
    if mode == "destroy":
        try:
            ioctl(fd, UI_DEV_DESTROY)
        except OSError:
            pass
        os.close(fd)
        print("destroy attempted")
        return
    create(fd)
    print("pad created")
    try:
        t0 = time.time()
        while True:
            if period > 0 and int(time.time() - t0) % period < 1:
                press(fd, BTN_START, 1)
            elif period > 0:
                press(fd, BTN_START, 0)
            time.sleep(0.25)
    except KeyboardInterrupt:
        pass
    ioctl(fd, UI_DEV_DESTROY)
    os.close(fd)
    print("pad destroyed")


if __name__ == "__main__":
    main()
