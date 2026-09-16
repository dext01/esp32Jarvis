#!/usr/bin/env python3
"""Слушаем плату 20 секунд — нажимай кнопку BOOT, смотрим прилетают ли EVT."""
import serial, time

s = serial.Serial()
s.port = '/dev/ttyUSB0'
s.baudrate = 115200
s.timeout = 0.2
s.dtr = False
s.rts = False
s.open()

print("Слушаю 20 секунд. НАЖИМАЙ КНОПКУ BOOT (короткие и длинные нажатия).\n")

start = time.time()
buf = b''
evts = []

while time.time() - start < 20.0:
    chunk = s.read(200)
    if chunk:
        buf += chunk
        while b'\n' in buf:
            line, _, buf = buf.partition(b'\n')
            text = line.decode(errors='replace').strip()
            if text:
                mark = ""
                if text.startswith("EVT"):
                    mark = "  << КНОПКА!"
                    evts.append(text)
                print(f"  <- {text!r}{mark}")

s.close()
print(f"\n=== EVT-событий кнопки: {len(evts)} ===")
for e in evts:
    print(f"  {e}")
