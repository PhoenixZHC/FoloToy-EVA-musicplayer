foreach(local_asset IN ITEMS eva_text_assets.c eva_text_assets.h eva_text_buttons.c eva_text_buttons.h eva_font_matisse_14.c eva_font_matisse_20.c eva_font_radio_16.c eva_font_radio_20.c web_ui.h)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/${local_asset}")
        message(FATAL_ERROR "Missing locally generated ${local_asset}; run tools/prepare_local_font.ps1 with your own font (see docs/ASSET_PREPARATION.md)")
    endif()
endforeach()

idf_component_register(
    SRCS "main.c"
         "eva_player.c"
         "eva_player_frame.c"
         "eva_player_model.c"
         "eva_settings.c"
         "eva_settings_store.c"
         "eva_radio_catalog.c"
         "eva_radio_model.c"
         "eva_radio_view.c"
         "eva_font_radio_16.c"
         "eva_font_radio_20.c"
         "eva_radio_stream.c"
         "eva_radio_config.c"
         "eva_radio_player.c"
         "eva_radio_setup.c"
         "eva_adpcm.c"
         "fam1_format.c"
         "audio_catalog.c"
         "eva_music_store.c"
         "eva_wifi.c"
         "eva_title.c"
         "eva_clock.c"
         "eva_track_layout.c"
         "eva_logo_assets.c"
         "eva_text_assets.c"
         "eva_text_buttons.c"
         "eva_font_matisse_14.c"
         "eva_font_matisse_20.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "demo_display.c"
         "demo_button.c"
         "demo_audio.c"
         "demo_battery.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer lwip esp_app_format
    REQUIRES bsp esp_wifi esp_http_server esp_http_client esp_netif nvs_flash fatfs wear_levelling json esp_audio_codec mbedtls
    EMBED_FILES "../assets/audio/startup.adpcm"
    EMBED_TXTFILES "eva_radio_setup.html"
)
