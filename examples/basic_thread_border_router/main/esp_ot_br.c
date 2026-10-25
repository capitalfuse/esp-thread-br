/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "sdkconfig.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_openthread.h"
#include "esp_openthread_border_router.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_ot_config.h"
#include "esp_ot_ota_commands.h"
#include "esp_ot_wifi_cmd.h"
#include "esp_spiffs.h"
#include "esp_vfs_eventfd.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#include "border_router_launch.h"
#include "esp_br_web.h"

#define TAG "esp_ot_br"

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

extern void wifi_reset_button_start(void);

/*
 * Convert OTA image state to a readable string.
 */
static const char *ota_state_to_string(esp_ota_img_states_t state)
{
    switch (state) {
    case ESP_OTA_IMG_NEW:
        return "NEW";
    case ESP_OTA_IMG_PENDING_VERIFY:
        return "PENDING_VERIFY";
    case ESP_OTA_IMG_VALID:
        return "VALID";
    case ESP_OTA_IMG_INVALID:
        return "INVALID";
    case ESP_OTA_IMG_ABORTED:
        return "ABORTED";
    case ESP_OTA_IMG_UNDEFINED:
        return "UNDEFINED";
    default:
        return "UNKNOWN";
    }
}

/*
 * Check the currently running OTA partition and
 * confirm the firmware image if rollback is enabled.
 */
static void check_ota_state(void)
{
    const esp_partition_t *running =
        esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGE(
            TAG,
            "Failed to get running OTA partition"
        );
        return;
    }

    ESP_LOGI(
        TAG,
        "Running firmware partition: %s @ 0x%" PRIx32,
        running->label,
        running->address
    );

    /*
     * Get the current OTA image state.
     */
    esp_ota_img_states_t ota_state;
    esp_err_t ret =
        esp_ota_get_state_partition(
            running,
            &ota_state
        );
    if (ret == ESP_OK) {
        ESP_LOGI(
            TAG,
            "OTA image state before confirmation: %s",
            ota_state_to_string(ota_state)
        );
    } else {
        ESP_LOGW(
            TAG,
            "Failed to get OTA image state: %s",
            esp_err_to_name(ret)
        );
    }

    /*
     * Confirm the currently running firmware.
     *
     * This changes ESP_OTA_IMG_PENDING_VERIFY to
     * ESP_OTA_IMG_VALID and cancels rollback.
     */
    ret =
        esp_ota_mark_app_valid_cancel_rollback();

    if (ret == ESP_OK) {
        ESP_LOGI(
            TAG,
            "OTA image confirmed as valid"
        );
    } else {
        ESP_LOGW(
            TAG,
            "OTA image confirmation failed: %s",
            esp_err_to_name(ret)
        );
    }

    /*
     * Check the state again after confirmation.
     */
    ret =
        esp_ota_get_state_partition(
            running,
            &ota_state
        );
    if (ret == ESP_OK) {
        ESP_LOGI(
            TAG,
            "OTA image state after confirmation: %s",
            ota_state_to_string(ota_state)
        );
    } else {
        ESP_LOGW(
            TAG,
            "Failed to get OTA image state after confirmation: %s",
            esp_err_to_name(ret)
        );
    }
}

static esp_err_t init_spiffs(void)
{
#if CONFIG_AUTO_UPDATE_RCP
    esp_vfs_spiffs_conf_t rcp_fw_conf = {.base_path = "/" CONFIG_RCP_PARTITION_NAME,
                                         .partition_label = CONFIG_RCP_PARTITION_NAME,
                                         .max_files = 10,
                                         .format_if_mount_failed = false};
    ESP_RETURN_ON_ERROR(esp_vfs_spiffs_register(&rcp_fw_conf), TAG, "Failed to mount rcp firmware storage");
#endif
#if CONFIG_OPENTHREAD_BR_START_WEB
    esp_vfs_spiffs_conf_t web_server_conf = {
        .base_path = "/spiffs", .partition_label = "web_storage", .max_files = 10, .format_if_mount_failed = false};
    ESP_RETURN_ON_ERROR(esp_vfs_spiffs_register(&web_server_conf), TAG, "Failed to mount web storage");
#endif
    return ESP_OK;
}

void app_main(void)
{
    /*
     * ---------------------------------------------------------
     * OTA / rollback status
     * ---------------------------------------------------------
     *
     * Check which OTA partition is currently running and
     * confirm the firmware image.
     */
    check_ota_state();

    // Used eventfds:
    // * netif
    // * task queue
    // * border router
    size_t max_eventfd = 3;

#if CONFIG_OPENTHREAD_RADIO_NATIVE || CONFIG_OPENTHREAD_RADIO_SPINEL_SPI
    // * SpiSpinelInterface (The Spi Spinel Interface needs an eventfd.)
    max_eventfd++;
#endif
#if CONFIG_OPENTHREAD_RADIO_TREL
    // * TREL reception (The Thread Radio Encapsulation Link needs an eventfd for reception.)
    max_eventfd++;
#endif
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = max_eventfd,
    };

    esp_openthread_config_t openthread_config = {
        .netif_config = ESP_NETIF_DEFAULT_OPENTHREAD(),
        .platform_config =
            {
                .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
                .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
                .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
            },
    };
    esp_rcp_update_config_t rcp_update_config = ESP_OPENTHREAD_RCP_UPDATE_CONFIG();
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(init_spiffs());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#if !CONFIG_OPENTHREAD_BR_AUTO_START && CONFIG_EXAMPLE_CONNECT_ETHERNET
// TODO: Add a mechanism for connecting ETH manually.
#error Currently we do not support a manual way to connect ETH, if you want to use ETH, please enable OPENTHREAD_BR_AUTO_START.
#endif

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("esp-ot-br"));
#if CONFIG_OPENTHREAD_CLI_OTA
    esp_set_ota_server_cert((char *)server_cert_pem_start);
#endif

#if CONFIG_OPENTHREAD_BR_START_WEB
    esp_br_web_start("/spiffs");
#endif

    launch_openthread_border_router(&openthread_config, &rcp_update_config);

wifi_reset_button_start();

}
