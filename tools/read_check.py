#!/usr/bin/env python3
"""Проверяем TX платы на 115200 — работает ли после смены порта."""
import serial, time

s = serial.Serial()
s.port = '/dev/ttyUSB0'
s.baudrate = 115200
s.timeout = 0.5
s.dtr = False
s.rts = False
s.open()

print(f"Открыл {s.port} @ {s.baudrate}, слушаю 5 секунд...\n")

# сначала пошлём ping — плата должна ответить PONG
time.sleep(0.3)
s.reset_input_buffer()
s.write(b"ping\n")
s.flush()

start = time.time()
buf = b''
lines = []
raw_bytes = 0
non_ascii = 0

while time.time() - start < 5.0:
    chunk = s.read(200)
    if chunk:
        raw_bytes += len(chunk)
        non_ascii += sum(1 for b in chunk if b > 127 or (b < 32 and b not in (9, 10, 13)))
        buf += chunk
        while b'\n' in buf:
            line, _, buf = buf.partition(b'\n')
            text = line.decode(errors='replace').strip()
            if text:
                print(f"  <- {text!r}")
                lines.append(text)

s.close()
print(f"\n=== получено байт: {raw_bytes}, строк: {len(lines)}, не-ASCII байт: {non_ascii} ===")
if any('READY' in l or 'PONG' in l or 'OK' in l or 'EVT' in l for l in lines):
    print("TX РАБОТАЕТ - видим осмысленные сообщения от платы!")
elif raw_bytes > 0 and non_ascii > raw_bytes * 0.3:
    print("TX всё ещё мусорит (много не-ASCII байт)")
elif raw_bytes == 0:
    print("Полная тишина — плата ничего не шлёт (или baudrate неверный)")
else:
    print("Что-то приходит, но не похоже на наш протокол")
