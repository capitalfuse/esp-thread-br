/*
 * wifi_reset.c
 *
 * Pressing and holding the BOOT button (ESP32-C6-DevKitC-1: GPIO9, active-low)
 * for 3 seconds clears the Wi-Fi settings stored by esp_ot_wifi_cmd.c in its
 * custom "wifi_config" NVS namespace (keys: ssid / password), then reboots
 * the device.
 *
 * IMPORTANT: In this configuration, calling esp_wifi_restore() has no effect.
 * The SSID/PSK actually used for automatic Wi-Fi reconnection is stored in
 * the custom storage ("wifi_config" namespace), which is accessed through
 * esp_ot_wifi_config_get_ssid() / esp_ot_wifi_config_get_password().
 * Therefore, the settings must be cleared using the
 * esp_ot_wifi_config_clear() function provided by esp_ot_wifi_cmd.c.
 * This performs exactly the same operation as the
 * `wifi config clear` command in the serial console.
 *
 * PREREQUISITE: The BOOT button on GPIO9 is unrelated to the RCP pins
 * (GPIO7/8) and can be freely used as a GPIO input while the application
 * is running. GPIO8 is currently used by the LED driver, so it must not
 * be touched.
 */

#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ot_wifi_cmd.h"   /* esp_ot_wifi_config_init/clear/get_ssid, esp_ot_wifi_disconnect, esp_ot_wifi_state_get */

static const char *TAG = "wifi_reset_btn";

#define WIFI_RESET_BUTTON_GPIO   GPIO_NUM_9   /* BOOT button */
#define WIFI_RESET_HOLD_MS       3000         /* 3 seconds hold to reset */
#define WIFI_RESET_POLL_MS       100

static void wifi_reset_button_task(void *arg)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << WIFI_RESET_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    int held_ms = 0;

    while (1) {
        if (gpio_get_level(WIFI_RESET_BUTTON_GPIO) == 0) { /* push down (Low) */
            held_ms += WIFI_RESET_POLL_MS;
            if (held_ms >= WIFI_RESET_HOLD_MS) {
                ESP_LOGW(TAG, "BOOT button held %dms, clearing stored Wi-Fi config...", held_ms);

                /* Disconnect if currently connected to prevent reconnection */
                if (esp_ot_wifi_state_get() == OT_WIFI_CONNECTED) {
                    esp_ot_wifi_disconnect();
                    vTaskDelay(pdMS_TO_TICKS(200));
                }

                /* Ensure the "wifi_config" NVS namespace is open before clearing it */
                esp_err_t init_err = esp_ot_wifi_config_init();
                if (init_err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ot_wifi_config_init failed: %s", esp_err_to_name(init_err));
                } else {
                    esp_err_t clr_err = esp_ot_wifi_config_clear();
                    if (clr_err != ESP_OK) {
                        ESP_LOGE(TAG, "esp_ot_wifi_config_clear failed: %s", esp_err_to_name(clr_err));
                    } else {
                        ESP_LOGW(TAG, "Stored Wi-Fi ssid/password cleared. Rebooting...");
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(500));
                esp_restart();
            }
        } else {
            held_ms = 0; /* Reset the counter when the button is released */
        }
        vTaskDelay(pdMS_TO_TICKS(WIFI_RESET_POLL_MS));
    }
}

void wifi_reset_button_start(void)
{
    xTaskCreate(wifi_reset_button_task, "wifi_reset_btn", 3072, NULL, 5, NULL);
}