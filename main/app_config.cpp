#include "app_priv.h"

#include <ctype.h>

#include "driver/usb_serial_jtag.h"
#include "esp_system.h"

static const char *APP_CFG_NS = "app_cfg";
static const char *APP_CFG_KEY_WIFI_SSID = "wifi_ssid";
static const char *APP_CFG_KEY_WIFI_PASS = "wifi_pass";

static const char *skip_ws(const char *text)
{
    while (text != NULL && *text != '\0' && isspace((unsigned char)*text))
    {
        ++text;
    }
    return text;
}

static void trim_line(char *line)
{
    if (line == NULL)
    {
        return;
    }

    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    {
        line[len - 1] = '\0';
        --len;
    }
}

static bool parse_next_token(const char **cursor, char *out, size_t out_size)
{
    if (cursor == NULL || *cursor == NULL || out == NULL || out_size == 0)
    {
        return false;
    }

    const char *p = skip_ws(*cursor);
    if (p == NULL || *p == '\0')
    {
        return false;
    }

    bool quoted = false;
    char quote_char = '\0';
    if (*p == '"' || *p == '\'')
    {
        quoted = true;
        quote_char = *p++;
    }

    bool truncated = false;
    bool quote_closed = !quoted;
    size_t used = 0;

    while (*p != '\0')
    {
        if (quoted)
        {
            if (*p == quote_char)
            {
                quote_closed = true;
                ++p;
                break;
            }
        }
        else if (isspace((unsigned char)*p))
        {
            break;
        }

        if (used + 1 < out_size)
        {
            out[used++] = *p;
        }
        else
        {
            truncated = true;
        }
        ++p;
    }

    out[used] = '\0';
    *cursor = skip_ws(p);

    if (truncated || !quote_closed)
    {
        return false;
    }

    if (quoted)
    {
        return true;
    }
    return used > 0;
}

static bool parse_wifi_set_args(const char *args, char *ssid_out, size_t ssid_out_size,
                                char *pass_out, size_t pass_out_size)
{
    const char *cursor = args;
    if (!parse_next_token(&cursor, ssid_out, ssid_out_size))
    {
        return false;
    }
    if (!parse_next_token(&cursor, pass_out, pass_out_size))
    {
        return false;
    }
    cursor = skip_ws(cursor);
    return (cursor != NULL && *cursor == '\0');
}

static void app_config_apply_defaults(void)
{
    snprintf(g_wifi_config.wifi_ssid, sizeof(g_wifi_config.wifi_ssid), "%s", WIFI_SSID_LOCAL);
    snprintf(g_wifi_config.wifi_pass, sizeof(g_wifi_config.wifi_pass), "%s", WIFI_PASS_LOCAL);
    g_wifi_config.wifi_override_active = false;
}

void app_config_load_from_nvs(void)
{
    app_config_apply_defaults();

    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(APP_CFG_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK)
    {
        ESP_LOGI(APP_TAG, "config: using built-in Wi-Fi defaults");
        return;
    }

    char ssid[APP_WIFI_SSID_MAX_LEN + 1] = {0};
    char pass[APP_WIFI_PASS_MAX_LEN + 1] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(pass);

    esp_err_t ssid_err = nvs_get_str(nvs, APP_CFG_KEY_WIFI_SSID, ssid, &ssid_len);
    esp_err_t pass_err = nvs_get_str(nvs, APP_CFG_KEY_WIFI_PASS, pass, &pass_len);
    nvs_close(nvs);

    if (ssid_err == ESP_OK && pass_err == ESP_OK && ssid[0] != '\0')
    {
        snprintf(g_wifi_config.wifi_ssid, sizeof(g_wifi_config.wifi_ssid), "%s", ssid);
        snprintf(g_wifi_config.wifi_pass, sizeof(g_wifi_config.wifi_pass), "%s", pass);
        g_wifi_config.wifi_override_active = true;
        ESP_LOGI(APP_TAG, "config: loaded saved Wi-Fi override for SSID '%s'", g_wifi_config.wifi_ssid);
        return;
    }

    ESP_LOGI(APP_TAG, "config: no saved Wi-Fi override, using defaults");
}

const char *app_config_wifi_ssid(void)
{
    return g_wifi_config.wifi_ssid;
}

const char *app_config_wifi_pass(void)
{
    return g_wifi_config.wifi_pass;
}

bool app_config_wifi_override_active(void)
{
    return g_wifi_config.wifi_override_active;
}

esp_err_t app_config_set_wifi_override(const char *ssid, const char *pass)
{
    if (ssid == NULL || ssid[0] == '\0' || pass == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t ssid_len = strlen(ssid);
    size_t pass_len = strlen(pass);
    if (ssid_len > APP_WIFI_SSID_MAX_LEN || pass_len > APP_WIFI_PASS_MAX_LEN)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(APP_CFG_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(nvs, APP_CFG_KEY_WIFI_SSID, ssid);
    if (err == ESP_OK)
    {
        err = nvs_set_str(nvs, APP_CFG_KEY_WIFI_PASS, pass);
    }
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (err == ESP_OK)
    {
        snprintf(g_wifi_config.wifi_ssid, sizeof(g_wifi_config.wifi_ssid), "%s", ssid);
        snprintf(g_wifi_config.wifi_pass, sizeof(g_wifi_config.wifi_pass), "%s", pass);
        g_wifi_config.wifi_override_active = true;
    }

    return err;
}

esp_err_t app_config_clear_wifi_override(void)
{
    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(APP_CFG_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_err_t erase_ssid_err = nvs_erase_key(nvs, APP_CFG_KEY_WIFI_SSID);
    esp_err_t erase_pass_err = nvs_erase_key(nvs, APP_CFG_KEY_WIFI_PASS);
    if (erase_ssid_err != ESP_OK && erase_ssid_err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(nvs);
        return erase_ssid_err;
    }
    if (erase_pass_err != ESP_OK && erase_pass_err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(nvs);
        return erase_pass_err;
    }

    err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    app_config_apply_defaults();
    return ESP_OK;
}

static void app_console_print_help(void)
{
    ESP_LOGI(APP_TAG, "console commands:");
    ESP_LOGI(APP_TAG, "  wifi show");
    ESP_LOGI(APP_TAG, "  wifi set <ssid> <password>");
    ESP_LOGI(APP_TAG, "  wifi set \"My SSID\" \"My Password\"");
    ESP_LOGI(APP_TAG, "  wifi clear");
    ESP_LOGI(APP_TAG, "  wifi reboot");
}

static void app_console_handle_wifi(const char *args)
{
    char subcmd[16] = {0};
    const char *cursor = args;
    if (!parse_next_token(&cursor, subcmd, sizeof(subcmd)))
    {
        app_console_print_help();
        return;
    }

    if (strcmp(subcmd, "show") == 0)
    {
        const size_t pass_len = strlen(app_config_wifi_pass());
        ESP_LOGI(APP_TAG, "wifi source : %s",
                 app_config_wifi_override_active() ? "NVS override" : "wifi_local.h defaults");
        ESP_LOGI(APP_TAG, "wifi ssid   : %s", app_config_wifi_ssid());
        ESP_LOGI(APP_TAG, "wifi pass   : %s (%u chars)",
                 pass_len == 0 ? "<empty>" : "********", (unsigned)pass_len);
        return;
    }

    if (strcmp(subcmd, "set") == 0)
    {
        char ssid[APP_WIFI_SSID_MAX_LEN + 1] = {0};
        char pass[APP_WIFI_PASS_MAX_LEN + 1] = {0};
        if (!parse_wifi_set_args(cursor, ssid, sizeof(ssid), pass, sizeof(pass)))
        {
            ESP_LOGW(APP_TAG, "usage: wifi set <ssid> <password>");
            ESP_LOGW(APP_TAG, "or:    wifi set \"My SSID\" \"My Password\"");
            return;
        }

        esp_err_t err = app_config_set_wifi_override(ssid, pass);
        if (err != ESP_OK)
        {
            ESP_LOGE(APP_TAG, "config: save failed: %s", esp_err_to_name(err));
            return;
        }

        ESP_LOGI(APP_TAG, "config: saved Wi-Fi override for SSID '%s'", ssid);
        ESP_LOGI(APP_TAG, "config: run 'wifi reboot' to apply now");
        return;
    }

    if (strcmp(subcmd, "clear") == 0)
    {
        esp_err_t err = app_config_clear_wifi_override();
        if (err != ESP_OK)
        {
            ESP_LOGE(APP_TAG, "config: clear failed: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(APP_TAG, "config: Wi-Fi override cleared (defaults restored)");
        ESP_LOGI(APP_TAG, "config: run 'wifi reboot' to apply now");
        return;
    }

    if (strcmp(subcmd, "reboot") == 0)
    {
        ESP_LOGW(APP_TAG, "config: rebooting now");
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
        return;
    }

    app_console_print_help();
}

static void app_console_process_line(char *line)
{
    trim_line(line);
    const char *cursor = skip_ws(line);
    if (cursor == NULL || *cursor == '\0')
    {
        return;
    }

    char command[16] = {0};
    if (!parse_next_token(&cursor, command, sizeof(command)))
    {
        return;
    }

    if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0)
    {
        app_console_print_help();
        return;
    }

    if (strcmp(command, "wifi") == 0)
    {
        app_console_handle_wifi(cursor);
        return;
    }

    ESP_LOGW(APP_TAG, "console: unknown command '%s'", command);
    app_console_print_help();
}

void app_config_boot_console_window(uint32_t timeout_ms)
{
    if (timeout_ms == 0)
    {
        return;
    }

    usb_serial_jtag_driver_config_t usb_cfg = {};
    usb_cfg.tx_buffer_size = 512;
    usb_cfg.rx_buffer_size = 128;
    esp_err_t usb_err = usb_serial_jtag_driver_install(&usb_cfg);
    if (usb_err != ESP_OK && usb_err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(APP_TAG, "console: usb serial driver install failed: %s", esp_err_to_name(usb_err));
        return;
    }

    ESP_LOGI(APP_TAG, "console: %u s config window open (type 'help')", (unsigned)(timeout_ms / 1000U));

    uint32_t start_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;

    char line[192] = {0};
    size_t line_len = 0;
    uint8_t buf[32] = {0};

    while (true)
    {
        uint32_t now_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
        if ((int32_t)(now_ms - start_ms) >= (int32_t)timeout_ms)
        {
            break;
        }

        int read = usb_serial_jtag_read_bytes(buf, sizeof(buf), pdMS_TO_TICKS(100));
        if (read <= 0)
        {
            continue;
        }

        for (int i = 0; i < read; ++i)
        {
            char c = (char)buf[i];
            if (c == '\r')
            {
                continue;
            }

            if (c == '\n')
            {
                line[line_len] = '\0';
                app_console_process_line(line);
                line_len = 0;
                line[0] = '\0';
                continue;
            }

            if (line_len + 1 < sizeof(line))
            {
                line[line_len++] = c;
            }
            else
            {
                line_len = 0;
                line[0] = '\0';
                ESP_LOGW(APP_TAG, "console: input line too long, dropped");
            }
        }
    }

    esp_err_t uninstall_err = usb_serial_jtag_driver_uninstall();
    if (uninstall_err != ESP_OK)
    {
        ESP_LOGW(APP_TAG, "console: usb serial driver uninstall failed: %s", esp_err_to_name(uninstall_err));
    }
    ESP_LOGI(APP_TAG, "console: config window closed");
}
