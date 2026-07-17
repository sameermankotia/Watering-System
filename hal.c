/*
 * hal.c - fake hardware so the project runs without a real board.
 *
 * Everything in here is a stub. The moisture reading is simulated: it slowly
 * dries out over time, and jumps back up when the pump runs. That way the
 * state machine in main.c actually has something to react to.
 *
 * On real hardware you'd delete most of this and call your MCU's HAL instead.
 */
#include "hal.h"
#include <stdio.h>

/* simulated soil moisture, starts kinda damp */
static int sim_moisture = 55;
static int pump_running = 0;

/* button isr callback that main.c registers */
static void (*button_cb)(void) = NULL;

/* rough counter so we can fake a button press once in a while (sim only) */
static int tick_count = 0;

void hal_init(void)
{
    printf("[HAL] init - gpio/adc ready (simulated)\n");
    sim_moisture = 55;
    pump_running = 0;
}

uint8_t hal_read_moisture(void)
{
    /*
     * fake physics: soil dries out a little every read. if the pump is on it
     * gets wetter fast. clamp to 0..100 so we don't wrap around.
     */
    if (pump_running) {
        sim_moisture += 8;
    } else {
        sim_moisture -= 2;  /* slow dry out */
    }

    if (sim_moisture > 100) sim_moisture = 100;
    if (sim_moisture < 0)   sim_moisture = 0;

    /* every ~15 reads pretend someone pressed the manual button */
    tick_count++;
    if (tick_count % 15 == 0 && button_cb != NULL) {
        printf("[HAL] (sim) button pressed!\n");
        button_cb();
    }

    return (uint8_t)sim_moisture;
}

void hal_pump_on(void)
{
    if (!pump_running) {
        printf("[HAL] >>> PUMP ON\n");
    }
    pump_running = 1;
}

void hal_pump_off(void)
{
    if (pump_running) {
        printf("[HAL] <<< PUMP OFF\n");
    }
    pump_running = 0;
}

void hal_led_toggle(void)
{
    /* on a board this flips a gpio. here we don't spam the console. */
}

void hal_register_button_isr(void (*isr)(void))
{
    button_cb = isr;
}
