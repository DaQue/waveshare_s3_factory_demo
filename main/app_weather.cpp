#include "app_priv.h"

const char *weekday_name(int wday)
{
    if (wday < 0 || wday > 6)
    {
        return "?";
    }
    return WEEKDAY_SHORT[wday];
}

void format_hour_label(int hour24, char *out, size_t out_size)
{
    int hour12 = hour24 % 12;
    if (hour12 == 0)
    {
        hour12 = 12;
    }
    const char *ampm = (hour24 >= 12) ? "PM" : "AM";
    snprintf(out, out_size, "%d%s", hour12, ampm);
}

static bool owm_icon_is_night(const char *icon_code)
{
    return (icon_code != NULL && strlen(icon_code) >= 3 && icon_code[2] == 'n');
}

static const char *forecast_condition_short(int weather_id)
{
    if (weather_id >= 200 && weather_id < 300)
    {
        return "Storm";
    }
    if (weather_id >= 300 && weather_id < 400)
    {
        return "Drizzle";
    }
    if (weather_id >= 500 && weather_id < 600)
    {
        if (weather_id == 511)
        {
            return "Sleet";
        }
        return "Rain";
    }
    if (weather_id >= 600 && weather_id < 700)
    {
        return "Snow";
    }
    if (weather_id >= 700 && weather_id < 800)
    {
        if (weather_id == 741)
        {
            return "Fog";
        }
        return "Mist";
    }
    if (weather_id == 800)
    {
        return "Clear";
    }
    if (weather_id == 801)
    {
        return "Partly Cloudy";
    }
    if (weather_id == 802)
    {
        return "Cloudy";
    }
    if (weather_id >= 803 && weather_id <= 804)
    {
        return "Overcast";
    }

    return "Cloudy";
}

drawing_weather_icon_t map_owm_condition_to_icon(int weather_id, const char *icon_code)
{
    bool is_night = owm_icon_is_night(icon_code);

    if (weather_id >= 200 && weather_id < 300)
    {
        return DRAWING_WEATHER_ICON_THUNDERSTORM;
    }
    if (weather_id >= 300 && weather_id < 400)
    {
        return DRAWING_WEATHER_ICON_SHOWER_RAIN;
    }
    if (weather_id >= 500 && weather_id < 600)
    {
        if (weather_id == 511)
        {
            return DRAWING_WEATHER_ICON_SLEET;
        }
        if (weather_id >= 520)
        {
            return DRAWING_WEATHER_ICON_SHOWER_RAIN;
        }
        return DRAWING_WEATHER_ICON_RAIN;
    }
    if (weather_id >= 600 && weather_id < 700)
    {
        return DRAWING_WEATHER_ICON_SNOW;
    }
    if (weather_id >= 700 && weather_id < 800)
    {
        // Atmospheric conditions (mist/haze/smoke/dust, etc.) are often rendered
        // as abstract line art. Use clouds for clearer at-a-glance UX.
        return DRAWING_WEATHER_ICON_CLOUDS;
    }
    if (weather_id == 800)
    {
        return is_night ? DRAWING_WEATHER_ICON_CLEAR_NIGHT : DRAWING_WEATHER_ICON_CLEAR_DAY;
    }
    if (weather_id == 801)
    {
        return is_night ? DRAWING_WEATHER_ICON_FEW_CLOUDS_NIGHT : DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
    }
    if (weather_id == 802)
    {
        return DRAWING_WEATHER_ICON_CLOUDS;
    }
    if (weather_id >= 803 && weather_id <= 804)
    {
        return DRAWING_WEATHER_ICON_OVERCAST;
    }

    return is_night ? DRAWING_WEATHER_ICON_FEW_CLOUDS_NIGHT : DRAWING_WEATHER_ICON_CLOUDS;
}

static bool json_read_i64(const cJSON *item, int64_t *out)
{
    if (out == NULL || item == NULL || !cJSON_IsNumber(item))
    {
        return false;
    }
    *out = (int64_t)item->valuedouble;
    return true;
}

static bool json_read_float(const cJSON *item, float *out)
{
    if (out == NULL || item == NULL || !cJSON_IsNumber(item))
    {
        return false;
    }
    *out = (float)item->valuedouble;
    return true;
}

static int json_read_int(const cJSON *item, int fallback)
{
    if (item == NULL || !cJSON_IsNumber(item))
    {
        return fallback;
    }
    return item->valueint;
}

static const char *json_read_string(const cJSON *item, const char *fallback)
{
    if (item == NULL || !cJSON_IsString(item) || item->valuestring == NULL)
    {
        return fallback;
    }
    return item->valuestring;
}

bool parse_weather_json(const char *json_text, weather_payload_t *out)
{
    if (out == NULL)
    {
        return false;
    }

    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL)
    {
        return false;
    }

    cJSON *main_obj = cJSON_GetObjectItemCaseSensitive(root, "main");
    cJSON *weather_arr = cJSON_GetObjectItemCaseSensitive(root, "weather");
    cJSON *wind_obj = cJSON_GetObjectItemCaseSensitive(root, "wind");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *sys_obj = cJSON_GetObjectItemCaseSensitive(root, "sys");

    cJSON *temp = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "temp") : NULL;
    cJSON *feels = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "feels_like") : NULL;
    cJSON *humidity = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "humidity") : NULL;
    cJSON *pressure = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "pressure") : NULL;

    cJSON *wind_speed = (wind_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(wind_obj, "speed") : NULL;

    cJSON *country = (sys_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(sys_obj, "country") : NULL;
    cJSON *weather0 = (weather_arr != NULL && cJSON_IsArray(weather_arr)) ? cJSON_GetArrayItem(weather_arr, 0) : NULL;
    cJSON *desc = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "description") : NULL;
    cJSON *weather_id = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "id") : NULL;
    cJSON *icon = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "icon") : NULL;

    float temp_f = 0.0f;
    if (!json_read_float(temp, &temp_f))
    {
        cJSON_Delete(root);
        return false;
    }

    float feels_f = temp_f;
    (void)json_read_float(feels, &feels_f);

    float wind_mph = 0.0f;
    (void)json_read_float(wind_speed, &wind_mph);

    memset(out, 0, sizeof(*out));
    out->temp_f = temp_f;
    out->feels_f = feels_f;
    out->wind_mph = wind_mph;
    out->humidity = json_read_int(humidity, -1);
    out->pressure_hpa = json_read_int(pressure, -1);
    int weather_id_value = json_read_int(weather_id, 0);
    const char *icon_code = json_read_string(icon, NULL);
    out->icon = map_owm_condition_to_icon(weather_id_value, icon_code);

    snprintf(out->city, sizeof(out->city), "%s", json_read_string(name, "?"));
    snprintf(out->country, sizeof(out->country), "%s", json_read_string(country, ""));
    snprintf(out->condition, sizeof(out->condition), "%s", json_read_string(desc, "(unknown)"));

    ESP_LOGI(APP_TAG, "weather: id=%d icon=%s desc=%s mapped=%d",
             weather_id_value,
             (icon_code != NULL) ? icon_code : "?",
             out->condition,
             (int)out->icon);

    cJSON_Delete(root);
    return true;
}

static void forecast_payload_set_defaults(forecast_payload_t *out)
{
    static const char *default_titles[APP_FORECAST_ROWS] = {
        "Tue", "Wed", "Thu", "Fri"};
    out->row_count = APP_FORECAST_ROWS;
    for (int i = 0; i < APP_FORECAST_ROWS; ++i)
    {
        snprintf(out->rows[i].title, sizeof(out->rows[i].title), "%s", default_titles[i]);
        snprintf(out->rows[i].detail, sizeof(out->rows[i].detail), "Low --° Wind --");
        snprintf(out->rows[i].temp_text, sizeof(out->rows[i].temp_text), "--°");
        out->rows[i].temp_f = 0;
        out->rows[i].feels_f = 0;
        out->rows[i].wind_mph = 0;
        out->rows[i].icon = DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
        out->days[i].count = 0;
    }
    snprintf(out->preview_text, sizeof(out->preview_text), "Tue --°   Wed --°   Thu --°");
}

bool parse_forecast_json(const char *json_text, forecast_payload_t *out)
{
    if (out == NULL)
    {
        return false;
    }

    forecast_payload_set_defaults(out);

    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL)
    {
        return false;
    }

    cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "list");
    if (!cJSON_IsArray(list))
    {
        cJSON_Delete(root);
        return false;
    }

    cJSON *city = cJSON_GetObjectItemCaseSensitive(root, "city");
    cJSON *timezone = (city != NULL) ? cJSON_GetObjectItemCaseSensitive(city, "timezone") : NULL;
    int tz_offset = json_read_int(timezone, 0);

    typedef struct {
        int year;
        int yday;
        int wday;
        float high_f;
        float low_f;
        float wind_peak_mph;
        drawing_weather_icon_t icon;
        char condition_short[20];
        int icon_score;
        forecast_hourly_payload_t hourly[APP_FORECAST_HOURLY_MAX];
        bool set;
        bool icon_set;
        uint8_t hourly_count;
    } day_summary_t;

    static day_summary_t days[APP_FORECAST_MAX_DAYS];
    memset(days, 0, sizeof(days));
    int day_count = 0;
    int first_entry_hour = -1;

    int list_count = cJSON_GetArraySize(list);
    for (int i = 0; i < list_count; ++i)
    {
        cJSON *entry = cJSON_GetArrayItem(list, i);
        cJSON *dt = (entry != NULL) ? cJSON_GetObjectItemCaseSensitive(entry, "dt") : NULL;
        cJSON *main_obj = (entry != NULL) ? cJSON_GetObjectItemCaseSensitive(entry, "main") : NULL;
        if (!cJSON_IsNumber(dt) || !cJSON_IsObject(main_obj))
        {
            continue;
        }

        cJSON *temp = cJSON_GetObjectItemCaseSensitive(main_obj, "temp");
        cJSON *wind_obj = cJSON_GetObjectItemCaseSensitive(entry, "wind");
        cJSON *wind_speed = cJSON_IsObject(wind_obj) ? cJSON_GetObjectItemCaseSensitive(wind_obj, "speed") : NULL;
        float temp_f = 0.0f;
        if (!json_read_float(temp, &temp_f))
        {
            continue;
        }

        int64_t dt_value = 0;
        if (!json_read_i64(dt, &dt_value))
        {
            continue;
        }

        float wind_speed_f = 0.0f;
        bool has_wind_speed = json_read_float(wind_speed, &wind_speed_f);

        time_t local_epoch = (time_t)(dt_value + (int64_t)tz_offset);
        struct tm tm_local = {};
        gmtime_r(&local_epoch, &tm_local);
        if (first_entry_hour < 0)
        {
            first_entry_hour = tm_local.tm_hour;
        }

        int idx = -1;
        for (int d = 0; d < day_count; ++d)
        {
            if (days[d].set && days[d].year == tm_local.tm_year && days[d].yday == tm_local.tm_yday)
            {
                idx = d;
                break;
            }
        }
        if (idx < 0)
        {
            if (day_count >= APP_FORECAST_MAX_DAYS)
            {
                continue;
            }
            idx = day_count++;
            days[idx].set = true;
            days[idx].year = tm_local.tm_year;
            days[idx].yday = tm_local.tm_yday;
            days[idx].wday = tm_local.tm_wday;
            days[idx].high_f = temp_f;
            days[idx].low_f = temp_f;
            days[idx].wind_peak_mph = has_wind_speed ? wind_speed_f : 0.0f;
            days[idx].icon = DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
            snprintf(days[idx].condition_short, sizeof(days[idx].condition_short), "Cloudy");
            days[idx].icon_set = false;
            days[idx].icon_score = -1;
            days[idx].hourly_count = 0;
        }
        if (temp_f > days[idx].high_f)
        {
            days[idx].high_f = temp_f;
        }
        if (temp_f < days[idx].low_f)
        {
            days[idx].low_f = temp_f;
        }
        if (has_wind_speed && wind_speed_f > days[idx].wind_peak_mph)
        {
            days[idx].wind_peak_mph = wind_speed_f;
        }

        cJSON *weather_arr = cJSON_GetObjectItemCaseSensitive(entry, "weather");
        cJSON *weather0 = (weather_arr != NULL && cJSON_IsArray(weather_arr)) ? cJSON_GetArrayItem(weather_arr, 0) : NULL;
        cJSON *weather_id = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "id") : NULL;
        cJSON *weather_icon = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "icon") : NULL;
        int weather_id_value = json_read_int(weather_id, 0);
        const char *weather_icon_value = json_read_string(weather_icon, NULL);
        drawing_weather_icon_t mapped_icon = map_owm_condition_to_icon(
            weather_id_value,
            weather_icon_value);

        int icon_score = 0;
        if (tm_local.tm_hour == 12)
        {
            icon_score = 3;
        }
        else if (tm_local.tm_hour == 9 || tm_local.tm_hour == 15)
        {
            icon_score = 2;
        }
        else
        {
            icon_score = 1;
        }
        if (!days[idx].icon_set || icon_score > days[idx].icon_score)
        {
            days[idx].icon = mapped_icon;
            snprintf(days[idx].condition_short, sizeof(days[idx].condition_short), "%s",
                     forecast_condition_short(weather_id_value));
            days[idx].icon_set = true;
            days[idx].icon_score = icon_score;
        }

        if (days[idx].hourly_count < APP_FORECAST_HOURLY_MAX)
        {
            forecast_hourly_payload_t *slot = &days[idx].hourly[days[idx].hourly_count];
            int temp_i = (int)lroundf(temp_f);
            cJSON *feels_like = cJSON_GetObjectItemCaseSensitive(main_obj, "feels_like");
            float feels_like_f = temp_f;
            (void)json_read_float(feels_like, &feels_like_f);
            int feels_i = (int)lroundf(feels_like_f);
            int wind_i = has_wind_speed ? (int)lroundf(wind_speed_f) : 0;

            slot->temp_f = temp_i;
            slot->feels_f = feels_i;
            slot->wind_mph = wind_i;
            slot->icon = mapped_icon;
            format_hour_label(tm_local.tm_hour, slot->time_text, sizeof(slot->time_text));
            snprintf(slot->detail, sizeof(slot->detail), "Feels %d° Wind %d", feels_i, wind_i);
            snprintf(slot->temp_text, sizeof(slot->temp_text), "%d°", temp_i);
            days[idx].hourly_count++;
        }
    }

    int start_day = 0;
    if (day_count > 1 && first_entry_hour > 0)
    {
        // OWM 5-day forecast starts from the next 3h slot; if it is not midnight,
        // the first grouped day is a partial "today" bucket. Skip it for day-ahead UI.
        start_day = 1;
    }

    int available_days = day_count - start_day;
    if (available_days < 0)
    {
        available_days = 0;
    }

    int row_count = (available_days < APP_FORECAST_ROWS) ? available_days : APP_FORECAST_ROWS;
    out->row_count = (uint8_t)row_count;
    for (int i = 0; i < row_count; ++i)
    {
        const day_summary_t *day = &days[start_day + i];
        forecast_row_payload_t *row = &out->rows[i];
        int high_i = (int)lroundf(day->high_f);
        int low_i = (int)lroundf(day->low_f);
        int wind_i = (int)lroundf(day->wind_peak_mph);

        row->temp_f = high_i;
        row->feels_f = low_i;
        row->wind_mph = wind_i;
        row->icon = day->icon_set ? day->icon : DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;

        snprintf(row->title, sizeof(row->title), "%s", weekday_name(day->wday));
        snprintf(row->detail, sizeof(row->detail), "%s Low %d° Wind %d",
                 day->condition_short, low_i, wind_i);
        snprintf(row->temp_text, sizeof(row->temp_text), "%d°", high_i);

        out->days[i].count = day->hourly_count;
        for (int h = 0; h < day->hourly_count && h < APP_FORECAST_HOURLY_MAX; ++h)
        {
            out->days[i].entries[h] = day->hourly[h];
        }
    }

    int preview_count = (available_days < APP_PREVIEW_DAYS) ? available_days : APP_PREVIEW_DAYS;
    if (preview_count > 0)
    {
        out->preview_text[0] = '\0';
        for (int i = 0; i < preview_count; ++i)
        {
            char day_chunk[32] = {0};
            const day_summary_t *day = &days[start_day + i];
            int high_i = (int)lroundf(day->high_f);
            snprintf(day_chunk, sizeof(day_chunk), "%s %d°", weekday_name(day->wday), high_i);

            if (i > 0)
            {
                strncat(out->preview_text, "   ", sizeof(out->preview_text) - strlen(out->preview_text) - 1);
            }
            strncat(out->preview_text, day_chunk, sizeof(out->preview_text) - strlen(out->preview_text) - 1);
        }
    }

    cJSON_Delete(root);
    return (row_count > 0);
}
