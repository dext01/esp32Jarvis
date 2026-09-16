#!/usr/bin/env python3
"""
Тест кнопки: слушаем EVT через IndicatorEsp32 + callback.
Жми кнопку BOOT — должно печатать 'MUTE -> on/off' и менять индикацию.

Запуск:
    python3 test_button.py
"""

import asyncio
import logging
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from indicator import IndicatorEsp32  # noqa

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)

muted = False


async def on_mute(new_state: bool) -> None:
    global muted
    muted = new_state
    print(f"\n>>> MUTE -> {'ON' if new_state else 'OFF'}  <<<\n")


async def main() -> None:
    ind = IndicatorEsp32(port="/dev/ttyUSB0", on_mute_change=on_mute)
    await ind.open()
    # для наглядности: чтобы плата не показывала boot-дыхание, переведём в idle
    await ind.set_state("idle")

    print("Слушаю кнопку 30 секунд. Жми BOOT (коротко = mute toggle, "
          "долго = btn_long).")
    print("Ctrl+C — выйти.\n")

    try:
        await asyncio.sleep(30.0)
    finally:
        print("\nЗакрываю порт...")
        await ind.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nПрервано.")
