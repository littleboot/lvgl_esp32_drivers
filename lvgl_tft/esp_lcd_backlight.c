/**
 * @file esp_lcd_backlight.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <soc/gpio_sig_map.h>
#include "esp_lcd_backlight.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/ledc_periph.h" // to invert LEDC output on IDF version < v4.3
#include "esp_idf_version.h"
#if ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(5,0,0)
#include "rom/gpio.h"
#endif
typedef struct {
    bool pwm_control; // true: LEDC is used, false: GPIO is used
    int index;        // Either GPIO or LEDC channel
} disp_backlight_t;

static const char *TAG = "disp_backlight";

disp_backlight_h disp_backlight_new(const disp_backlight_config_t *config)
{
    // Check input parameters
    if (config == NULL)
        return NULL;
    if (!GPIO_IS_VALID_OUTPUT_GPIO(config->gpio_num)) {
        ESP_LOGW(TAG, "Invalid GPIO number");
        return NULL;
    }
    disp_backlight_t *bckl_dev = calloc(1, sizeof(disp_backlight_t));
    if (bckl_dev == NULL){
        ESP_LOGW(TAG, "Not enough memory");
        return NULL;
    }

    if (config->pwm_control){
        // Configure LED (Backlight) pin as PWM for Brightness control.
        bckl_dev->pwm_control = true;
        bckl_dev->index = config->channel_idx;
        const ledc_channel_config_t LCD_backlight_channel = {
            .gpio_num = config->gpio_num,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = config->channel_idx,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = config->timer_idx,
            .duty = 0,
            .hpoint = 0,
            .flags.output_invert = config->output_invert
        };
        const ledc_timer_config_t LCD_backlight_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_10_BIT,
            .timer_num = config->timer_idx,
            .freq_hz = 5000,
            .clk_cfg = LEDC_AUTO_CLK};

        ESP_ERROR_CHECK(ledc_timer_config(&LCD_backlight_timer));
        ESP_ERROR_CHECK(ledc_channel_config(&LCD_backlight_channel));
    }
    else
    {
        // Configure GPIO for output
        bckl_dev->index = config->gpio_num;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5,0,0)
        gpio_pad_select_gpio(config->gpio_num);
#else
        esp_rom_gpio_pad_select_gpio(config->gpio_num);
#endif
        ESP_ERROR_CHECK(gpio_set_direction(config->gpio_num, GPIO_MODE_OUTPUT));
        gpio_iomux_out(config->gpio_num, SIG_GPIO_OUT_IDX, config->output_invert);
    }

    return (disp_backlight_h)bckl_dev;
}

void disp_backlight_set(disp_backlight_h bckl, int brightness_percent)
{
    // Check input parameters
    if (bckl == NULL)
        return;
    if (brightness_percent > 100)
        brightness_percent = 100;
    if (brightness_percent < 0)
        brightness_percent = 0;

    disp_backlight_t *bckl_dev = (disp_backlight_t *) bckl;
    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);

    if (bckl_dev->pwm_control) {
        uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, bckl_dev->index, duty_cycle));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, bckl_dev->index));
    } else {
        ESP_ERROR_CHECK(gpio_set_level(bckl_dev->index, brightness_percent));
    }
}

void disp_backlight_delete(disp_backlight_h bckl)
{
    if (bckl == NULL)
        return;

    disp_backlight_t *bckl_dev = (disp_backlight_t *) bckl;
    if (bckl_dev->pwm_control) {
        ledc_stop(LEDC_LOW_SPEED_MODE, bckl_dev->index, 0);
    } else {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5,0,0)
        gpio_pad_select_gpio(bckl_dev->index);
#else
        esp_rom_gpio_pad_select_gpio(bckl_dev->index);
#endif
    }
    free (bckl);
}
