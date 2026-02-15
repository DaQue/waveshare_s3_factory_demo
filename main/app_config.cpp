#include "app_priv.h"

#include <ctype.h>

#include "driver/usb_serial_jtag.h"
#include "esp_system.h"

static const char *APP_CFG_NS = "app_cfg";
static const char *APP_CFG_KEY_WIFI_SSID = "wifi_ssid";
static const char *APP_CFG_KEY_WIFI_PASS = "wifi_pass";
static const char *APP_CFG_KEY_WX_API = "wx_api_key";
static const char *APP_CFG_KEY_WX_QUERY = "wx_query";

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
    snprintf(g_wifi_config.weather_api_key, sizeof(g_wifi_config.weather_api_key), "%s", WEATHER_API_KEY_LOCAL);
    snprintf(g_wifi_config.weather_query, sizeof(g_wifi_config.weather_query), "%s", WEATHER_QUERY_LOCAL);
    g_wifi_config.weather_api_override_active = false;
    g_wifi_config.weather_query_override_active = false;
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
    char wx_api[APP_WEATHER_API_KEY_MAX_LEN + 1] = {0};
    char wx_query[APP_WEATHER_QUERY_MAX_LEN + 1] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(pass);
    size_t wx_api_len = sizeof(wx_api);
    size_t wx_query_len = sizeof(wx_query);

    esp_err_t ssid_err = nvs_get_str(nvs, APP_CFG_KEY_WIFI_SSID, ssid, &ssid_len);
    esp_err_t pass_err = nvs_get_str(nvs, APP_CFG_KEY_WIFI_PASS, pass, &pass_len);
    esp_err_t api_err = nvs_get_str(nvs, APP_CFG_KEY_WX_API, wx_api, &wx_api_len);
    esp_err_t query_err = nvs_get_str(nvs, APP_CFG_KEY_WX_QUERY, wx_query, &wx_query_len);
    nvs_close(nvs);

    if (ssid_err == ESP_OK && pass_err == ESP_OK && ssid[0] != '\0')
    {
        snprintf(g_wifi_config.wifi_ssid, sizeof(g_wifi_config.wifi_ssid), "%s", ssid);
        snprintf(g_wifi_config.wifi_pass, sizeof(g_wifi_config.wifi_pass), "%s", pass);
        g_wifi_config.wifi_override_active = true;
        ESP_LOGI(APP_TAG, "config: loaded saved Wi-Fi override for SSID '%s'", g_wifi_config.wifi_ssid);
    }
    else
    {
        ESP_LOGI(APP_TAG, "config: no saved Wi-Fi override, using defaults");
    }

    if (api_err == ESP_OK && wx_api[0] != '\0')
    {
        snprintf(g_wifi_config.weather_api_key, sizeof(g_wifi_config.weather_api_key), "%s", wx_api);
        g_wifi_config.weather_api_override_active = true;
        ESP_LOGI(APP_TAG, "config: loaded saved weather API key override (%u chars)",
                 (unsigned)strlen(g_wifi_config.weather_api_key));
    }
    else
    {
        ESP_LOGI(APP_TAG, "config: no saved weather API key override, using default");
    }

    if (query_err == ESP_OK && wx_query[0] != '\0')
    {
        snprintf(g_wifi_config.weather_query, sizeof(g_wifi_config.weather_query), "%s", wx_query);
        g_wifi_config.weather_query_override_active = true;
        ESP_LOGI(APP_TAG, "config: loaded saved weather query override '%s'", g_wifi_config.weather_query);
    }
    else
    {
        ESP_LOGI(APP_TAG, "config: no saved weather query override, using default");
    }
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

    snprintf(g_wifi_config.wifi_ssid, sizeof(g_wifi_config.wifi_ssid), "%s", WIFI_SSID_LOCAL);
    snprintf(g_wifi_config.wifi_pass, sizeof(g_wifi_config.wifi_pass), "%s", WIFI_PASS_LOCAL);
    g_wifi_config.wifi_override_active = false;
    return ESP_OK;
}

const char *app_config_weather_api_key(void)
{
    return g_wifi_config.weather_api_key;
}

const char *app_config_weather_query(void)
{
    return g_wifi_config.weather_query;
}

bool app_config_weather_api_override_active(void)
{
    return g_wifi_config.weather_api_override_active;
}

bool app_config_weather_query_override_active(void)
{
    return g_wifi_config.weather_query_override_active;
}

esp_err_t app_config_set_weather_api_key(const char *api_key)
{
    if (api_key == NULL || api_key[0] == '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(api_key);
    if (len > APP_WEATHER_API_KEY_MAX_LEN)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(APP_CFG_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(nvs, APP_CFG_KEY_WX_API, api_key);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (err == ESP_OK)
    {
        snprintf(g_wifi_config.weather_api_key, sizeof(g_wifi_config.weather_api_key), "%s", api_key);
        g_wifi_config.weather_api_override_active = true;
    }

    return err;
}

esp_err_t app_config_set_weather_query(const char *query)
{
    if (query == NULL || query[0] == '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(query);
    if (len > APP_WEATHER_QUERY_MAX_LEN)
    {
        return ESP_ERR_INVALID_SIZE;
    }

    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(APP_CFG_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(nvs, APP_CFG_KEY_WX_QUERY, query);
    if (err == ESP_OK)
    {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (err == ESP_OK)
    {
        snprintf(g_wifi_config.weather_query, sizeof(g_wifi_config.weather_query), "%s", query);
        g_wifi_config.weather_query_override_active = true;
    }

    return err;
}

esp_err_t app_config_clear_weather_override(void)
{
    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(APP_CFG_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_err_t erase_api_err = nvs_erase_key(nvs, APP_CFG_KEY_WX_API);
    esp_err_t erase_query_err = nvs_erase_key(nvs, APP_CFG_KEY_WX_QUERY);
    if (erase_api_err != ESP_OK && erase_api_err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(nvs);
        return erase_api_err;
    }
    if (erase_query_err != ESP_OK && erase_query_err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(nvs);
        return erase_query_err;
    }

    err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err != ESP_OK)
    {
        return err;
    }

    snprintf(g_wifi_config.weather_api_key, sizeof(g_wifi_config.weather_api_key), "%s", WEATHER_API_KEY_LOCAL);
    snprintf(g_wifi_config.weather_query, sizeof(g_wifi_config.weather_query), "%s", WEATHER_QUERY_LOCAL);
    g_wifi_config.weather_api_override_active = false;
    g_wifi_config.weather_query_override_active = false;
    return ESP_OK;
}

static void app_console_print_help(void)
{
    ESP_LOGI(APP_TAG, "commands:");
    ESP_LOGI(APP_TAG, "  wifi show                  - show Wi-Fi config");
    ESP_LOGI(APP_TAG, "  wifi set <ssid> <pass>     - set Wi-Fi credentials");
    ESP_LOGI(APP_TAG, "  wifi clear                 - clear Wi-Fi override");
    ESP_LOGI(APP_TAG, "  api show                   - show API config");
    ESP_LOGI(APP_TAG, "  api set-key <key>          - set OpenWeather API key");
    ESP_LOGI(APP_TAG, "  api set-query <query>      - set location query");
    ESP_LOGI(APP_TAG, "  api clear                  - clear API overrides");
    ESP_LOGI(APP_TAG, "  continue                   - exit config, boot normally");
    ESP_LOGI(APP_TAG, "  wifi reboot / api reboot   - save and reboot");
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

        ESP_LOGI(APP_TAG, "saved: SSID='%s' pass=******** (%u chars)", ssid, (unsigned)strlen(pass));
        ESP_LOGI(APP_TAG, "type 'wifi reboot' to apply, or 'wifi show' to verify");
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

static void app_console_handle_api(const char *args)
{
    char subcmd[20] = {0};
    const char *cursor = args;
    if (!parse_next_token(&cursor, subcmd, sizeof(subcmd)))
    {
        app_console_print_help();
        return;
    }

    if (strcmp(subcmd, "show") == 0)
    {
        size_t key_len = strlen(app_config_weather_api_key());
        ESP_LOGI(APP_TAG, "api key source : %s",
                 app_config_weather_api_override_active() ? "NVS override" : "wifi_local.h defaults");
        ESP_LOGI(APP_TAG, "api key value  : %s",
                 key_len == 0 ? "<empty>" : app_config_weather_api_key());
        ESP_LOGI(APP_TAG, "api query src  : %s",
                 app_config_weather_query_override_active() ? "NVS override" : "wifi_local.h defaults");
        ESP_LOGI(APP_TAG, "api query      : %s", app_config_weather_query());
        return;
    }

    if (strcmp(subcmd, "set-key") == 0)
    {
        char key[APP_WEATHER_API_KEY_MAX_LEN + 1] = {0};
        if (!parse_next_token(&cursor, key, sizeof(key)) || *skip_ws(cursor) != '\0')
        {
            ESP_LOGW(APP_TAG, "usage: api set-key <openweather_api_key>");
            return;
        }
        esp_err_t err = app_config_set_weather_api_key(key);
        if (err != ESP_OK)
        {
            ESP_LOGE(APP_TAG, "config: save API key failed: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(APP_TAG, "saved: api key='%s'", key);
        ESP_LOGI(APP_TAG, "type 'api reboot' to apply, or 'api show' to verify");
        return;
    }

    if (strcmp(subcmd, "set-query") == 0)
    {
        char query[APP_WEATHER_QUERY_MAX_LEN + 1] = {0};
        if (!parse_next_token(&cursor, query, sizeof(query)) || *skip_ws(cursor) != '\0')
        {
            ESP_LOGW(APP_TAG, "usage: api set-query <query_string>");
            ESP_LOGW(APP_TAG, "example: api set-query \"zip=63301,US\"");
            return;
        }
        esp_err_t err = app_config_set_weather_query(query);
        if (err != ESP_OK)
        {
            ESP_LOGE(APP_TAG, "config: save API query failed: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(APP_TAG, "saved: api query='%s'", query);
        ESP_LOGI(APP_TAG, "type 'api reboot' to apply, or 'api show' to verify");
        return;
    }

    if (strcmp(subcmd, "clear") == 0)
    {
        esp_err_t err = app_config_clear_weather_override();
        if (err != ESP_OK)
        {
            ESP_LOGE(APP_TAG, "config: clear API override failed: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(APP_TAG, "config: API key/query overrides cleared (defaults restored)");
        ESP_LOGI(APP_TAG, "config: run 'api reboot' (or 'wifi reboot') to apply now");
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

// Returns: 0 = empty/continue, 1 = valid command (enter interactive), -1 = exit requested
static int app_console_process_line(char *line)
{
    trim_line(line);
    const char *cursor = skip_ws(line);
    if (cursor == NULL || *cursor == '\0')
    {
        return 0;
    }

    char command[16] = {0};
    if (!parse_next_token(&cursor, command, sizeof(command)))
    {
        return 0;
    }

    // Exit commands
    if (strcmp(command, "continue") == 0 || strcmp(command, "exit") == 0 || strcmp(command, "done") == 0)
    {
        return -1;
    }

    if (strcmp(command, "help") == 0 || strcmp(command, "?") == 0)
    {
        app_console_print_help();
        return 1;
    }

    if (strcmp(command, "wifi") == 0)
    {
        app_console_handle_wifi(cursor);
        return 1;
    }

    if (strcmp(command, "api") == 0)
    {
        app_console_handle_api(cursor);
        return 1;
    }

    ESP_LOGW(APP_TAG, "console: unknown command '%s' (type 'help' or 'continue' to exit)", command);
    return 1; // Stay in interactive mode on error too
}

void app_config_boot_console_window(uint32_t timeout_ms)
{
    if (timeout_ms == 0)
    {
        return;
    }

    usb_serial_jtag_driver_config_t usb_cfg = {};
    usb_cfg.tx_buffer_size = 512;
    usb_cfg.rx_buffer_size = 512;
    esp_err_t usb_err = usb_serial_jtag_driver_install(&usb_cfg);
    if (usb_err != ESP_OK && usb_err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(APP_TAG, "console: usb serial driver install failed: %s", esp_err_to_name(usb_err));
        return;
    }

    ESP_LOGI(APP_TAG, "console: %u s to enter config mode, or type 'continue' to skip", (unsigned)(timeout_ms / 1000U));

    const char *prompt = "> ";
    app_console_print_help();
    usb_serial_jtag_write_bytes((const uint8_t *)prompt, strlen(prompt), pdMS_TO_TICKS(100));

    uint32_t deadline_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS + timeout_ms;
    uint32_t last_countdown_s = (timeout_ms / 1000U) + 1;
    bool interactive_mode = false;
    char line[192] = {0};
    size_t line_len = 0;
    uint8_t buf[32] = {0};

    while (true)
    {
        uint32_t now_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Only timeout if not in interactive mode
        if (!interactive_mode && (int32_t)(now_ms - deadline_ms) >= 0)
        {
            break;
        }

        // Show countdown only if not in interactive mode and not typing
        if (!interactive_mode)
        {
            uint32_t remaining_s = (deadline_ms - now_ms) / 1000U;
            if (remaining_s != last_countdown_s && line_len == 0)
            {
                if (remaining_s == 10 || remaining_s == 5 || remaining_s <= 3)
                {
                    char countdown[32];
                    snprintf(countdown, sizeof(countdown), "\r[%u s] > ", (unsigned)remaining_s);
                    usb_serial_jtag_write_bytes((const uint8_t *)countdown, strlen(countdown), pdMS_TO_TICKS(100));
                }
                last_countdown_s = remaining_s;
            }
        }

        int read = usb_serial_jtag_read_bytes(buf, sizeof(buf), pdMS_TO_TICKS(100));
        if (read <= 0)
        {
            continue;
        }

        // Any input extends the deadline (before entering interactive mode)
        if (!interactive_mode)
        {
            deadline_ms = now_ms + 5000;
            last_countdown_s = 6;
        }

        for (int i = 0; i < read; ++i)
        {
            char c = (char)buf[i];

            // Treat CR or LF as command terminator (terminals vary)
            if (c == '\r' || c == '\n')
            {
                // Skip if line is empty (handles CR+LF sequences)
                if (line_len == 0)
                {
                    continue;
                }
                // Echo newline
                usb_serial_jtag_write_bytes((const uint8_t *)"\r\n", 2, pdMS_TO_TICKS(100));
                line[line_len] = '\0';

                int result = app_console_process_line(line);
                line_len = 0;
                line[0] = '\0';

                if (result == -1)
                {
                    // Exit requested
                    goto console_exit;
                }
                if (result == 1 && !interactive_mode)
                {
                    // First valid command - enter interactive mode
                    interactive_mode = true;
                    ESP_LOGI(APP_TAG, "console: interactive mode (type 'continue' to exit)");
                }

                // Print prompt for next command
                usb_serial_jtag_write_bytes((const uint8_t *)prompt, strlen(prompt), pdMS_TO_TICKS(100));
                continue;
            }

            // Handle backspace (ASCII 8 or DEL 127)
            if (c == 8 || c == 127)
            {
                if (line_len > 0)
                {
                    --line_len;
                    // Erase character on terminal: backspace, space, backspace
                    usb_serial_jtag_write_bytes((const uint8_t *)"\b \b", 3, pdMS_TO_TICKS(100));
                }
                continue;
            }

            if (line_len + 1 < sizeof(line))
            {
                line[line_len++] = c;
                // Echo the character
                usb_serial_jtag_write_bytes((const uint8_t *)&c, 1, pdMS_TO_TICKS(100));
            }
            else
            {
                line_len = 0;
                line[0] = '\0';
                ESP_LOGW(APP_TAG, "console: input line too long, dropped");
                usb_serial_jtag_write_bytes((const uint8_t *)"\r\n", 2, pdMS_TO_TICKS(100));
                usb_serial_jtag_write_bytes((const uint8_t *)prompt, strlen(prompt), pdMS_TO_TICKS(100));
            }
        }
    }

console_exit:
    usb_serial_jtag_write_bytes((const uint8_t *)"\r\n", 2, pdMS_TO_TICKS(100));
    ESP_LOGI(APP_TAG, "console: config window closed");
}
