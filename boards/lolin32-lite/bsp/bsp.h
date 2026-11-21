/*
 * bsp.h — LOLIN32 Lite board support.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef LOLIN32_LITE_BSP_H
#define LOLIN32_LITE_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

/* The on-board LED is on GPIO5 and lit when the pin is low. */
#define RH_LED_PIN              5
#define RH_LED_ACTIVE_HIGH      0

#define RH_CONSOLE_UART_NUM     0
#define RH_CONSOLE_TX_PIN       1
#define RH_CONSOLE_RX_PIN       3
#define RH_CONSOLE_BAUDRATE     115200

/* The I2C bus (ESP-0012): not a header the board brings out, so two pins free
 * of the LED, the console and the strapping pins. */
#define RH_I2C0_SDA_PIN         23
#define RH_I2C0_SCL_PIN         19

void bsp_register_hals(void);
void bsp_init(void);

#ifdef __cplusplus
}
#endif

#endif /* LOLIN32_LITE_BSP_H */
