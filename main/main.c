/*
 * Task 1 - Option A (ESP-IDF)
 * What this program does:
 *   1. Reads Wi-Fi name and password from flash (NVS).
 *      If nothing is saved, asks you to type them in the serial monitor.
 *   2. Connects to Wi-Fi and reconnects if the connection is lost.
 *   3. Gets the real time from the internet (SNTP).
 *   4. Sends a heartbeat message to an MQTT broker every 5 seconds.
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "esp_err.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"

#include "mqtt_client.h"

#define MQTT_BROKER "mqtt://broker.hivemq.com"
#define MQTT_TOPIC  "iot_assignment/dinesh/heartbeat"

/*
  Two simple flags that tell the heartbeat task what is working
  "volatile" means the value can be changed by another function,
  so the program must always read the latest value
 */
static volatile bool wifi_connected = false;
static volatile bool mqtt_connected = false;

static bool sntp_started = false;   // make sure SNTP starts only once


//Saving and loading Wi-Fi details in flash (NVS)

// Save SSID and password. Returns ESP_OK if everything worked
static esp_err_t save_wifi(const char *ssid, const char *pass)
{
    nvs_handle_t handle;

    esp_err_t ret = nvs_open("wifi_data", NVS_READWRITE, &handle);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = nvs_set_str(handle, "ssid", ssid);
    if (ret == ESP_OK)
    {
        ret = nvs_set_str(handle, "password", pass);
    }
    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);   // commit = really write to flash
    }

    nvs_close(handle);
    return ret;
}

// Load SSID and password. Returns ESP_ERR_NVS_NOT_FOUND if nothing is saved
static esp_err_t load_wifi(char *ssid, size_t ssid_size,
                           char *pass, size_t pass_size)
{
    nvs_handle_t handle;

    esp_err_t ret = nvs_open("wifi_data", NVS_READONLY, &handle);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = nvs_get_str(handle, "ssid", ssid, &ssid_size);
    if (ret == ESP_OK)
    {
        ret = nvs_get_str(handle, "password", pass, &pass_size);
    }

    nvs_close(handle);
    return ret;
}


// Reading text typed in the serial monitor

static void read_line(const char *question, char *buffer, size_t size)
{
    size_t length = 0;   /* how many characters are typed so far */
 
    printf("%s", question);
    fflush(stdout);
 
    while (1)
    {
        int c = getchar();   /* returns EOF (-1) if no key was pressed */
 
        if (c == EOF)
        {
            vTaskDelay(pdMS_TO_TICKS(20));   /* nothing typed, wait a little */
            continue;
        }
 
        if (c == '\r' || c == '\n')          /* Enter key */
        {
            if (length > 0)                  /* ignore empty lines */
            {
                buffer[length] = '\0';       /* end of the text */
                printf("\n");
                return;
            }
        }
        else if ((c == 8 || c == 127) && length > 0)   /* Backspace key */
        {
            length--;
            printf("\b \b");                 /* erase the last character on screen */
            fflush(stdout);
        }
        else if (c >= 32 && c < 127 && length < size - 1)   /* normal character */
        {
            buffer[length] = (char)c;
            length++;
            putchar(c);                      /* show what was typed */
            fflush(stdout);
        }
    }
}


// Wi-Fi events 

// ESP-IDF calls this function when a Wi-Fi or IP event happens 
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // Wi-Fi driver is ready, so connect
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // Connection lost: update the flag and try again (auto-reconnect)
        wifi_event_sta_disconnected_t *info = (wifi_event_sta_disconnected_t *)event_data;
        printf("[WARN] Wi-Fi disconnected, reason code: %d. Reconnecting...\n", info->reason);
        vTaskDelay(pdMS_TO_TICKS(2000));   /* simple 2 second wait before retrying */
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        printf("[INFO] Wi-Fi connected, IP received\n");

        // Start SNTP (internet time) once. Do it BEFORE setting the flag
        if (!sntp_started)
        {
            esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            if (esp_netif_sntp_init(&config) == ESP_OK)
            {
                sntp_started = true;
            }
            else
            {
                printf("[ERROR] SNTP start failed\n");
            }
        }

        wifi_connected = true;
    }
}

//  MQTT events 

// ESP-IDF calls this function when the MQTT connection changes 
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    if (event_id == MQTT_EVENT_CONNECTED)
    {
        printf("[INFO] MQTT connected\n");
        mqtt_connected = true;
    }
    else if (event_id == MQTT_EVENT_DISCONNECTED)
    {
        printf("[WARN] MQTT disconnected\n");
        mqtt_connected = false;
    }
}


// FreeRTOS task: send heartbeat every 5 seconds 

static void heartbeat_task(void *arg)
{
    // Step 1: wait here until Wi-Fi is connected 
    while (!wifi_connected)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Step 2: wait up to 10 seconds for the internet time
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) == ESP_OK)
    {
        printf("[INFO] Time synchronized\n");
    }
    else
    {
        printf("[WARN] Time not synchronized yet\n");
    }
    printf("[INFO] Connecting to MQTT broker: %s\n", MQTT_BROKER);
    // Step 3: create and start the MQTT client 
    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = MQTT_BROKER
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_config);
    if (client == NULL)
    {
        printf("[ERROR] MQTT client creation failed\n");
        vTaskDelete(NULL);   // stop this task
    }

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    // Step 4: forever loop - send a message every 5 seconds
    while (1)
    {
        // Send only if both Wi-Fi and MQTT are connected 
        if (wifi_connected && mqtt_connected)
        {
            int64_t uptime = esp_timer_get_time() / 1000000;   // seconds since boot

            time_t now;
            time(&now);                          // real time (seconds)

            char message[150];
            snprintf(message, sizeof(message),
                     "{\"device_id\":\"ESP32\",\"uptime\":%lld,\"status\":\"online\",\"timestamp\":%lld}",
                     (long long)uptime, (long long)now);

            int id = esp_mqtt_client_publish(client, MQTT_TOPIC, message, 0, 1, 0);
            if (id >= 0)
            {
                printf("[INFO] Heartbeat sent: %s\n", message);
            }
            else
            {
                printf("[ERROR] Heartbeat failed\n");
            }
        }
        else
        {
            printf("[WARN] Network not ready, skipping heartbeat\n");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));   // wait 5 seconds
    }
}


// Main program 

void app_main(void)
{
    //1. Start flash storage (NVS). Erase and retry if it is full or old
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);   // if this still fails, print the error and stop

    // 2. Start the network system and Wi-Fi driver
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init));

    // 3. Connect our event handler function to Wi-Fi and IP events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               wifi_event_handler, NULL));

    // 4. Get Wi-Fi details: from flash if saved, otherwise ask the user
    char ssid[33];
    char password[65];

    ret = load_wifi(ssid, sizeof(ssid), password, sizeof(password));
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        printf("\nNo Wi-Fi details saved.\n");
        read_line("Enter Wi-Fi SSID: ", ssid, sizeof(ssid));
        read_line("Enter Wi-Fi password: ", password, sizeof(password));

        if (save_wifi(ssid, password) != ESP_OK)
        {
            printf("[ERROR] Could not save Wi-Fi details\n");
            return;
        }
    }
    else if (ret != ESP_OK)
    {
        printf("[ERROR] Could not read Wi-Fi details: %s\n", esp_err_to_name(ret));
        return;
    }

    // 5. Give the details to the Wi-Fi driver and start it
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);   // keep the radio fully awake for stable connections

    // 6. Start the heartbeat FreeRTOS task
    if (xTaskCreate(heartbeat_task, "heartbeat_task", 6144, NULL, 5, NULL) != pdPASS)
    {
        printf("[ERROR] Could not create heartbeat task\n");
    }
}