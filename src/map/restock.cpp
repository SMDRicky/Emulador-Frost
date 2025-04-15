// Copyright (c) RomuloSM
// Email: romulodevel@gmail.com
// Facebook: https://www.facebook.com/RomuloDevel
// rAthena: https://rathena.org/board/profile/6097-sbk_/

#include "restock.hpp"

#include "../common/cbasetypes.hpp"
#include "../common/core.hpp"
#include "../common/mmo.hpp"
#include "../common/nullpo.hpp"
#include "../common/utilities.hpp"
#include "../common/utils.hpp"

#include "battle.hpp"
#include "map.hpp"
#include "pc.hpp"
#include "storage.hpp"

#include <stdlib.h>

bool restock_check_vip(map_session_data* sd)
{
	if (battle_config.restock_vip_type == 1 && pc_get_group_id(sd) >= battle_config.restock_group_level)
		return true;
	else if (battle_config.restock_vip_type == 2 && pc_get_group_level(sd) >= battle_config.restock_group_level)
		return true;
#ifdef VIP_ENABLE
	else if( pc_isvip(sd) )
		return true;
#endif
	else
		return false;
}

int restock_get_slot(map_session_data *sd, t_itemid nameid, int max)
{
	int i = 0;

	nullpo_retr(0,sd);

	ARR_FIND(0, max, i, sd->status.restock[i].nameid == nameid);
	if( i >= max )
		return max;

	return i;
}

int restock_get_item(map_session_data *sd, t_itemid nameid, int stuffType, bool costume, bool packguild)
{
	int i, tmp_amount, amount = 0, max, max_storage;

	nullpo_retr(0,sd);

	if( nameid < 0 )
		return 0;

	if( sd->state.storage_flag )
		return 0;

	max = restock_check_vip(sd) ? battle_config.restock_vip_max_items : battle_config.restock_regular_max_items;
	if( (i = restock_get_slot(sd,nameid,max)) >= max )
		return 0;

	amount = tmp_amount = sd->status.restock[i].amount;
	max_storage = ARRAYLENGTH(sd->storage.u.items_storage);
	for( i=0; i < max_storage && tmp_amount > 0; i++ ) {
		int get_item = 0;
		if( tmp_amount <= 0 )
			break;
		if( sd->storage.u.items_storage[i].nameid != nameid )
			continue;
#ifdef STUFF_ENABLE
		if( itemdb_stuffType(&sd->storage.u.items_storage[i]) != stuffType )
			continue;
		if( packguild && !itemdb_ispackguild(&sd->storage.u.items_storage[i]) )
			continue;
#endif
#ifdef COSTUME_ENABLE
		if( costume && !itemdb_iscostume(&sd->storage.u.items_storage[i]) )
			continue;
#endif

		if( sd->storage.u.items_storage[i].amount < tmp_amount )
			get_item = sd->storage.u.items_storage[i].amount;
		else
			get_item = tmp_amount;

		tmp_amount -= get_item;
		if( get_item )
			storage_storageget(sd, &sd->storage, i, get_item);
	}

	return tmp_amount > 0 ? (amount - tmp_amount) : amount;
}

int restock_add_item(map_session_data *sd, t_itemid nameid, int amount)
{
	int i, max;

	nullpo_retr(-1,sd);

	max = restock_check_vip(sd) ? battle_config.restock_vip_max_items : battle_config.restock_regular_max_items;
	i = restock_get_slot(sd,nameid,max);
	if( i >= max )
		i = restock_get_slot(sd,0,max);

	// Check New Slot
	if( i < MAX_RESTOCK ) {
		sd->status.restock[i].nameid = nameid;
		sd->status.restock[i].amount = amount;
		return 1;
	}
	return 0;
}

int restock_del_item(map_session_data *sd, t_itemid nameid)
{
	int i, max;

	nullpo_retr(-1, sd);

	max = restock_check_vip(sd) ? battle_config.restock_vip_max_items : battle_config.restock_regular_max_items;
	i = restock_get_slot(sd,nameid,max);

	if( i < max ) {
		sd->status.restock[i].nameid = 0;
		sd->status.restock[i].amount = 0;
		return 1;
	}
	return 0;
}

int restock_clear(map_session_data* sd)
{
	int i, c = 0, max;

	nullpo_retr(-1, sd);

	max = restock_check_vip(sd) ? battle_config.restock_vip_max_items : battle_config.restock_regular_max_items;

	for( i = 0; i < MAX_RESTOCK; i++ ) {
		std::shared_ptr<item_data> i_data = nullptr;
		if( sd->status.restock[i].nameid == 0 )
			continue;
		else if( sd->status.restock[i].amount == 0 || (i_data = item_db.find(sd->status.restock[i].nameid)) == nullptr ) {
			sd->status.restock[i].nameid = 0;
			sd->status.restock[i].amount = 0;
		}
		else c++;
	}
	return c;
}

bool restock_check_zone(int16 m)
{
	struct map_data* mapdata = map_getmapdata(m);

	if (mapdata == NULL)
		return false;
	if (map_getmapflag(m, MF_RESTOCK_ON))
		return true;
	if (map_getmapflag(m, MF_RESTOCK_OFF))
		return false;
	if (!map_getmapflag(m, MF_PVP) && !map_getmapflag(m, MF_GVG) && !map_getmapflag(m, MF_BATTLEGROUND) && !map_getmapflag(m, MF_GVG_CASTLE))
		return true;
	if ((battle_config.restock_maps & 0x1) && map_getmapflag(m, MF_PVP))
		return true;
	if ((battle_config.restock_maps & 0x10) && map_getmapflag(m, MF_GVG))
		return true;
	if ((battle_config.restock_maps & 0x100) && map_getmapflag(m, MF_BATTLEGROUND))
		return true;
	if ((battle_config.restock_maps & 0x200) && map_getmapflag(m, MF_GVG_CASTLE) && !agit_flag && !agit2_flag && !agit3_flag)
		return true;
	if ((battle_config.restock_maps & 0x400) && map_getmapflag(m, MF_GVG_CASTLE) && (agit_flag && agit2_flag && agit3_flag))
		return true;

	return false;
}
