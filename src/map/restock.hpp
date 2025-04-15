// Copyright (c) RomuloSM
// Email: romulodevel@gmail.com
// Facebook: https://www.facebook.com/RomuloDevel
// rAthena: https://rathena.org/board/profile/6097-sbk_/

#ifndef RESTOCK_HPP
#define RESTOCK_HPP

#include "../common/cbasetypes.hpp"
#include "../common/mmo.hpp"

bool restock_check_vip(map_session_data* sd);
int restock_get_slot(map_session_data* sd, t_itemid nameid, int max);
int restock_get_item(map_session_data* sd, t_itemid nameid, int stuffType, bool costume, bool packguild);
int restock_add_item(map_session_data* sd, t_itemid nameid, int amount);
int restock_del_item(map_session_data* sd, t_itemid nameid);
int restock_clear(map_session_data* sd);
bool restock_check_zone(int16 m);

#endif /* RESTOCK_HPP */
