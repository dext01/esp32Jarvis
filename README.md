# esp32Jarvis

Модуль 5 голосового ассистента «Jarvis» (командный курсовой проект НГУ):
LED-индикатор состояния и кнопка mute на ESP32, подключаемые к оркестратору
по USB-serial.

Внутри — прошивка и Python-клиент. Оркестратор и остальные модули живут в
[gvbelavin/Jarvis_from_Spartans](https://github.com/gvbelavin/Jarvis_from_Spartans);
клиент из этого репозитория (`python/indicator.py`) туда включён как копия.

## Что делает

- Показывает на светодиоде текущее состояние оркестратора:
  `boot` / `idle` / `listening` / `thinking` / `speaking` / `guest` / `error`.
- Кнопка BOOT на плате — локальный mute микрофона:
  - **короткое нажатие** — toggle mute (плата сразу локально рисует паттерн muted,
    даже если хост не читает EVT — mute «не залипает» при обрыве связи);
  - **длинное нажатие (≥ 800 мс)** — шлёт `EVT btn_long` (зарезервировано).

Полное описание протокола — в [`protocol.md`](protocol.md).

## Железо

| Что | Куда |
|---|---|
| Плата | ESP32 DevKit (ESP32-D0WD-V3, CP210x USB-UART) |
| LED | GPIO2 (LEDC PWM, канал 0, 5 кГц, 8 бит) |
| Кнопка | GPIO0 (BOOT), внутренний pull-up |
| USB | 115200 8N1 |

На DevKit-плате GPIO2 разведён на встроенный синий светодиод, отдельная
пайка не нужна. Внешний светодиод (если нужен) — через резистор 220–330 Ω на
землю.

## Структура

```
esp32Jarvis/
├── firmware/                прошивка (PlatformIO, Arduino framework)
│   ├── platformio.ini
│   └── src/main.cpp
├── python/                  клиент для оркестратора
│   ├── indicator.py         IndicatorEsp32 + IndicatorNoop, async
│   ├── test_indicator.py    прогон всех LED-состояний по 15 с
│   └── test_button.py       проверка колбэка кнопки (EVT)
├── tools/                   отладочные утилиты
│   ├── read_button.py       сырое чтение EVT-строк
│   └── read_check.py        сырое чтение с портом (проверка TX)
├── protocol.md              спецификация serial-протокола
└── LICENSE                  MIT
```

## Прошивка

```bash
cd firmware
pio run -t upload
pio device monitor            # опционально, увидеть READY / state=boot / EVT
```

В `platformio.ini` порт зашит как `/dev/ttyUSB0`. Если у вас другой —
поменять `upload_port` и `monitor_port`.

## Python-клиент

Зависимость одна: `pyserial>=3.5`.

```python
import asyncio
from indicator import IndicatorEsp32

async def on_mute(muted: bool) -> None:
    print("mute:", muted)

async def main():
    ind = IndicatorEsp32(port="/dev/ttyUSB0", on_mute_change=on_mute)
    await ind.open()
    await ind.set_state("listening")
    await asyncio.sleep(5)
    await ind.close()

asyncio.run(main())
```

Если плата не подключена, `IndicatorEsp32.open()` **не бросает** —
пишет warning в лог и `set_state()` становится no-op. Оркестратор от этого
не падает.

Для CI/тестов без железа есть `IndicatorNoop` с тем же интерфейсом.

### Проверка на реальной плате

```bash
cd python
python3 test_indicator.py     # прогон всех состояний по 15 с
python3 test_button.py        # 30 с слушаем кнопку, печатаем MUTE
```

## Интеграция с оркестратором

`python/indicator.py` подключается в
[`gvbelavin/Jarvis_from_Spartans`](https://github.com/gvbelavin/Jarvis_from_Spartans)
через обёртку `IndicatingAudioAdapter`, которая вставляет `set_state()` в
скрытые точки внутри `listen_once()` (между wake word и записью, между
записью и STT). Флаг mute блокирует запуск wake word — микрофон не
открывается, пока mute=on.

Флаги оркестратора:

```bash
python app.py --mock --once                          # с реальной платой, если есть
python app.py --mock --once --no-indicator           # заведомо без платы
python app.py --mock --once --indicator-port /dev/ttyUSB1
```

## Известное про USB

- Хост НЕ должен дёргать DTR/RTS при открытии порта — иначе плата резетится
  (`indicator.py::_open_blocking()` явно ставит оба в False).
- Если TX платы приходит мусором — попробовать **другой физический USB-порт
  компа**. У меня один порт давал битые байты, соседний — чистый вывод.
  Проблема была в порту, не в плате и не в кабеле.
