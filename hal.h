/*
 * hal.h - tiny hardware abstraction layer
 *
 * The point of this file is so main.c doesn't care what board it's running on.
 * On real hardware these would call into the STM32 HAL / ESP-IDF / whatever.
 * Here they're just stubs (see hal.c) so the logic runs anywhere.
 */
#ifndef HAL_H
#define HAL_H

#include <stdint.h>

/* set up gpio / adc / etc. call once at boot */
void hal_init(void);

/* read the soil sensor. returns moisture as a percent, 0 (dry) .. 100 (wet) */
uint8_t hal_read_moisture(void);

/* pump control - the actuator */
void hal_pump_on(void);
void hal_pump_off(void);

/* status LED */
void hal_led_toggle(void);

/*
 * The button ISR lives in main.c (it needs the semaphore handle), but the HAL
 * gives us a way to register it. On a real MCU this hooks the EXTI/pin-change
 * interrupt. In the sim, hal.c fires it on a timer so we can test the path.
 */
void hal_register_button_isr(void (*isr)(void));

#endif /* HAL_H */
