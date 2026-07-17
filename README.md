# Smart Plant Moisture Control System (FreeRTOS)

Small embedded project I put together to learn FreeRTOS. Idea is pretty simple:
keep a plant's soil above some moisture level. A sensor reads the soil, and if
it gets too dry we turn on a water pump for a bit, then stop and let it soak in.

Wrote it to practice the stuff that keeps coming up in embedded interviews:
tasks, scheduling, queues/semaphores, an ISR, and a state machine.

## What it does

- Reads a soil moisture sensor every second (ADC).
- Sends the reading to a controller task over a queue.
- Controller runs a small state machine: IDLE -> WATERING -> SOAKING -> back to IDLE.
- Turns a pump GPIO on/off (the "actuator").
- A push button (interrupt) lets you force a manual watering.
- A separate task blinks an LED / prints status so you can see it's alive.

## Tasks

| Task            | Prio | Job                                          |
|-----------------|------|----------------------------------------------|
| SensorTask      | 2    | read ADC, push sample to queue               |
| ControllerTask  | 3    | state machine, drives the pump               |
| StatusTask      | 1    | heartbeat LED + print current state          |

Higher number = higher priority in FreeRTOS. Controller is highest because it's
the one making decisions about the pump.

## How the pieces talk to each other

```
 [button ISR] --give--> (binary semaphore) --take--> ControllerTask
 SensorTask --xQueueSend--> (moisture queue) --xQueueReceive--> ControllerTask
 ControllerTask --> pump GPIO
```

## Building

This is written against plain FreeRTOS with a tiny hardware abstraction layer
(`hal.h` / `hal.c`) so it doesn't depend on one specific board. On real hardware
you'd swap the HAL stubs for your MCU's HAL (STM32 HAL, ESP-IDF, etc). The stubs
just simulate a sensor and print what the pump/LED would do, so you can follow
the logic without a board plugged in.

Files:
- `main.c` - tasks, queue, semaphore, state machine (the interesting part)
- `hal.h` / `hal.c` - fake hardware so it runs on a PC / any FreeRTOS port
- `FreeRTOSConfig.h` - minimal config

## Notes / TODO

- Moisture values are 0-100 (%). 0 = bone dry, 100 = soaking wet.
- Thresholds are #defines at the top of main.c, tweak them for your plant.
- TODO: add a real timer so we don't over-water (watchdog on pump run time). Kind
  of half did this with WATER_MS but could be smarter.
- TODO: debounce the button better, right now the ISR just uses a cooldown.
