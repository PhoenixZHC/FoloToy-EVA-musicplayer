#include "eva_settings.h"
#include <stddef.h>
static const uint16_t s_timeouts[] = {0, 15, 30, 60, 120, 300};
void eva_settings_defaults(eva_settings_t *settings)
{
    *settings = (eva_settings_t){.mode=EVA_SOURCE_LOCAL, .brightness=100, .screen_off_seconds=0,
                               .screensaver_enabled=1, .screensaver_brightness=20};
}
bool eva_settings_valid(const eva_settings_t *settings)
{
    if (!settings || settings->mode > EVA_SOURCE_RADIO || settings->brightness < 10 ||
        settings->brightness > 100 || settings->brightness % 10 ||
        settings->screensaver_enabled > 1 || settings->screensaver_brightness < 10 ||
        settings->screensaver_brightness > 100 || settings->screensaver_brightness % 10) return false;
    for (size_t i=0; i<sizeof(s_timeouts)/sizeof(s_timeouts[0]); i++)
        if (settings->screen_off_seconds == s_timeouts[i]) return true;
    return false;
}
bool eva_settings_adjust(eva_settings_t *settings, eva_setting_item_t item, int direction)
{
    if (!eva_settings_valid(settings) || !direction) return false;
    if (item == EVA_SETTING_MODE) { settings->mode ^= 1U; return true; }
    if (item == EVA_SETTING_SCREENSAVER) { settings->screensaver_enabled ^= 1U; return true; }
    if (item == EVA_SETTING_BRIGHTNESS || item == EVA_SETTING_SCREENSAVER_BRIGHTNESS) {
        uint8_t *brightness = item == EVA_SETTING_BRIGHTNESS ?
            &settings->brightness : &settings->screensaver_brightness;
        int next = *brightness + (direction > 0 ? 10 : -10);
        if (next < 10 || next > 100) return false;
        *brightness = (uint8_t)next;
        return true;
    }
    if (item == EVA_SETTING_SCREEN_OFF) {
        size_t count=sizeof(s_timeouts)/sizeof(s_timeouts[0]);
        for (size_t i=0; i<count; i++) {
            if (settings->screen_off_seconds != s_timeouts[i]) continue;
            settings->screen_off_seconds=s_timeouts[(i+count+(direction>0?1:-1))%count];
            return true;
        }
    }
    return false;
}
bool eva_settings_screen_due(const eva_settings_t *settings, uint32_t last_activity, uint32_t now)
{
    return settings && settings->screen_off_seconds &&
        (uint32_t)(now-last_activity) >= (uint32_t)settings->screen_off_seconds*1000U;
}
