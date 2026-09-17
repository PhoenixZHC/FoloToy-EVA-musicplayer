#pragma once

#include "bsp_button.h"
#include <stdbool.h>

void eva_player_start(bool storage_ready, bool battery_ready);
void eva_player_set_storage_ready(bool storage_ready);
void eva_player_handle_button(bsp_btn_t btn, bsp_btn_ev_t ev);
