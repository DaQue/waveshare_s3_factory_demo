#include "app_priv.h"

static bool is_https_url(const char *url)
{
    return (url != NULL) && (strncmp(url, "https://", 8) == 0);
}

static esp_http_client_handle_t http_client_create(const char *url)
{
    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = WEATHER_HTTP_TIMEOUT_MS;
    config.user_agent = "waveshare-s3-weather-test/1.0";
    config.keep_alive_enable = true;

    if (is_https_url(url))
    {
        config.tls_version = ESP_HTTP_CLIENT_TLS_VER_TLS_1_2;
        config.skip_cert_common_name_check = false;
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    return esp_http_client_init(&config);
}

static esp_err_t http_get_text_once(esp_http_client_handle_t client, const char *url, char *response_buf, size_t response_buf_size,
                                    int *status_code, int *bytes_read)
{
    if (client == NULL || url == NULL || response_buf_size == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    response_buf[0] = '\0';
    if (status_code)
    {
        *status_code = 0;
    }
    if (bytes_read)
    {
        *bytes_read = 0;
    }

    esp_err_t err = esp_http_client_set_url(client, url);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_timeout_ms(client, WEATHER_HTTP_TIMEOUT_MS);

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        return err;
    }

    (void)esp_http_client_fetch_headers(client);
    if (status_code)
    {
        *status_code = esp_http_client_get_status_code(client);
    }

    int total = 0;
    while (total < (int)response_buf_size - 1)
    {
        int n = esp_http_client_read(client, response_buf + total, (int)response_buf_size - 1 - total);
        if (n < 0)
        {
            err = ESP_FAIL;
            break;
        }
        if (n == 0)
        {
            break;
        }
        total += n;
    }
    response_buf[total] = '\0';

    if (bytes_read)
    {
        *bytes_read = total;
    }

    if (total >= (int)response_buf_size - 1)
    {
        err = ESP_ERR_NO_MEM;
    }

    esp_http_client_close(client);
    return err;
}

bool weather_fetch_once(void)
{
    const char *weather_query = app_config_weather_query();
    const char *weather_api_key = app_config_weather_api_key();
    if (weather_query == NULL || weather_query[0] == '\0' || weather_api_key == NULL || weather_api_key[0] == '\0')
    {
        app_set_status_fmt("https: missing weather query or API key");
        app_set_bottom_fmt("set API/query config");
        return false;
    }

    char weather_url[512] = {0};
    int weather_url_len = snprintf(weather_url, sizeof(weather_url),
                                   "https://api.openweathermap.org/data/2.5/weather?%s&units=imperial&appid=%s",
                                   weather_query, weather_api_key);
    if (weather_url_len <= 0 || weather_url_len >= (int)sizeof(weather_url))
    {
        app_set_status_fmt("https: url build failed");
        app_set_bottom_fmt("weather URL error");
        return false;
    }

    char forecast_url[512] = {0};
    int forecast_url_len = snprintf(forecast_url, sizeof(forecast_url),
                                    "https://api.openweathermap.org/data/2.5/forecast?%s&units=imperial&appid=%s",
                                    weather_query, weather_api_key);
    if (forecast_url_len <= 0 || forecast_url_len >= (int)sizeof(forecast_url))
    {
        app_set_status_fmt("https: forecast url build failed");
        app_set_bottom_fmt("forecast URL error");
        return false;
    }

    esp_http_client_handle_t client = http_client_create(weather_url);
    if (client == NULL)
    {
        app_set_status_fmt("https: client init failed");
        app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
        return false;
    }
    struct http_client_guard_t {
        esp_http_client_handle_t handle;
        ~http_client_guard_t()
        {
            if (handle != NULL)
            {
                esp_http_client_cleanup(handle);
            }
        }
    } client_guard = {client};

    app_set_status_fmt("https: GET weather (%s)", weather_query);
    app_set_bottom_fmt("fetching current conditions...");
    app_render_if_dirty();

    static char weather_response[WEATHER_HTTP_BUFFER_SIZE] = {0};
    int http_status = 0;
    int http_bytes = 0;
    esp_err_t err = http_get_text_once(client, weather_url, weather_response, sizeof(weather_response), &http_status, &http_bytes);

    if (err != ESP_OK)
    {
        app_set_status_fmt("https: transport error %s", esp_err_to_name(err));
        app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
        return false;
    }

    app_set_status_fmt("https: status %d bytes %d", http_status, http_bytes);

    if (http_status != 200)
    {
        snprintf(g_app.weather_text, sizeof(g_app.weather_text), "API returned status %d", http_status);
        app_mark_dirty(false, true, false, false);
        app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
        return false;
    }

    weather_payload_t wx = {};
    if (!parse_weather_json(weather_response, &wx))
    {
        app_set_status_fmt("json: parse failed");
        snprintf(g_app.weather_text, sizeof(g_app.weather_text), "weather JSON parse failed");
        app_mark_dirty(false, true, false, false);
        app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
        return false;
    }

    app_apply_weather(&wx);

    app_set_status_fmt("https: GET forecast");
    app_render_if_dirty();

    static char forecast_response[WEATHER_FORECAST_HTTP_BUFFER_SIZE] = {0};
    int fc_status = 0;
    int fc_bytes = 0;
    err = http_get_text_once(client, forecast_url, forecast_response, sizeof(forecast_response), &fc_status, &fc_bytes);
    if (err != ESP_OK)
    {
        app_set_status_fmt("https: forecast transport %s", esp_err_to_name(err));
        app_set_bottom_fmt("forecast retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
        return false;
    }

    if (fc_status != 200)
    {
        app_set_status_fmt("https: forecast status %d", fc_status);
        app_set_bottom_fmt("forecast retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
        return false;
    }

    memset(&g_forecast_cache, 0, sizeof(g_forecast_cache));
    if (!parse_forecast_json(forecast_response, &g_forecast_cache))
    {
        app_set_status_fmt("json: forecast parse failed");
        app_set_bottom_fmt("forecast retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
        return false;
    }

    app_apply_forecast_payload(&g_forecast_cache);
    app_set_status_fmt("sync: ok %s %s", wx.city, wx.country);
    app_set_bottom_fmt("next sync in %u min", (unsigned)(WEATHER_REFRESH_MS / 60000));
    return true;
}
