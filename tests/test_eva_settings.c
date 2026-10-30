#include "eva_settings.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void)
{
    eva_settings_t value;
    eva_settings_defaults(&value);
    assert(value.mode==EVA_SOURCE_LOCAL && value.brightness==100 && value.screen_off_seconds==0);
    assert(eva_settings_valid(&value));
    assert(eva_settings_adjust(&value,EVA_SETTING_MODE,1) && value.mode==EVA_SOURCE_RADIO);
    assert(eva_settings_adjust(&value,EVA_SETTING_MODE,-1) && value.mode==EVA_SOURCE_LOCAL);
    assert(!eva_settings_adjust(&value,EVA_SETTING_BRIGHTNESS,1));
    for(int i=0;i<20;i++)eva_settings_adjust(&value,EVA_SETTING_BRIGHTNESS,-1);
    assert(value.brightness==10);
    assert(!eva_settings_adjust(&value,EVA_SETTING_BRIGHTNESS,-1));
    assert(eva_settings_adjust(&value,EVA_SETTING_BRIGHTNESS,1) && value.brightness==20);
    assert(eva_settings_adjust(&value,EVA_SETTING_SCREEN_OFF,1) && value.screen_off_seconds==15);
    assert(!eva_settings_screen_due(&value,1000,15999));
    assert(eva_settings_screen_due(&value,1000,16000));
    assert(eva_settings_screen_due(&value,UINT32_MAX-1000,14000));
    assert(eva_settings_adjust(&value,EVA_SETTING_SCREEN_OFF,-1) && value.screen_off_seconds==0);
    assert(!eva_settings_screen_due(&value,0,UINT32_MAX));
    assert(eva_settings_adjust(&value,EVA_SETTING_SCREEN_OFF,-1) && value.screen_off_seconds==300);
    assert(eva_settings_adjust(&value,EVA_SETTING_SCREEN_OFF,1) && value.screen_off_seconds==0);
    assert(!eva_settings_adjust(&value,EVA_SETTING_NETWORK,1));
    assert(!eva_settings_adjust(&value,EVA_SETTING_HELP,1));
    assert(!eva_settings_adjust(&value,EVA_SETTING_BACK,1));
    value.mode=3; assert(!eva_settings_valid(&value));
    eva_settings_defaults(&value); value.brightness=0; assert(!eva_settings_valid(&value));
    value.brightness=11; assert(!eva_settings_valid(&value));
    value.brightness=100; value.screen_off_seconds=31; assert(!eva_settings_valid(&value));
    assert(!eva_settings_valid(NULL));
    puts("settings model tests passed");
    return 0;
}
