# Captures USB serial from the Cardputer without toggling DTR/RTS (no reset). Reopens if the port
# drops (reboot).  python tools/dev/listen.py <port> <secs>
import serial, sys, time
port = sys.argv[1]; secs = float(sys.argv[2])
buf = b""; end = time.time() + secs
while time.time() < end:
    try:
        s = serial.Serial()
        s.port = port; s.baudrate = 115200; s.timeout = 0.2
        s.dtr = False; s.rts = False
        s.open()
        while time.time() < end:
            buf += s.read(4096)
        s.close()
    except (serial.SerialException, OSError) as e:
        time.sleep(0.2)
txt = buf.decode(errors="replace")
print(txt[-5000:])
print("--- %d bytes" % len(buf))
