// Copyright (c) RomuloSM
// Email: romulodevel@gmail.com
// Facebook: https://www.facebook.com/RomuloDevel
// rAthena: https://rathena.org/board/profile/6097-sbk_/

#ifndef _WIN32
#include <unistd.h>
#else
#include <common/winapi.hpp>
#endif
#include <common/cbasetypes.hpp>
#include <common/sql.hpp>
#include <common/strlib.hpp>
#include <common/showmsg.hpp>
#include <common/socket.hpp>
#include <common/database.hpp>
#include <common/mmo.hpp>

#include "char_battleground.hpp"
#include "inter.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// Battleground Expansion
bool bg_ranking_load(uint32 char_id, struct s_battleground_ranking *bdata)
{
	char* data;

	if( SQL_ERROR == Sql_Query(sql_handle, "SELECT "
		"`wins`, `loss`, `tied`, `score_wins`, `score_loss`, `kills`, `deaths`, `damage_given`, `damage_taken`, "
		"`runestone_kills`, `runestone_damage`, `runestone_repair`, `emperium_kills`, `emperium_damage`, "
		"`barrier_kills`, `barrier_damage`, `barrier_repair`, `objective_kills`, `objective_damage`, "
		"`cristal_kills`, `cristal_damage`, `flag_kills`, `flag_damage`, `flag_capture`, `flag_recapture`, "
		"`guard_kills`, `guard_deaths`, `guard_damage_given`, `guard_damage_taken`, `skill_support_success`, "
		"`skill_support_fail`, `skill_success`, `skill_fail`, `skill_damage_given`, `skill_damage_taken`, "
		"`heal_hp`, `heal_sp`, `item_heal_hp`, `item_heal_sp`, `item_heal`, `oridecon`, `elunium`, `steel`, "
		"`emveretarcon`, `wooden_block`, `stones`, `yellow_gemstones`, `red_gemstones`, `blue_gemstones`, "
		"`ammos`, `poison_bottle`, `fire_bottle`, `acid_bottle`"
		" FROM `bg_ranking` WHERE `char_id`='%d' AND `bg_id`='0'", char_id) )
	{
		Sql_ShowDebug(sql_handle);
		return false;
	}

	if( SQL_SUCCESS != Sql_NextRow(sql_handle) )
	{
		Sql_FreeResult(sql_handle);
		return false;
	}

	Sql_GetData(sql_handle,  0, &data, NULL); bdata->battle.wins = atoi(data);
	Sql_GetData(sql_handle,  1, &data, NULL); bdata->battle.loss = atoi(data);
	Sql_GetData(sql_handle,  2, &data, NULL); bdata->battle.tied = atoi(data);
	Sql_GetData(sql_handle,  3, &data, NULL); bdata->battle.score_wins = atoi(data);
	Sql_GetData(sql_handle,  4, &data, NULL); bdata->battle.score_loss = atoi(data);
	Sql_GetData(sql_handle,  5, &data, NULL); bdata->player.kills = atoi(data);
	Sql_GetData(sql_handle,  6, &data, NULL); bdata->player.deaths = atoi(data);
	Sql_GetData(sql_handle,  7, &data, NULL); bdata->player.damage_given = atoi(data);
	Sql_GetData(sql_handle,  8, &data, NULL); bdata->player.damage_taken = atoi(data);
	Sql_GetData(sql_handle,  9, &data, NULL); bdata->runestone.kills = atoi(data);
	Sql_GetData(sql_handle, 10, &data, NULL); bdata->runestone.damage = atoi(data);
	Sql_GetData(sql_handle, 11, &data, NULL); bdata->runestone.repair = atoi(data);
	Sql_GetData(sql_handle, 12, &data, NULL); bdata->emperium.kills = atoi(data);
	Sql_GetData(sql_handle, 13, &data, NULL); bdata->emperium.damage = atoi(data);
	Sql_GetData(sql_handle, 14, &data, NULL); bdata->barricade.kills = atoi(data);
	Sql_GetData(sql_handle, 15, &data, NULL); bdata->barricade.damage = atoi(data);
	Sql_GetData(sql_handle, 16, &data, NULL); bdata->barricade.repair = atoi(data);
	Sql_GetData(sql_handle, 17, &data, NULL); bdata->objective.kills = atoi(data);
	Sql_GetData(sql_handle, 18, &data, NULL); bdata->objective.damage = atoi(data);
	Sql_GetData(sql_handle, 19, &data, NULL); bdata->crystal.kills = atoi(data);
	Sql_GetData(sql_handle, 20, &data, NULL); bdata->crystal.damage = atoi(data);
	Sql_GetData(sql_handle, 21, &data, NULL); bdata->flag.kills = atoi(data);
	Sql_GetData(sql_handle, 22, &data, NULL); bdata->flag.damage = atoi(data);
	Sql_GetData(sql_handle, 23, &data, NULL); bdata->flag.capture = atoi(data);
	Sql_GetData(sql_handle, 24, &data, NULL); bdata->flag.recapture = atoi(data);
	Sql_GetData(sql_handle, 25, &data, NULL); bdata->guardian.kills = atoi(data);
	Sql_GetData(sql_handle, 26, &data, NULL); bdata->guardian.deaths = atoi(data);
	Sql_GetData(sql_handle, 27, &data, NULL); bdata->guardian.damage_given = atoi(data);
	Sql_GetData(sql_handle, 28, &data, NULL); bdata->guardian.damage_taken = atoi(data);
	Sql_GetData(sql_handle, 29, &data, NULL); bdata->skill.support_success = atoi(data);
	Sql_GetData(sql_handle, 30, &data, NULL); bdata->skill.support_fail = atoi(data);
	Sql_GetData(sql_handle, 31, &data, NULL); bdata->skill.success = atoi(data);
	Sql_GetData(sql_handle, 32, &data, NULL); bdata->skill.fail = atoi(data);
	Sql_GetData(sql_handle, 33, &data, NULL); bdata->skill.damage_given = atoi(data);
	Sql_GetData(sql_handle, 34, &data, NULL); bdata->skill.damage_taken = atoi(data);
	Sql_GetData(sql_handle, 35, &data, NULL); bdata->heal.hp = atoi(data);
	Sql_GetData(sql_handle, 36, &data, NULL); bdata->heal.sp = atoi(data);
	Sql_GetData(sql_handle, 37, &data, NULL); bdata->heal.item_hp = atoi(data);
	Sql_GetData(sql_handle, 38, &data, NULL); bdata->heal.item_sp = atoi(data);
	Sql_GetData(sql_handle, 39, &data, NULL); bdata->item.heal = atoi(data);
	Sql_GetData(sql_handle, 40, &data, NULL); bdata->item.oridecon = atoi(data);
	Sql_GetData(sql_handle, 41, &data, NULL); bdata->item.elunium = atoi(data);
	Sql_GetData(sql_handle, 42, &data, NULL); bdata->item.steel = atoi(data);
	Sql_GetData(sql_handle, 43, &data, NULL); bdata->item.emveretarcon = atoi(data);
	Sql_GetData(sql_handle, 44, &data, NULL); bdata->item.wooden_block = atoi(data);
	Sql_GetData(sql_handle, 45, &data, NULL); bdata->item.stone = atoi(data);
	Sql_GetData(sql_handle, 46, &data, NULL); bdata->item.yellow_gemstone = atoi(data);
	Sql_GetData(sql_handle, 47, &data, NULL); bdata->item.red_gemstone = atoi(data);
	Sql_GetData(sql_handle, 48, &data, NULL); bdata->item.blue_gemstone = atoi(data);
	Sql_GetData(sql_handle, 49, &data, NULL); bdata->item.ammos = atoi(data);
	Sql_GetData(sql_handle, 50, &data, NULL); bdata->item.poison_bottle = atoi(data);
	Sql_GetData(sql_handle, 51, &data, NULL); bdata->item.fire_bottle = atoi(data);
	Sql_GetData(sql_handle, 52, &data, NULL); bdata->item.acid_bottle = atoi(data);
	Sql_FreeResult(sql_handle);
	return true;
}

bool bg_ranking_save(uint32 char_id, struct s_battleground_ranking *p, struct s_battleground_ranking *cp)
{
	if(
		(p->battle.wins != cp->battle.wins) || (p->battle.loss != cp->battle.loss) || (p->battle.tied != cp->battle.tied) ||
		(p->battle.score_wins != cp->battle.score_wins) || (p->battle.score_loss != p->battle.score_loss) || (p->player.kills != cp->player.kills) ||
		(p->player.deaths != cp->player.deaths) || (p->player.damage_given != cp->player.damage_given) || (p->player.damage_taken != cp->player.damage_taken) ||
		(p->runestone.kills != cp->runestone.kills) || (p->runestone.damage != cp->runestone.damage) || (p->runestone.repair != cp->runestone.repair) ||
		(p->emperium.kills != cp->emperium.kills) || (p->emperium.damage != cp->emperium.damage) || (p->barricade.kills != cp->barricade.kills) ||
		(p->barricade.damage != cp->barricade.damage) || (p->barricade.repair != cp->barricade.repair) || (p->emperium.kills != cp->emperium.kills) ||
		(p->objective.kills != cp->objective.kills) || (p->objective.damage != cp->objective.damage) || (p->crystal.kills != cp->crystal.kills) ||
		(p->crystal.damage != cp->crystal.damage) || (p->flag.kills != cp->flag.kills) || (p->flag.damage != cp->flag.damage) || (p->flag.capture != cp->flag.capture) ||
		(p->flag.recapture != cp->flag.recapture) || (p->guardian.deaths != cp->guardian.deaths) || (p->guardian.damage_given || cp->guardian.damage_given) ||
		(p->guardian.damage_taken != cp->guardian.damage_taken) || (p->skill.support_success != cp->skill.support_success) || (p->skill.support_fail != cp->skill.support_fail) ||
		(p->skill.success != cp->skill.success) || (p->skill.fail != cp->skill.fail) || (p->skill.damage_given != cp->skill.damage_given) || (p->skill.damage_taken != cp->skill.damage_taken) ||
		(p->heal.hp != cp->heal.hp) || (p->heal.sp != cp->heal.sp) || (p->heal.item_hp != cp->heal.item_hp) || (p->heal.item_sp != cp->heal.item_sp) ||
		(p->item.heal != p->item.heal) || (p->item.oridecon != cp->item.oridecon) || (p->item.elunium != cp->item.elunium) || (p->item.steel != cp->item.steel) ||
		(p->item.emveretarcon != cp->item.emveretarcon) || (p->item.wooden_block != cp->item.wooden_block) || (p->item.stone != cp->item.stone) ||
		(p->item.yellow_gemstone != cp->item.yellow_gemstone) || (p->item.red_gemstone != cp->item.red_gemstone) || (p->item.blue_gemstone != cp->item.blue_gemstone) ||
		(p->item.ammos != cp->item.ammos) || (p->item.poison_bottle != cp->item.poison_bottle) || (p->item.fire_bottle != cp->item.fire_bottle) || (p->item.acid_bottle != cp->item.acid_bottle)
	)
	{
		if (SQL_ERROR == Sql_Query(sql_handle, "REPLACE INTO `bg_ranking` (`char_id`, `bg_id`, `wins`, `loss`, `tied`, `score_wins`, `score_loss`, `kills`, `deaths`, "
			"`damage_given`, `damage_taken`, `runestone_kills`, `runestone_damage`, `runestone_repair`, `emperium_kills`, `emperium_damage`, `barrier_kills`, `barrier_damage`, "
			"`barrier_repair`, `objective_kills`, `objective_damage`, `cristal_kills`, `cristal_damage`, `flag_kills`, `flag_damage`, `flag_capture`, `flag_recapture`, "
			"`guard_kills`, `guard_deaths`, `guard_damage_given`, `guard_damage_taken`, `skill_support_success`, `skill_support_fail`, `skill_success`, `skill_fail`, "
			"`skill_damage_given`, `skill_damage_taken`, `heal_hp`, `heal_sp`, `item_heal_hp`,  `item_heal_sp`, `item_heal`, `oridecon`, `elunium`, `steel`, `emveretarcon`, "
			"`wooden_block`, `stones`, `yellow_gemstones`, `red_gemstones`, `blue_gemstones`, `ammos`, `poison_bottle`, `fire_bottle`, `acid_bottle`) "
			"VALUES (%d, 0, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)",
			char_id, p->battle.wins, p->battle.loss, p->battle.tied, p->battle.score_wins, p->battle.score_loss,
			p->player.kills, p->player.deaths, p->player.damage_given, p->player.damage_taken, p->runestone.kills,
			p->runestone.damage, p->runestone.repair, p->emperium.kills, p->emperium.damage, p->barricade.kills,
			p->barricade.damage, p->barricade.repair, p->objective.kills, p->objective.damage, p->crystal.kills,
			p->crystal.damage, p->flag.kills, p->flag.damage, p->flag.capture, p->flag.recapture, p->guardian.kills,
			p->guardian.deaths, p->guardian.damage_given, p->guardian.damage_taken, p->skill.support_success,
			p->skill.support_fail, p->skill.success, p->skill.fail, p->skill.damage_given, p->skill.damage_taken,
			p->heal.hp, p->heal.sp, p->heal.item_hp, p->heal.item_sp, p->item.heal, p->item.oridecon, p->item.elunium,
			p->item.steel, p->item.emveretarcon, p->item.wooden_block, p->item.stone, p->item.yellow_gemstone, p->item.red_gemstone,
			p->item.blue_gemstone, p->item.ammos, p->item.poison_bottle, p->item.fire_bottle, p->item.acid_bottle
		)) {
			Sql_ShowDebug(sql_handle);
			return false;
		}
	}
	return true;
}
