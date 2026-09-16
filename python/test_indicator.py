#!/usr/bin/env python3
"""
Standalone-тест модуля indicator.py — прогоняет полный цикл состояний
через IndicatorEsp32, не запуская весь оркестратор.

Запуск:
    python3 test_indicator.py
"""

import asyncio
import logging
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from indicator import IndicatorEsp32  # noqa

logging.basicConfig(
    level=logging.DEBUG,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)

SCENARIO = [
    ("idle",       15.0),
    ("listening",  15.0),
    ("thinking",   15.0),
    ("speaking",   15.0),
    ("guest",      15.0),
    ("error",      15.0),
    ("idle",        3.0),
]


async def main() -> None:
    ind = IndicatorEsp32(port="/dev/ttyUSB0")
    await ind.open()

    print("\nПрогон сценария (Ctrl+C — прервать):\n")
    try:
        for state, hold in SCENARIO:
            print(f"  -> {state} (держим {hold} с)")
            await ind.set_state(state)
            await asyncio.sleep(hold)
    finally:
        print("\nЗакрываю порт (плата останется в idle)...")
        await ind.close()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nПрервано.")
