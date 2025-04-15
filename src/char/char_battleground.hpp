// Copyright (c) RomuloSM
// Email: romulodevel@gmail.com
// Facebook: https://www.facebook.com/RomuloDevel
// rAthena: https://rathena.org/board/profile/6097-sbk_/

#ifndef _BATTLEGROUND_CHAR_HPP_
#define _BATTLEGROUND_CHAR_HPP_

#include "../common/cbasetypes.hpp"

bool bg_ranking_load(uint32 char_id, struct s_battleground_ranking *bdata);
bool bg_ranking_save(uint32 char_id, struct s_battleground_ranking *p, struct s_battleground_ranking *cp);

#endif /* _BATTLEGROUND_CHAR_HPP_ */
