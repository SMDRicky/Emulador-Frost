// Copyright (c) RomuloSM
// Email: romulodevel@gmail.com
// Facebook: https://www.facebook.com/RomuloDevel
// rAthena: https://rathena.org/board/profile/6097-sbk_/

#ifndef _CHAR_RESTOCK_HPP_
#define _CHAR_RESTOCK_HPP_

#include "../common/cbasetypes.hpp"

bool char_restock_load(uint32 char_id, struct item* item);
bool char_restock_save(uint32 char_id, struct mmo_charstatus* status);

#endif /* _CHAR_RESTOCK_HPP_ */
