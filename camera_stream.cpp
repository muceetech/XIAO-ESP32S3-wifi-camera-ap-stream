#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "esp_camera.h"
#include "esp_http_server.h"


// ============================================================
// SOFTAP SETTINGS
// ============================================================

#define WIFI_AP_SSID       "ESP32-CAMERA"
#define WIFI_AP_PASSWORD   "12345678"

#define WIFI_AP_CHANNEL    6
#define WIFI_AP_MAX_CONN   1


static const char *TAG = "XIAO_CAMERA";


// ============================================================
// XIAO ESP32-S3 SENSE - OV2640
// ============================================================

#define CAM_PIN_PWDN     21
#define CAM_PIN_RESET     1

#define CAM_PIN_XCLK     10

#define CAM_PIN_SIOD     40
#define CAM_PIN_SIOC     39

#define CAM_PIN_D7       48
#define CAM_PIN_D6       11
#define CAM_PIN_D5       12
#define CAM_PIN_D4       14
#define CAM_PIN_D3       16
#define CAM_PIN_D2       18
#define CAM_PIN_D1       17
#define CAM_PIN_D0       15

#define CAM_PIN_VSYNC    38
#define CAM_PIN_HREF     47
#define CAM_PIN_PCLK     13


// ============================================================
// MJPEG STREAM
// ============================================================

#define PART_BOUNDARY "123456789000000000000987654321"

static const char *STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary="
    PART_BOUNDARY;

static const char *STREAM_BOUNDARY =
    "\r\n--" PART_BOUNDARY "\r\n";

static const char *STREAM_PART =
    "Content-Type: image/jpeg\r\n"
    "Content-Length: %u\r\n"
    "\r\n";


// ============================================================
// CAMERA INITIALIZATION
// ============================================================

static esp_err_t camera_init()
{
    camera_config_t config = {};

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_d0 = CAM_PIN_D0;
    config.pin_d1 = CAM_PIN_D1;
    config.pin_d2 = CAM_PIN_D2;
    config.pin_d3 = CAM_PIN_D3;
    config.pin_d4 = CAM_PIN_D4;
    config.pin_d5 = CAM_PIN_D5;
    config.pin_d6 = CAM_PIN_D6;
    config.pin_d7 = CAM_PIN_D7;

    config.pin_xclk = CAM_PIN_XCLK;
    config.pin_pclk = CAM_PIN_PCLK;
    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;

    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;

    config.pin_pwdn = CAM_PIN_PWDN;
    config.pin_reset = CAM_PIN_RESET;

    config.xclk_freq_hz = 20000000;

    // JPEG directly from OV2640
    config.pixel_format = PIXFORMAT_JPEG;

    // 320 x 240
    config.frame_size = FRAMESIZE_QVGA;

    // Lower number = higher JPEG quality
    config.jpeg_quality = 12;

    // Use PSRAM
    config.fb_location = CAMERA_FB_IN_PSRAM;

    // Two frame buffers
    config.fb_count = 2;

    // Always get latest frame
    config.grab_mode = CAMERA_GRAB_LATEST;


    ESP_LOGI(TAG, "Initializing OV2640...");

    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Camera initialization failed: 0x%x",
            err
        );

        return err;
    }


    sensor_t *sensor = esp_camera_sensor_get();

    if (sensor == NULL)
    {
        ESP_LOGE(TAG, "Camera sensor not found");
        return ESP_FAIL;
    }


    sensor->set_brightness(sensor, 0);
    sensor->set_contrast(sensor, 0);
    sensor->set_saturation(sensor, 0);


    ESP_LOGI(TAG, "OV2640 initialized");

    return ESP_OK;
}


// ============================================================
// WIFI SOFTAP
// ============================================================

static void wifi_init_softap()
{
    ESP_ERROR_CHECK(
        esp_netif_init()
    );

    ESP_ERROR_CHECK(
        esp_event_loop_create_default()
    );


    esp_netif_t *ap_netif =
        esp_netif_create_default_wifi_ap();

    if (ap_netif == NULL)
    {
        ESP_LOGE(
            TAG,
            "Failed to create AP interface"
        );

        return;
    }


    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg)
    );


    wifi_config_t wifi_config = {};

    strncpy(
        (char *)wifi_config.ap.ssid,
        WIFI_AP_SSID,
        sizeof(wifi_config.ap.ssid)
    );

    strncpy(
        (char *)wifi_config.ap.password,
        WIFI_AP_PASSWORD,
        sizeof(wifi_config.ap.password)
    );


    wifi_config.ap.ssid_len =
        strlen(WIFI_AP_SSID);

    wifi_config.ap.channel =
        WIFI_AP_CHANNEL;

    wifi_config.ap.max_connection =
        WIFI_AP_MAX_CONN;

    wifi_config.ap.authmode =
        WIFI_AUTH_WPA2_PSK;


    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_AP)
    );


    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_AP,
            &wifi_config
        )
    );


    // Disable Wi-Fi power saving
    ESP_ERROR_CHECK(
        esp_wifi_set_ps(WIFI_PS_NONE)
    );


    ESP_ERROR_CHECK(
        esp_wifi_start()
    );


    ESP_LOGI(
        TAG,
        "=========================================="
    );

    ESP_LOGI(
        TAG,
        "ESP32-S3 CAMERA ACCESS POINT"
    );

    ESP_LOGI(
        TAG,
        "SSID     : %s",
        WIFI_AP_SSID
    );

    ESP_LOGI(
        TAG,
        "Password : %s",
        WIFI_AP_PASSWORD
    );

    ESP_LOGI(
        TAG,
        "IP       : 192.168.4.1"
    );

    ESP_LOGI(
        TAG,
        "=========================================="
    );
}


// ============================================================
// STREAM HANDLER
// ============================================================

static esp_err_t stream_handler(
    httpd_req_t *req
)
{
    esp_err_t res;


    res = httpd_resp_set_type(
        req,
        STREAM_CONTENT_TYPE
    );

    if (res != ESP_OK)
        return res;


    httpd_resp_set_hdr(
        req,
        "Access-Control-Allow-Origin",
        "*"
    );


    while (true)
    {
        camera_fb_t *fb =
            esp_camera_fb_get();


        if (!fb)
        {
            ESP_LOGE(
                TAG,
                "Camera capture failed"
            );

            return ESP_FAIL;
        }


        char part_buf[64];


        size_t hlen =
            snprintf(
                part_buf,
                sizeof(part_buf),
                STREAM_PART,
                fb->len
            );


        // Boundary

        res = httpd_resp_send_chunk(
            req,
            STREAM_BOUNDARY,
            strlen(STREAM_BOUNDARY)
        );


        // JPEG header

        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(
                req,
                part_buf,
                hlen
            );
        }


        // JPEG data

        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(
                req,
                (const char *)fb->buf,
                fb->len
            );
        }


        esp_camera_fb_return(fb);


        // Client disconnected

        if (res != ESP_OK)
        {
            ESP_LOGW(
                TAG,
                "Camera client disconnected"
            );

            break;
        }


        // Approximately 15 FPS

        vTaskDelay(
            pdMS_TO_TICKS(66)
        );
    }


    return res;
}


// ============================================================
// SINGLE JPEG CAPTURE
// ============================================================

static esp_err_t capture_handler(
    httpd_req_t *req
)
{
    camera_fb_t *fb =
        esp_camera_fb_get();


    if (!fb)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }


    httpd_resp_set_type(
        req,
        "image/jpeg"
    );


    esp_err_t res =
        httpd_resp_send(
            req,
            (const char *)fb->buf,
            fb->len
        );


    esp_camera_fb_return(fb);

    return res;
}


// ============================================================
// HTTP SERVER
// ============================================================

static httpd_handle_t start_webserver()
{
    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();


    config.server_port = 80;

    config.max_uri_handlers = 8;

    config.stack_size = 12288;

    config.max_open_sockets = 4;

    config.recv_wait_timeout = 10;

    config.send_wait_timeout = 10;


    httpd_handle_t server = NULL;


    if (
        httpd_start(
            &server,
            &config
        ) != ESP_OK
    )
    {
        ESP_LOGE(
            TAG,
            "Failed to start HTTP server"
        );

        return NULL;
    }


    // --------------------------------------------------------
    // /stream
    // --------------------------------------------------------

    httpd_uri_t stream_uri = {};

    stream_uri.uri = "/stream";

    stream_uri.method = HTTP_GET;

    stream_uri.handler = stream_handler;


    httpd_register_uri_handler(
        server,
        &stream_uri
    );


    // --------------------------------------------------------
    // /capture
    // --------------------------------------------------------

    httpd_uri_t capture_uri = {};

    capture_uri.uri = "/capture";

    capture_uri.method = HTTP_GET;

    capture_uri.handler = capture_handler;


    httpd_register_uri_handler(
        server,
        &capture_uri
    );


    ESP_LOGI(
        TAG,
        "=========================================="
    );

    ESP_LOGI(
        TAG,
        "HTTP CAMERA SERVER READY"
    );

    ESP_LOGI(
        TAG,
        "Stream  : http://192.168.4.1/stream"
    );

    ESP_LOGI(
        TAG,
        "Capture : http://192.168.4.1/capture"
    );

    ESP_LOGI(
        TAG,
        "=========================================="
    );


    return server;
}


// ============================================================
// MAIN
// ============================================================

extern "C" void app_main()
{
    ESP_LOGI(
        TAG,
        "=========================================="
    );

    ESP_LOGI(
        TAG,
        " XIAO ESP32-S3 SENSE"
    );

    ESP_LOGI(
        TAG,
        " OV2640 CAMERA"
    );

    ESP_LOGI(
        TAG,
        " SOFTAP CAMERA SERVER"
    );

    ESP_LOGI(
        TAG,
        " ESP-IDF 5.5.3"
    );

    ESP_LOGI(
        TAG,
        "=========================================="
    );


    // --------------------------------------------------------
    // NVS
    // --------------------------------------------------------

    esp_err_t ret =
        nvs_flash_init();


    if (
        ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND
    )
    {
        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );

        ESP_ERROR_CHECK(
            nvs_flash_init()
        );
    }


    // --------------------------------------------------------
    // Camera
    // --------------------------------------------------------

    ESP_ERROR_CHECK(
        camera_init()
    );


    // --------------------------------------------------------
    // ESP32-S3 Wi-Fi AP
    // --------------------------------------------------------

    wifi_init_softap();


    // --------------------------------------------------------
    // HTTP camera server
    // --------------------------------------------------------

    start_webserver();


    ESP_LOGI(
        TAG,
        "SYSTEM READY"
    );


    while (true)
    {
        vTaskDelay(
            pdMS_TO_TICKS(1000)
        );
    }
}
