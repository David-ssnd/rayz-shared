#ifndef GPIO_INIT_H
#define GPIO_INIT_H

/**
 * @brief Initialize reset button and check for factory reset (2 second hold)
 */
void init_reset_button_and_check_factory_reset(void);

/**
 * @brief Initialize laser GPIO pin (weapon-specific)
 * @param pin The GPIO pin number to initialize as output
 */
void init_laser_gpio(int pin);

#endif // GPIO_INIT_H
