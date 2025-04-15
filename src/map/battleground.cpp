// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "battleground.hpp"

#include <unordered_map>

#include <common/cbasetypes.hpp>
#include <common/malloc.hpp>
#include <common/nullpo.hpp>
#include <common/random.hpp>
#include <common/showmsg.hpp>
#include <common/socket.hpp>
#include <common/strlib.hpp>
#include <common/timer.hpp>
#include <common/utilities.hpp>
#include <common/utils.hpp>

#include "battle.hpp"
#include "clif.hpp"
#include "channel.hpp"
#include "guild.hpp"
#include "homunculus.hpp"
#include "mapreg.hpp"
#include "mercenary.hpp"
#include "mob.hpp"
#include "npc.hpp"
#include "party.hpp"
#include "pc.hpp"
#include "pet.hpp"

using namespace rathena;

// [RomuloSM]: Battleground Expansion
struct guild bg_guild[2];
bool bg_happyhour_flag = false;

BattlegroundDatabase battleground_db;																					
std::unordered_map<int, std::shared_ptr<s_battleground_data>> bg_team_db;
std::vector<std::shared_ptr<s_battleground_queue>> bg_queues;
int bg_queue_count = 1;

const std::string BattlegroundDatabase::getDefaultLocation() {
	return std::string(db_path) + "/battleground_db.yml";
}

/**
 * Reads and parses an entry from the battleground_db
 * @param node: The YAML node containing the entry
 * @return count of successfully parsed rows
 */
uint64 BattlegroundDatabase::parseBodyNode(const ryml::NodeRef& node) {
	uint32 id;

	if (!this->asUInt32(node, "Id", id))
		return 0;

	std::shared_ptr<s_battleground_type> bg = this->find(id);
	bool exists = bg != nullptr;

	if (!exists) {
		if (!this->nodesExist(node, { "Name", "Locations" }))
			return 0;

		bg = std::make_shared<s_battleground_type>();
		bg->id = id;
	}

	if (this->nodeExists(node, "Name")) {
		std::string name;

		if (!this->asString(node, "Name", name))
			return 0;

		name.resize(NAME_LENGTH);
		bg->name = name;
	}

	// [RomuloSM]: Battleground Expansion
	if (this->nodeExists(node, "DisplayName")) {
		std::string display_name;
		if (!this->asString(node, "DisplayName", display_name))
			return 0;

		display_name.resize(NAME_LENGTH);
		bg->display_name = display_name;

	} else {
		if (!exists)
			bg->display_name = bg->name;
	}

	if (this->nodeExists(node, "MinPlayers")) {
		int min;

		if (!this->asInt32(node, "MinPlayers", min))
			return 0;

		if (min < 1) {
			this->invalidWarning(node["MinPlayers"], "Minimum players %d cannot be less than 1, capping to 1.\n", min);
			min = 1;
		}

		if (min * 2 > MAX_BG_MEMBERS) {
			this->invalidWarning(node["MinPlayers"], "Minimum players %d exceeds MAX_BG_MEMBERS, capping to %d.\n", min, MAX_BG_MEMBERS / 2);
			min = MAX_BG_MEMBERS / 2;
		}

		bg->required_players = min;
	} else {
		if (!exists)
			bg->required_players = 1;
	}

	if (this->nodeExists(node, "MaxPlayers")) {
		int max;

		if (!this->asInt32(node, "MaxPlayers", max))
			return 0;

		if (max < 1) {
			this->invalidWarning(node["MaxPlayers"], "Maximum players %d cannot be less than 1, capping to 1.\n", max);
			max = 1;
		}

		if (max * 2 > MAX_BG_MEMBERS) {
			this->invalidWarning(node["MaxPlayers"], "Maximum players %d exceeds MAX_BG_MEMBERS, capping to %d.\n", max, MAX_BG_MEMBERS / 2);
			max = MAX_BG_MEMBERS / 2;
		}

		bg->max_players = max;
	} else {
		if (!exists)
			bg->max_players = MAX_BG_MEMBERS / 2;
	}

	if (this->nodeExists(node, "MinLevel")) {
		int min;

		if (!this->asInt32(node, "MinLevel", min))
			return 0;

		if (min > MAX_LEVEL) {
			this->invalidWarning(node["MinLevel"], "Minimum level %d exceeds MAX_LEVEL, capping to %d.\n", min, MAX_LEVEL);
			min = MAX_LEVEL;
		}

		bg->min_lvl = min;
	} else {
		if (!exists)
			bg->min_lvl = 1;
	}

	if (this->nodeExists(node, "MaxLevel")) {
		int max;

		if (!this->asInt32(node, "MaxLevel", max))
			return 0;

		if (max > MAX_LEVEL) {
			this->invalidWarning(node["MaxLevel"], "Maximum level %d exceeds MAX_LEVEL, capping to %d.\n", max, MAX_LEVEL);
			max = MAX_LEVEL;
		}

		bg->max_lvl = max;
	} else {
		if (!exists)
			bg->max_lvl = MAX_LEVEL;
	}

	if (this->nodeExists(node, "Deserter")) {
		uint32 deserter;

		if (!this->asUInt32(node, "Deserter", deserter))
			return 0;

		bg->deserter_time = deserter;
	} else {
		if (!exists)
			bg->deserter_time = 600;
	}

	if (this->nodeExists(node, "StartDelay")) {
		uint32 delay;

		if (!this->asUInt32(node, "StartDelay", delay))
			return 0;

		bg->start_delay = delay;
	} else {
		if (!exists)
			bg->start_delay = 0;
	}

	if (this->nodeExists(node, "Join")) {
		const auto& joinNode = node["Join"];

		if (this->nodeExists(joinNode, "Solo")) {
			bool active;

			if (!this->asBool(joinNode, "Solo", active))
				return 0;

			bg->solo = active;
		} else {
			if (!exists)
				bg->solo = true;
		}

		if (this->nodeExists(joinNode, "Party")) {
			bool active;

			if (!this->asBool(joinNode, "Party", active))
				return 0;

			bg->party = active;
		} else {
			if (!exists)
				bg->party = true;
		}

		if (this->nodeExists(joinNode, "Guild")) {
			bool active;

			if (!this->asBool(joinNode, "Guild", active))
				return 0;

			bg->guild = active;
		} else {
			if (!exists)
				bg->guild = true;
		}
	} else {
		if (!exists) {
			bg->solo = true;
			bg->party = true;
			bg->guild = true;
		}
	}

	if (this->nodeExists(node, "JobRestrictions")) {
		const auto& jobsNode = node["JobRestrictions"];

		for (const auto& jobit : jobsNode) {
			std::string job_name;
			c4::from_chars(jobit.key(), &job_name);
			std::string job_name_constant = "JOB_" + job_name;
			int64 constant;

			if (!script_get_constant(job_name_constant.c_str(), &constant)) {
				this->invalidWarning(node["JobRestrictions"], "Job %s does not exist.\n", job_name.c_str());
				continue;
			}

			bool active;

			if (!this->asBool(jobsNode, job_name, active))
				return 0;

			if (active)
				bg->job_restrictions.push_back(static_cast<int32>(constant));
			else
				util::vector_erase_if_exists(bg->job_restrictions, static_cast<int32>(constant));
		}
	}

	if (this->nodeExists(node, "Locations")) {
		int count = 0;

		for (const auto& location : node["Locations"]) {
			s_battleground_map map_entry;

			if (this->nodeExists(location, "Map")) {
				std::string map_name;

				if (!this->asString(location, "Map", map_name))
					return 0;

				map_entry.mapindex = mapindex_name2id(map_name.c_str());

				if (map_entry.mapindex == 0) {
					this->invalidWarning(location["Map"], "Invalid battleground map name %s, skipping.\n", map_name.c_str());
					return 0;
				}

				if( map_mapindex2mapid( map_entry.mapindex ) < 0 ){
					// Ignore silently, the map is on another mapserver
					return 0;
				}
			}

			if (this->nodeExists(location, "StartEvent")) {
				if (!this->asString(location, "StartEvent", map_entry.bgcallscript))
					return 0;

				if (map_entry.bgcallscript.length() > EVENT_NAME_LENGTH) {
					this->invalidWarning(location["StartEvent"], "StartEvent \"%s\" exceeds maximum of %d characters, capping...\n", map_entry.bgcallscript.c_str(), EVENT_NAME_LENGTH - 1);
				}

				map_entry.bgcallscript.resize(EVENT_NAME_LENGTH);

				if (map_entry.bgcallscript.find("::On") == std::string::npos) {
					this->invalidWarning(location["StartEvent"], "Battleground StartEvent label %s should begin with '::On', skipping.\n", map_entry.bgcallscript.c_str());
					return 0;
				}
			}

			std::vector<std::string> team_list = { "TeamA", "TeamB" };

			for (const auto &it : team_list) {
				c4::csubstr team_name = c4::to_csubstr(it);
				const ryml::NodeRef& team = location;

				if (this->nodeExists(team, it)) {
					s_battleground_team *team_ptr;

					if (it.find("TeamA") != std::string::npos)
						team_ptr = &map_entry.team1;
					else if (it.find("TeamB") != std::string::npos)
						team_ptr = &map_entry.team2;
					else {
						this->invalidWarning(team[team_name], "An invalid Team is defined.\n");
						return 0;
					}

					if (this->nodeExists(team[team_name], "RespawnX")) {
						uint16 warp_x;

						if (!this->asUInt16(team[team_name], "RespawnX", warp_x))
							return 0;

						if (warp_x == 0) {
							this->invalidWarning(node["RespawnX"], "RespawnX has to be greater than zero.\n");
							return 0;
						}

						map_data *md = map_getmapdata(map_mapindex2mapid(map_entry.mapindex));

						if (warp_x >= md->xs) {
							this->invalidWarning(node["RespawnX"], "RespawnX has to be smaller than %hu.\n", md->xs);
							return 0;
						}

						team_ptr->warp_x = warp_x;
					}

					if (this->nodeExists(team[team_name], "RespawnY")) {
						uint16 warp_y;

						if (!this->asUInt16(team[team_name], "RespawnY", warp_y))
							return 0;

						if (warp_y == 0) {
							this->invalidWarning(node["RespawnY"], "RespawnY has to be greater than zero.\n");
							return 0;
						}

						map_data *md = map_getmapdata(map_mapindex2mapid(map_entry.mapindex));

						if (warp_y >= md->ys) {
							this->invalidWarning(node["RespawnY"], "RespawnY has to be smaller than %hu.\n", md->ys);
							return 0;
						}

						team_ptr->warp_y = warp_y;
					}

					if (this->nodeExists(team[team_name], "DeathEvent")) {
						if (!this->asString(team[team_name], "DeathEvent", team_ptr->death_event))
							return 0;

						team_ptr->death_event.resize(EVENT_NAME_LENGTH);

						if (team_ptr->death_event.find("::On") == std::string::npos) {
							this->invalidWarning(team["DeathEvent"], "Battleground DeathEvent label %s should begin with '::On', skipping.\n", team_ptr->death_event.c_str());
							return 0;
						}
					}

					if (this->nodeExists(team[team_name], "QuitEvent")) {
						if (!this->asString(team[team_name], "QuitEvent", team_ptr->quit_event))
							return 0;

						team_ptr->quit_event.resize(EVENT_NAME_LENGTH);

						if (team_ptr->quit_event.find("::On") == std::string::npos) {
							this->invalidWarning(team["QuitEvent"], "Battleground QuitEvent label %s should begin with '::On', skipping.\n", team_ptr->quit_event.c_str());
							return 0;
						}
					}

					if (this->nodeExists(team[team_name], "ActiveEvent")) {
						if (!this->asString(team[team_name], "ActiveEvent", team_ptr->active_event))
							return 0;

						team_ptr->active_event.resize(EVENT_NAME_LENGTH);

						if (team_ptr->active_event.find("::On") == std::string::npos) {
							this->invalidWarning(team["ActiveEvent"], "Battleground ActiveEvent label %s should begin with '::On', skipping.\n", team_ptr->active_event.c_str());
							return 0;
						}
					}

					if (this->nodeExists(team[team_name], "Variable")) {
						if (!this->asString(team[team_name], "Variable", team_ptr->bg_id_var))
							return 0;

						team_ptr->bg_id_var.resize(NAME_LENGTH);
					}

					// [RomuloSM]: Battleground Expansion
					if (this->nodeExists(team[team_name], "Team")) {
						std::string team_name_;

						if (!this->asString(team[team_name], "Team", team_name_))
							return 0;

						team_name_.resize(NAME_LENGTH);

						if( strcmp(team_name_.c_str(),"Guillaume") != 0 && strcmp(team_name_.c_str(),"Croix") != 0 ) {
							this->invalidWarning(team[team_name], "Invalid Team '%s'.\n", team_name_);
							return 0;
						}

						int teamId = strcmp(team_name_.c_str(),"Guillaume") == 0 ? 0 : 1;
						team_ptr->teamId = teamId;
					}
					else {
						this->invalidWarning(team[team_name], "Battleground Team not set, skipping.\n");
						return 0;
					}

					map_entry.id = count++;
					map_entry.isReserved = false;
				}
			}

			bg->maps.push_back(map_entry);
		}
	}

	if (!exists)
		this->put(id, bg);

	return 1;
}

/**
 * Search for a battleground based on the given name
 * @param name: Battleground name
 * @return s_battleground_type on success or nullptr on failure
 */
std::shared_ptr<s_battleground_type> bg_search_name(const char *name)
{
	for (const auto &entry : battleground_db) {
		auto bg = entry.second;

		if (!stricmp(bg->name.c_str(), name))
			return bg;
	}

	return nullptr;
}

/**
 * Search for a Battleground queue based on the given queue ID
 * @param queue_id: Queue ID
 * @return s_battleground_queue on success or nullptr on failure
 */
std::shared_ptr<s_battleground_queue> bg_search_queue(int queue_id)
{
	for (const auto &queue : bg_queues) {
		if (queue_id == queue->queue_id)
			return queue;
	}

	return nullptr;
}

/**
 * Search for an available player in Battleground
 * @param bg: Battleground data
 * @return map_session_data
 */
map_session_data* bg_getavailablesd(s_battleground_data *bg)
{
	nullpo_retr(nullptr, bg);

	for (const auto &member : bg->members) {
		if (member.sd != nullptr)
			return member.sd;
	}

	return nullptr;
}

/**
 * Delete a Battleground team from the db
 * @param bg_id: Battleground ID
 * @return True on success or false otherwise
 */
bool bg_team_delete(int bg_id)
{
	std::shared_ptr<s_battleground_data> bgteam = util::umap_find(bg_team_db, bg_id);

	if (bgteam) {
		for (const auto &pl_sd : bgteam->members) {
			bg_send_dot_remove(pl_sd.sd);
			pl_sd.sd->bg_id = 0;
		}

		// [RomuloSM]: Battleground Expansion
		if( bgteam->expansion.channel_name.c_str() ) {
			struct Channel *ch = channel_searchname((char *)bgteam->expansion.channel_name.c_str());
			if( ch )
				channel_delete(ch,false);
		}

		if( bgteam->expansion.cd_tid != INVALID_TIMER )
			delete_timer(bgteam->expansion.cd_tid, bg_countdown);
		bgteam->expansion.cd_tid = INVALID_TIMER;

		bg_team_db.erase(bg_id);

		return true;
	}
	
	return false;
}

/**
 * Warps a Battleground team
 * @param bg_id: Battleground ID
 * @param mapindex: Map Index
 * @param x: X coordinate
 * @param y: Y coordinate
 * @return True on success or false otherwise
 */
bool bg_team_warp(int bg_id, unsigned short mapindex, short x, short y)
{
	std::shared_ptr<s_battleground_data> bgteam = util::umap_find(bg_team_db, bg_id);

	if (bgteam) {
		for (const auto &pl_sd : bgteam->members)
			pc_setpos(pl_sd.sd, mapindex, x, y, CLR_TELEPORT);

		return true;
	}

	return false;
}

/**
 * Remove a player's Battleground map marker
 * @param sd: Player data
 */
void bg_send_dot_remove(map_session_data *sd)
{
	nullpo_retv(sd);

	if( sd && sd->bg_id )
		clif_bg_xy_remove(sd);
	return;
}

/**
 * Join a player to a Battleground team
 * @param bg_id: Battleground ID
 * @param sd: Player data
 * @param is_queue: Joined from queue
 * @return True on success or false otherwise
 */
bool bg_team_join(int bg_id, map_session_data *sd, bool is_queue, bool flag)
{
	if (!sd || sd->bg_id)
		return false;

	std::shared_ptr<s_battleground_data> bgteam = util::umap_find(bg_team_db, bg_id);

	if (bgteam) {
		if (bgteam->members.size() == MAX_BG_MEMBERS)
			return false; // No free slots

		s_battleground_member_data member = {};

		sd->bg_id = bg_id;
		member.sd = sd;
		member.x = sd->bl.x;
		member.y = sd->bl.y;
		if (is_queue) { // Save the location from where the person entered the battleground
			member.entry_point.map = sd->mapindex;
			member.entry_point.x = sd->bl.x;
			member.entry_point.y = sd->bl.y;
		}
		
		// [RomuloSM]: Battleground Expansion
		sd->idletime = last_tick;
		sd->bge.guild_notice = false;
		if( bgteam->members.size() == 0 ) bgteam->expansion.master_id = sd->status.char_id;
		pc_setglobalreg(sd, add_str(BG_CLOTHERSCOLOR_VAR), sd->status.clothes_color);

		bgteam->members.push_back(member);

		guild_send_dot_remove(sd);

		for (const auto &pl_sd : bgteam->members) {
			if (pl_sd.sd != sd)
				clif_hpmeter_single( *sd, pl_sd.sd->bl.id, pl_sd.sd->battle_status.hp, pl_sd.sd->battle_status.max_hp );

			// [RomuloSM]: Battleground Expansion
			memset(&sd->bge.ranking, 0, sizeof(sd->bge.ranking));

			if( battle_config.bg_expansion_enable ) {
				char output[128];
				clif_fake_guild_emblem(pl_sd.sd, bgteam->expansion.g);

				if( pl_sd.sd != sd ) {
					sprintf(output, msg_txt(pl_sd.sd, 1662), sd->status.name);
					clif_messagecolor(&pl_sd.sd->bl, BGE_COLOR, output, true, SELF);
				}
				else {
					sprintf(output, msg_txt(pl_sd.sd, 1654), msg_txt(pl_sd.sd,(bgteam->expansion.teamId ? 1671 : 1665)), sd->status.name);
					clif_messagecolor(&pl_sd.sd->bl, BGE_COLOR, output, true, SELF);
					if( battle_config.bg_channel && bgteam->expansion.channel_name.c_str() ) {
						struct Channel *chan = channel_searchname((char*)bgteam->expansion.channel_name.c_str());
						if( chan != NULL ) {
							channel_join(chan,pl_sd.sd);
							sprintf(output, msg_txt(pl_sd.sd,1677), bgteam->expansion.channel_name.c_str());
							clif_messagecolor(&pl_sd.sd->bl, BGE_COLOR, output, true, SELF);
						}
					}
					clif_bg_notice(sd);
				}
			}
		}

		clif_bg_hp(sd);
		clif_bg_xy(sd);
		return true;
	}

	return false;
}

/**
 * Remove a player from Battleground team
 * @param sd: Player data
 * @param quit: True if closed client or false otherwise
 * @param deserter: Whether to apply the deserter status or not
 * @return Remaining count in Battleground team or -1 on failure
 */
int bg_team_leave(map_session_data *sd, bool quit, bool deserter, enum bg_team_leave_type flag)
{
	if (!sd || !sd->bg_id)
		return -1;

	bg_send_dot_remove(sd);

	int bg_id = sd->bg_id;
	std::shared_ptr<s_battleground_data> bgteam = util::umap_find(bg_team_db, bg_id);

	sd->bg_id = 0;

	// [RomuloSM]: Battleground Expansion
	int clothers_color = static_cast<int>(pc_readglobalreg(sd, add_str(BG_CLOTHERSCOLOR_VAR)));
	pc_changelook(sd, LOOK_CLOTHES_COLOR, clothers_color);
	pc_setglobalreg(sd, add_str(BG_CLOTHERSCOLOR_VAR), clothers_color);

	if (bgteam) {
		// Warping members out only applies to the Battleground Queue System
		if (battle_config.feature_bgqueue) {
			auto member = bgteam->members.begin();

			while (member != bgteam->members.end()) {
				if (member->sd == sd) {
					if (member->entry_point.map != 0 && !map_getmapflag(map_mapindex2mapid(member->entry_point.map), MF_NOSAVE))
						pc_setpos(sd, member->entry_point.map, member->entry_point.x, member->entry_point.y, CLR_TELEPORT);
					else
						pc_setpos( sd, mapindex_name2id( sd->status.save_point.map ), sd->status.save_point.x, sd->status.save_point.y, CLR_TELEPORT ); // Warp to save point if the entry map has no save flag.

					bgteam->members.erase(member);
					break;
				} else
					member++;
			}
		}

		// [RomuloSM]: Countdown Clear
		clif_showdigit(sd, (unsigned char)3, 0);

		// [RomuloSM]: Battleground Expansion
		if( battle_config.bg_expansion_enable ) {
			auto g = sd->guild;

			// Return Normal Guild
			status_change_end(&sd->bl,SC_GUILDAURA,INVALID_TIMER);
			status_change_end(&sd->bl,SC_BATTLEORDERS,INVALID_TIMER);
			status_change_end(&sd->bl,SC_REGENERATION,INVALID_TIMER);
			
			if( sd->status.guild_id && g != nullptr )
			{
				clif_guild_belonginfo( *sd );
				clif_guild_basicinfo( *sd );
				clif_guild_allianceinfo(sd);
				clif_guild_memberlist( *sd );
				clif_guild_skillinfo(sd);
				clif_guild_emblem(*sd, g->guild);
			}
	
			clif_name_area(&sd->bl);
			clif_guild_emblem_area(&sd->bl);

			memset(&sd->bge.ranking, 0, sizeof(sd->bge.ranking));
			bg_block_skill_end(sd);

			// Leave Notify and Change Master
			bg_leave_notify(bgteam, sd, flag, (bgteam->expansion.master_id == sd->status.char_id));
		}
		else {
			char output[CHAT_SIZE_MAX];

			if (quit)
				sprintf(output, "Server: %s has quit the game...", sd->status.name);
			else
				sprintf(output, "Server: %s is leaving the battlefield...", sd->status.name);

			clif_bg_message(bgteam.get(), 0, "Server", output, strlen(output) + 1);
		}

		if (!bgteam->logout_event.empty() && quit)
			npc_event(sd, bgteam->logout_event.c_str(), 0);

		if (deserter) {
			std::shared_ptr<s_battleground_type> bg = battleground_db.find(bg_id);

			if (bg)
				sc_start(nullptr, &sd->bl, SC_ENTRY_QUEUE_NOTIFY_ADMISSION_TIME_OUT, 100, 1, static_cast<t_tick>(bg->deserter_time) * 1000); // Deserter timer
		}

		return bgteam->members.size();
	}

	return -1;
}

/**
 * Respawn a Battleground player
 * @param sd: Player data
 * @return True on success or false otherwise
 */
bool bg_member_respawn(map_session_data *sd)
{
	if (!sd || !sd->bg_id || !pc_isdead(sd))
		return false;

	std::shared_ptr<s_battleground_data> bgteam = util::umap_find(bg_team_db, sd->bg_id);

	if (bgteam) {
		if (bgteam->cemetery.map == 0)
			return false; // Respawn not handled by Core

		pc_setpos(sd, bgteam->cemetery.map, bgteam->cemetery.x, bgteam->cemetery.y, CLR_OUTSIGHT);
		status_revive(&sd->bl, 1, 100);

		return true; // Warped
	}

	return false;
}

/**
 * Initialize Battleground data
 * @param mapindex: Map Index
 * @param rx: Return X coordinate (on death)
 * @param ry: Return Y coordinate (on death)
 * @param ev: Logout NPC Event
 * @param dev: Death NPC Event
 * @return Battleground ID
 */
int bg_create(uint16 mapindex, s_battleground_team* team)
{
	int bg_team_counter = 1;

	while (bg_team_db.find(bg_team_counter) != bg_team_db.end())
		bg_team_counter++;

	bg_team_db[bg_team_counter] = std::make_shared<s_battleground_data>();

	std::shared_ptr<s_battleground_data> bg = util::umap_find(bg_team_db, bg_team_counter);

	bg->id = bg_team_counter;
	bg->cemetery.map = mapindex;
	bg->cemetery.x = team->warp_x;
	bg->cemetery.y = team->warp_y;
	bg->logout_event = team->quit_event.c_str();
	bg->die_event = team->death_event.c_str();
	bg->active_event = team->active_event.c_str();

	// [RomuloSM]: Battleground Expansion
	bg->expansion.teamId = team->teamId;
	bg->expansion.g = &bg_guild[team->teamId];
	bg->expansion.guild_id = bg->expansion.g->guild_id;
	bg->expansion.cd_sec = 0;
	bg->expansion.cd_tid = INVALID_TIMER;

	if( battle_config.bg_channel )
		bg_create_channel(bg->id);

	return bg->id;
}

/**
 * Get an object's Battleground ID
 * @param bl: Object
 * @return Battleground ID
 */
int bg_team_get_id(struct block_list *bl)
{
	nullpo_ret(bl);

	switch( bl->type ) {
		case BL_PC:
			return ((TBL_PC*)bl)->bg_id;
		case BL_PET:
			if( ((TBL_PET*)bl)->master )
				return ((TBL_PET*)bl)->master->bg_id;
			break;
		case BL_MOB: {
			map_session_data *msd;
			struct mob_data *md = (TBL_MOB*)bl;

			if( md->special_state.ai && (msd = map_id2sd(md->master_id)) != nullptr )
				return msd->bg_id;

			return md->bg_id;
		}
		case BL_HOM:
			if( ((TBL_HOM*)bl)->master )
				return ((TBL_HOM*)bl)->master->bg_id;
			break;
		case BL_MER:
			if( ((TBL_MER*)bl)->master )
				return ((TBL_MER*)bl)->master->bg_id;
			break;
		case BL_SKILL:
			return ((TBL_SKILL*)bl)->group->bg_id;
		case BL_NPC:
			if( ((TBL_NPC*)bl)->subtype == NPCTYPE_SCRIPT )
				return ((TBL_NPC*)bl)->bg_id;
			break;
	}

	return 0;
}

/**
 * Send a Battleground chat message
 * @param sd: Player data
 * @param mes: Message
 * @param len: Message length
 */
void bg_send_message(map_session_data *sd, const char *mes, int len)
{
	nullpo_retv(sd);

	if (sd->bg_id == 0)
		return;
	
	std::shared_ptr<s_battleground_data> bgteam = util::umap_find(bg_team_db, sd->bg_id);

	if (bgteam)
		clif_bg_message(bgteam.get(), sd->bl.id, sd->status.name, mes, len);

	return;
}

/**
 * Update a player's Battleground minimap icon
 * @see DBApply
 */
int bg_send_xy_timer_sub(std::shared_ptr<s_battleground_data> bg)
{
	map_session_data *sd;

	for (auto &pl_sd : bg->members) {
		sd = pl_sd.sd;

		if (sd->bl.x != pl_sd.x || sd->bl.y != pl_sd.y) { // xy update
			pl_sd.x = sd->bl.x;
			pl_sd.y = sd->bl.y;
			clif_bg_xy(sd);
		}

		// [RomuloSM]: BG Expansion
		if( battle_config.bg_expansion_enable && battle_config.bg_idle_autokick ) {
			if( DIFF_TICK(last_tick, sd->idletime) >= battle_config.bg_idle_autokick ) {
				bg_team_leave(sd, true, true, BGTL_KICK_IDLE);
				clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1663), false, SELF);
			}
		}
	}

	return 0;
}

/**
 * Update a player's Battleground minimap icon
 * @param tid: Timer ID
 * @param tick: Timer
 * @param id: ID
 * @return 0 on success or 1 otherwise
 */
TIMER_FUNC(bg_send_xy_timer)
{
	for (const auto &entry : bg_team_db)
		bg_send_xy_timer_sub(entry.second);

	return 0;
}

/**
 * Mark a Battleground as ready to begin queuing for a free map
 * @param tid: Timer ID
 * @param tick: Timer
 * @param id: ID
 * @return 0 on success or 1 otherwise
 */
static TIMER_FUNC(bg_on_ready_loopback)
{
	int queue_id = (int)data;
	std::shared_ptr<s_battleground_queue> queue = bg_search_queue(queue_id);

	if (queue == nullptr) {
		ShowError("bg_on_ready_loopback: Invalid battleground queue %d.\n", queue_id);
		return 1;
	}

	std::shared_ptr<s_battleground_type> bg = battleground_db.find(queue->id);

	if (bg) {
		bg_queue_on_ready(bg->name.c_str(), queue);
		return 0;
	} else {
		ShowError("bg_on_ready_loopback: Can't find battleground %d in the battlegrounds database.\n", queue->id);
		return 1;
	}
}

/**
 * Reset Battleground queue data if players don't accept in time
 * @param tid: Timer ID
 * @param tick: Timer
 * @param id: ID
 * @return 0 on success or 1 otherwise
 */
static TIMER_FUNC(bg_on_ready_expire)
{
	int queue_id = (int)data;
	std::shared_ptr<s_battleground_queue> queue = bg_search_queue(queue_id);

	if (queue == nullptr) {
		ShowError("bg_on_ready_expire: Invalid battleground queue %d.\n", queue_id);
		return 1;
	}

	std::string bg_name = battleground_db.find(queue->id)->name;

	for (const auto &sd : queue->teama_members) {
		clif_bg_queue_apply_result(BG_APPLY_QUEUE_FINISHED, bg_name.c_str(), sd);
		clif_bg_queue_entry_init(sd);
	}

	for (const auto &sd : queue->teamb_members) {
		clif_bg_queue_apply_result(BG_APPLY_QUEUE_FINISHED, bg_name.c_str(), sd);
		clif_bg_queue_entry_init(sd);
	}

	bg_queue_clear(queue, true);
	return 0;
}

/**
 * Start a Battleground when all players have accepted
 * @param tid: Timer ID
 * @param tick: Timer
 * @param id: ID
 * @return 0 on success or 1 otherwise
 */
static TIMER_FUNC(bg_on_ready_start)
{
	int queue_id = (int)data;
	std::shared_ptr<s_battleground_queue> queue = bg_search_queue(queue_id);

	if (queue == nullptr) {
		ShowError("bg_on_ready_start: Invalid battleground queue %d.\n", queue_id);
		return 1;
	}

	queue->tid_start = INVALID_TIMER;
	bg_queue_start_battleground(queue);

	return 0;
}

/**
 * Check if the given player is in a battleground
 * @param sd: Player data
 * @return True if in a battleground or false otherwise
 */
bool bg_player_is_in_bg_map(map_session_data *sd)
{
	nullpo_retr(false, sd);

	for (const auto &pair : battleground_db) {
		for (const auto &it : pair.second->maps) {
			if (it.mapindex == sd->mapindex)
				return true;
		}
	}

	return false;
}

/**
 * Battleground status change check
 * @param sd: Player data
 * @param name: Battleground name
 * @return True if the player is good to join a queue or false otherwise
 */
static bool bg_queue_check_status(map_session_data* sd, const char *name)
{
	nullpo_retr(false, sd);

	if (sd->sc.count) {
		if (sd->sc.getSCE(SC_ENTRY_QUEUE_APPLY_DELAY)) { // Exclude any player who's recently left a battleground queue
			char buf[CHAT_SIZE_MAX];

			sprintf(buf, msg_txt(sd, 339), static_cast<int32>((get_timer(sd->sc.getSCE(SC_ENTRY_QUEUE_APPLY_DELAY)->timer)->tick - gettick()) / 1000)); // You can't apply to a battleground queue for %d seconds due to recently leaving one.
			clif_bg_queue_apply_result(BG_APPLY_NONE, name, sd);
			clif_messagecolor(&sd->bl, color_table[COLOR_LIGHT_GREEN], buf, false, SELF);
			return false;
		}

		if (sd->sc.getSCE(SC_ENTRY_QUEUE_NOTIFY_ADMISSION_TIME_OUT)) { // Exclude any player who's recently deserted a battleground
			char buf[CHAT_SIZE_MAX];
			int32 status_tick = static_cast<int32>(DIFF_TICK(get_timer(sd->sc.getSCE(SC_ENTRY_QUEUE_NOTIFY_ADMISSION_TIME_OUT)->timer)->tick, gettick()) / 1000);

			sprintf(buf, msg_txt(sd, 338), status_tick / 60, status_tick % 60); // You can't apply to a battleground queue due to recently deserting a battleground. Time remaining: %d minutes and %d seconds.
			clif_bg_queue_apply_result(BG_APPLY_NONE, name, sd);
			clif_messagecolor(&sd->bl, color_table[COLOR_LIGHT_GREEN], buf, false, SELF);
			return false;
		}
	}

	return true;
}

/**
 * Check to see if a Battleground is joinable
 * @param bg: Battleground data
 * @param sd: Player data
 * @param name: Battleground name
 * @return True on success or false otherwise
 */
bool bg_queue_check_joinable(std::shared_ptr<s_battleground_type> bg, map_session_data *sd, const char *name)
{
	nullpo_retr(false, sd);

	for (const auto &job : bg->job_restrictions) { // Check class requirement
		if (sd->status.class_ == job) {
			sd->bge.apply_error = BG_APPLY_PLAYER_CLASS; // [RomuloSM]: Battleground Expansion
			clif_bg_queue_apply_result(BG_APPLY_PLAYER_CLASS, name, sd);
			return false;
		}
	}

	if (bg->min_lvl > 0 && sd->status.base_level < bg->min_lvl) { // Check minimum level requirement
		sd->bge.apply_error = BG_APPLY_PLAYER_LEVEL; // [RomuloSM]: Battleground Expansion
		clif_bg_queue_apply_result(BG_APPLY_PLAYER_LEVEL, name, sd);
		return false;
	}

	if (bg->max_lvl > 0 && sd->status.base_level > bg->max_lvl) { // Check maximum level requirement
		sd->bge.apply_error = BG_APPLY_PLAYER_LEVEL; // [RomuloSM]: Battleground Expansion
		clif_bg_queue_apply_result(BG_APPLY_PLAYER_LEVEL, name, sd);
		return false;
	}

	if (!bg_queue_check_status(sd, name)) { // Check status blocks
		sd->bge.apply_error = BG_APPLY_NONE; // [RomuloSM]: Battleground Expansion
		return false;
	}

	if (bg_player_is_in_bg_map(sd)) { // Is the player currently in a battleground map? Reject them.
		sd->bge.apply_error = BG_APPLY_NONE; // [RomuloSM]: Battleground Expansion
		clif_bg_queue_apply_result(BG_APPLY_NONE, name, sd);
		clif_messagecolor(&sd->bl, color_table[COLOR_LIGHT_GREEN], msg_txt(sd, 337), false, SELF); // You can't apply to a battleground queue from this map.
		return false;
	}

	if (battle_config.bgqueue_nowarp_mapflag > 0 && map_getmapflag(sd->bl.m, MF_NOWARP)) { // Check player's current position for mapflag check
		sd->bge.apply_error = BG_APPLY_NONE; // [RomuloSM]: Battleground Expansion
		clif_bg_queue_apply_result(BG_APPLY_NONE, name, sd);
		clif_messagecolor(&sd->bl, color_table[COLOR_LIGHT_GREEN], msg_txt(sd, 337), false, SELF); // You can't apply to a battleground queue from this map.
		return false;
	}

	return true;
}

/**
 * Mark a map as reserved for a Battleground
 * @param name: Battleground map name
 * @param state: Whether to mark reserved or not
 * @param ended: Whether the Battleground event is complete; players getting prize
 * @return True on success or false otherwise
 */
bool bg_queue_reservation(const char *name, bool state, bool ended)
{
	uint16 mapindex = mapindex_name2id(name);

	for (auto &pair : battleground_db) {
		for (auto &map : pair.second->maps) {
			if (map.mapindex == mapindex) {
				map.isReserved = state;
				for (auto &queue : bg_queues) {
					if (queue->map == &map) {
						if (ended) // The ended flag is applied from bg_reserve (bg_unbook clears it for the next queue)
							queue->state = QUEUE_STATE_ENDED;
						if (!state)
							bg_queue_clear(queue, true);
					}
				}
				return true;
			}
		}
	}

	return false;
}

/**
 * Join as an individual into a Battleground
 * @param name: Battleground name
 * @param sd: Player who requested to join the battlegrounds
 */
void bg_queue_join_solo(const char *name, map_session_data *sd)
{
	if (!sd) {
		ShowError("bg_queue_join_solo: Tried to join non-existent player\n.");
		return;
	}

	std::shared_ptr<s_battleground_type> bg = bg_search_name(name);

	if (!bg) {
		ShowWarning("bq_queue_join_solo: Could not find battleground \"%s\" requested by %s (AID: %d / CID: %d)\n", name, sd->status.name, sd->status.account_id, sd->status.char_id);
		return;
	}

	if (!bg->solo) {
		clif_bg_queue_apply_result(BG_APPLY_INVALID_APP, name, sd);
		return;
	}

	bg_queue_join_multi(name, sd, { sd }); // Join as solo
}

/**
 * Join a party onto the same side of a Battleground
 * @param name: Battleground name
 * @param sd: Player who requested to join the battlegrounds
 */
void bg_queue_join_party(const char *name, map_session_data *sd)
{
	if (!sd) {
		ShowError("bg_queue_join_party: Tried to join non-existent player\n.");
		return;
	}

	struct party_data *p = party_search(sd->status.party_id);

	if (!p) {
		clif_bg_queue_apply_result(BG_APPLY_INVALID_APP, name, sd);
		return; // Someone has bypassed the client check for being in a party
	}

	for (const auto &it : p->party.member) {
		if (it.leader && sd->status.char_id != it.char_id) {
			clif_bg_queue_apply_result(BG_APPLY_PARTYGUILD_LEADER, name, sd);
			return; // Not the party leader
		}
	}

	std::shared_ptr<s_battleground_type> bg = bg_search_name(name);

	if (bg) {
		if (!bg->party) {
			clif_bg_queue_apply_result(BG_APPLY_INVALID_APP, name, sd);
			return;
		}

		int p_online = 0;

		for (const auto &it : p->party.member) {
			if (it.online)
				p_online++;
		}

		if (p_online > bg->max_players) {
			clif_bg_queue_apply_result(BG_APPLY_PLAYER_COUNT, name, sd);
			return; // Too many party members online
		}

		std::vector<map_session_data *> list;

		for (const auto &it : p->party.member) {
			if (list.size() == bg->max_players)
				break;

			if (it.online) {
				map_session_data *pl_sd = map_charid2sd(it.char_id);

				if (pl_sd)
					list.push_back(pl_sd);
			}
		}

		bg_queue_join_multi(name, sd, list); // Join as party, all on the same side of the BG
	} else {
		ShowWarning("clif_parse_bg_queue_apply_request: Could not find Battleground: \"%s\" requested by player: %s (AID:%d CID:%d)\n", name, sd->status.name, sd->status.account_id, sd->status.char_id);
		clif_bg_queue_apply_result(BG_APPLY_INVALID_NAME, name, sd);
		return; // Invalid BG name
	}
}

/**
 * Join a guild onto the same side of a Battleground
 * @param name: Battleground name
 * @param sd: Player who requested to join the battlegrounds
 */
void bg_queue_join_guild(const char *name, map_session_data *sd)
{
	if (!sd) {
		ShowError("bg_queue_join_guild: Tried to join non-existent player\n.");
		return;
	}

	if (!sd->guild) {
		clif_bg_queue_apply_result(BG_APPLY_INVALID_APP, name, sd);
		return; // Someone has bypassed the client check for being in a guild
	}
	
	if (strcmp(sd->status.name, sd->guild->guild.master) != 0) {
		clif_bg_queue_apply_result(BG_APPLY_PARTYGUILD_LEADER, name, sd);
		return; // Not the guild leader
	}

	std::shared_ptr<s_battleground_type> bg = bg_search_name(name);

	if (bg) {
		if (!bg->guild) {
			clif_bg_queue_apply_result(BG_APPLY_INVALID_APP, name, sd);
			return;
		}

		auto &g = sd->guild;

		if (g->guild.connect_member > bg->max_players) {
			clif_bg_queue_apply_result(BG_APPLY_PLAYER_COUNT, name, sd);
			return; // Too many guild members online
		}

		std::vector<map_session_data *> list;

		for (const auto &it : g->guild.member) {
			if (list.size() == bg->max_players)
				break;

			if (it.online) {
				map_session_data *pl_sd = map_charid2sd(it.char_id);

				if (pl_sd)
					list.push_back(pl_sd);
			}
		}

		bg_queue_join_multi(name, sd, list); // Join as guild, all on the same side of the BG
	} else {
		ShowWarning("clif_parse_bg_queue_apply_request: Could not find Battleground: \"%s\" requested by player: %s (AID:%d CID:%d)\n", name, sd->status.name, sd->status.account_id, sd->status.char_id);
		clif_bg_queue_apply_result(BG_APPLY_INVALID_NAME, name, sd);
		return; // Invalid BG name
	}
}

/**
 * Join multiple players onto the same side of a Battleground
 * @param name: Battleground name
 * @param sd: Player who requested to join the battlegrounds
 * @param list: Contains all players including the player who requested to join
 */
uint16 bg_queue_join_multi(const char *name, map_session_data *sd, std::vector <map_session_data *> list)
{
	if (!sd) {
		ShowError("bg_queue_join_multi: Tried to join non-existent player\n.");
		return BG_APPLY_NONE;
	}

	std::shared_ptr<s_battleground_type> bg = bg_search_name(name);

	if (!bg) {
		ShowWarning("bq_queue_join_multi: Could not find battleground \"%s\" requested by %s (AID: %d / CID: %d)\n", name, sd->status.name, sd->status.account_id, sd->status.char_id);
		return BG_APPLY_INVALID_NAME;
	}

	if (!bg_queue_check_joinable(bg, sd, name)){
		return sd->bge.apply_error; // [RomuloSM]: Battleground Expansion
	}

	// [RomuloSM]: Battleground Expansion
	if( bg_check_dual(sd) ) {
		sd->bge.apply_error = BG_APPLY_INVALID_APP;
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd, 1698), false, SELF);
		clif_bg_queue_apply_result(BG_APPLY_INVALID_APP, name, sd);
		return BG_APPLY_INVALID_APP;
	}

	for (const auto &queue : bg_queues) {
		if (queue->id != bg->id || queue->state == QUEUE_STATE_SETUP_DELAY || queue->state == QUEUE_STATE_ENDED)
			continue;

		// Make sure there's enough space on one side to join as a party/guild in this queue
		if (queue->teama_members.size() + list.size() > bg->max_players && queue->teamb_members.size() + list.size() > bg->max_players) {
			break;
		}

		bool r = rnd() % 2 != 0;
		std::vector<map_session_data *> *team = r ? &queue->teamb_members : &queue->teama_members;

		if (queue->state == QUEUE_STATE_ACTIVE) {
			// If one team has lesser members try to balance (on an active BG)
			if (r && queue->teama_members.size() < queue->teamb_members.size())
				team = &queue->teama_members;
			else if (!r && queue->teamb_members.size() < queue->teama_members.size())
				team = &queue->teamb_members;
		} else {
			// If the designated team is full, put the player into the other team
			if (team->size() + list.size() > bg->required_players)
				team = r ? &queue->teama_members : &queue->teamb_members;
		}

		while (!list.empty() && team->size() < bg->max_players) {
			map_session_data *sd2 = list.back();

			list.pop_back();

			if (!sd2 || sd2->bg_queue_id > 0)
				continue;

			if (!bg_queue_check_joinable(bg, sd2, name))
				continue;

			sd2->bg_queue_id = queue->queue_id;
			team->push_back(sd2);

			// [RomuloSM]: Announce Players
			if (battle_config.bg_announce) {
				char output[200];
				int member_count = (bg->required_players*2)-(queue->teama_members.size()+queue->teamb_members.size());
				if( member_count <= 0 ) {
					member_count = (bg->max_players*2)-(queue->teama_members.size()+queue->teamb_members.size());
					sprintf(output, msg_txt(NULL,1694), sd2->status.name, bg->display_name.c_str(), member_count);
				} else {
					sprintf(output, msg_txt(NULL,1678), sd2->status.name, bg->display_name.c_str(), member_count);
				}
				clif_broadcast2(&sd2->bl, output, strlen(output)+1, BGE_COLOR, FW_NORMAL, 12, 0, 0, BG_LISTEN);
			}

			clif_bg_queue_apply_result(BG_APPLY_ACCEPT, name, sd2);
			clif_bg_queue_apply_notify(name, sd2);
		}

		if (queue->state == QUEUE_STATE_ACTIVE) { // Battleground is already active
			for (auto &pl_sd : *team) {
				if (queue->map->mapindex == pl_sd->mapindex)
					continue;

				pc_set_bg_queue_timer(pl_sd);
				clif_bg_queue_lobby_notify(name, pl_sd);
			}
		} else if (queue->state == QUEUE_STATE_SETUP && queue->teamb_members.size() >= bg->required_players && queue->teama_members.size() >= bg->required_players) // Enough players have joined
			bg_queue_on_ready(name, queue);

		return BG_APPLY_ACCEPT;
	}

	// Something went wrong, sends reconnect and then reapply message to client.
	clif_bg_queue_apply_result(BG_APPLY_RECONNECT, name, sd);
	return BG_APPLY_RECONNECT;
}

/**
 * Clear Battleground queue for next one
 * @param queue: Queue to clean up
 * @param ended: If a Battleground has ended through normal means (by script command bg_unbook)
 */
void bg_queue_clear(std::shared_ptr<s_battleground_queue> queue, bool ended)
{
	if (queue == nullptr)
		return;

	if (queue->tid_requeue != INVALID_TIMER) {
		delete_timer(queue->tid_requeue, bg_on_ready_loopback);
		queue->tid_requeue = INVALID_TIMER;
	}

	if (queue->tid_expire != INVALID_TIMER) {
		delete_timer(queue->tid_expire, bg_on_ready_expire);
		queue->tid_expire = INVALID_TIMER;
	}

	if (queue->tid_start != INVALID_TIMER) {
		delete_timer(queue->tid_start, bg_on_ready_start);
		queue->tid_start = INVALID_TIMER;
	}

	if (ended) {
		if (queue->map != nullptr) {
			queue->map->isReserved = false; // Remove reservation to free up for future queue
			queue->map = nullptr;
		}

		for (const auto &sd : queue->teama_members)
			sd->bg_queue_id = 0;

		for (const auto &sd : queue->teamb_members)
			sd->bg_queue_id = 0;

		queue->teama_members.clear();
		queue->teamb_members.clear();
		queue->teama_members.shrink_to_fit();
		queue->teamb_members.shrink_to_fit();
		queue->accepted_players = 0;
		queue->state = QUEUE_STATE_SETUP;
	}
}

/**
 * Sub function for leaving a Battleground queue
 * @param sd: Player leaving
 * @param members: List of players in queue data
 * @param apply_sc: Apply the SC_ENTRY_QUEUE_APPLY_DELAY status on queue leave (default: true)
 * @return True on success or false otherwise
 */
static bool bg_queue_leave_sub(map_session_data *sd, std::vector<map_session_data *> &members, bool apply_sc)
{
	if (!sd)
		return false;

	auto list_it = members.begin();

	while (list_it != members.end()) {
		if (*list_it == sd) {
			members.erase(list_it);

			if (apply_sc)
				sc_start(nullptr, &sd->bl, SC_ENTRY_QUEUE_APPLY_DELAY, 100, 1, 60000);
			sd->bg_queue_id = 0;
			pc_delete_bg_queue_timer(sd);
			return true;
		} else {
			list_it++;
		}
	}

	return false;
}

/**
 * Leave a Battleground queue
 * @param sd: Player data
 * @param apply_sc: Apply the SC_ENTRY_QUEUE_APPLY_DELAY status on queue leave (default: true)
 * @return True on success or false otherwise
 */
bool bg_queue_leave(map_session_data *sd, bool apply_sc)
{
	if (!sd || sd->bg_queue_id == 0)
		return false;

	pc_delete_bg_queue_timer(sd);

	for (auto &queue : bg_queues) {
		if (sd->bg_queue_id == queue->queue_id) {
			if (!bg_queue_leave_sub(sd, queue->teama_members, apply_sc) && !bg_queue_leave_sub(sd, queue->teamb_members, apply_sc)) {
				ShowError("bg_queue_leave: Couldn't find player %s in battlegrounds queue.\n", sd->status.name);
				return false;
			} else {
				if ((queue->state == QUEUE_STATE_SETUP || queue->state == QUEUE_STATE_SETUP_DELAY) && queue->teama_members.empty() && queue->teamb_members.empty()) // If there are no players left in the queue (that hasn't started), discard it
					bg_queue_clear(queue, true);

				return true;
			}
		}
	}

	return false;
}

/**
 * Send packets to all clients in queue to notify them that the battleground is ready to be joined
 * @param name: Battleground name
 * @param queue: Battleground queue
 * @return True on success or false otherwise
 */
bool bg_queue_on_ready(const char *name, std::shared_ptr<s_battleground_queue> queue)
{
	std::shared_ptr<s_battleground_type> bg = battleground_db.find(queue->id);

	if (!bg) {
		ShowError("bg_queue_on_ready: Couldn't find battleground ID %d in battlegrounds database.\n", queue->id);
		return false;
	}

	if (queue->teama_members.size() < queue->required_players || queue->teamb_members.size() < queue->required_players)
		return false; // Return players to the queue and stop reapplying the timer

	bool map_reserved = false;

	for (auto &map : bg->maps) {
		if (!map.isReserved) {
			map.isReserved = true;
			map_reserved = true;
			queue->map = &map;
			break;
		}
	}

	if (!map_reserved) { // All the battleground maps are reserved. Set a timer to check for an open battleground every 10 seconds.
		queue->tid_requeue = add_timer(gettick() + 10000, bg_on_ready_loopback, 0, (intptr_t)queue->queue_id);
		return false;
	}

	queue->state = QUEUE_STATE_SETUP_DELAY;
	queue->tid_expire = add_timer(gettick() + 20000, bg_on_ready_expire, 0, (intptr_t)queue->queue_id);

	for (const auto &sd : queue->teama_members)
		clif_bg_queue_lobby_notify(name, sd);

	for (const auto &sd : queue->teamb_members)
		clif_bg_queue_lobby_notify(name, sd);

	return true;
}

/**
 * Send a player into an active Battleground
 * @param sd: Player to send in
 * @param queue: Queue data
 */
void bg_join_active(map_session_data *sd, std::shared_ptr<s_battleground_queue> queue)
{
	if (sd == nullptr || queue == nullptr)
		return;

	// Check player's current position for mapflag check
	if (battle_config.bgqueue_nowarp_mapflag > 0 && map_getmapflag(sd->bl.m, MF_NOWARP)) {
		clif_messagecolor(&sd->bl, color_table[COLOR_LIGHT_GREEN], msg_txt(sd, 337), false, SELF); // You can't apply to a battleground queue from this map.
		bg_queue_leave(sd);
		clif_bg_queue_entry_init(sd);
		return;
	}

	pc_delete_bg_queue_timer(sd); // Cancel timer so player doesn't leave the queue.

	int bg_id_team_1 = static_cast<int>(mapreg_readreg(add_str(queue->map->team1.bg_id_var.c_str())));
	std::shared_ptr<s_battleground_data> bgteam_1 = util::umap_find(bg_team_db, bg_id_team_1);

	for (auto &pl_sd : queue->teama_members) {
		if (sd != pl_sd)
			continue;

		if (bgteam_1 == nullptr) {
			bg_queue_leave(sd);
			clif_bg_queue_apply_result(BG_APPLY_RECONNECT, battleground_db.find(queue->id)->name.c_str(), sd);
			clif_bg_queue_entry_init(sd);
			return;
		}

		clif_bg_queue_entry_init(pl_sd);
		bg_team_join(bg_id_team_1, pl_sd, true, true);
		npc_event(pl_sd, bgteam_1->active_event.c_str(), 0);

		// [RomuloSM]: Battleground Expansion
		if( battle_config.bg_clothes1color ) {
			pc_changelook(pl_sd, LOOK_CLOTHES_COLOR, battle_config.bg_clothes1color);
		}
		return;
	}

	int bg_id_team_2 = static_cast<int>(mapreg_readreg(add_str(queue->map->team2.bg_id_var.c_str())));
	std::shared_ptr<s_battleground_data> bgteam_2 = util::umap_find(bg_team_db, bg_id_team_2);

	for (auto &pl_sd : queue->teamb_members) {
		if (sd != pl_sd)
			continue;

		if (bgteam_2 == nullptr) {
			bg_queue_leave(sd);
			clif_bg_queue_apply_result(BG_APPLY_RECONNECT, battleground_db.find(queue->id)->name.c_str(), sd);
			clif_bg_queue_entry_init(sd);
			return;
		}

		clif_bg_queue_entry_init(pl_sd);
		bg_team_join(bg_id_team_2, pl_sd, true, true);
		npc_event(pl_sd, bgteam_2->active_event.c_str(), 0);

		// [RomuloSM]: Battleground Expansion
		if( battle_config.bg_clothes2color ) {
			pc_changelook(pl_sd, LOOK_CLOTHES_COLOR, battle_config.bg_clothes2color);
		}
		return;
	}

	return;
}

/**
 * Check to see if any players in the queue are on a map with MF_NOWARP and remove them from the queue
 * @param queue: Queue data
 * @return True if the player is on a map with MF_NOWARP or false otherwise
 */
bool bg_mapflag_check(std::shared_ptr<s_battleground_queue> queue) {
	if (queue == nullptr || battle_config.bgqueue_nowarp_mapflag == 0)
		return false;

	bool found = false;

	for (const auto &sd : queue->teama_members) {
		if (map_getmapflag(sd->bl.m, MF_NOWARP)) {
			clif_messagecolor(&sd->bl, color_table[COLOR_LIGHT_GREEN], msg_txt(sd, 337), false, SELF); // You can't apply to a battleground queue from this map.
			bg_queue_leave(sd);
			clif_bg_queue_entry_init(sd);
			found = true;
		}
	}

	for (const auto &sd : queue->teamb_members) {
		if (map_getmapflag(sd->bl.m, MF_NOWARP)) {
			clif_messagecolor(&sd->bl, color_table[COLOR_LIGHT_GREEN], msg_txt(sd, 337), false, SELF); // You can't apply to a battleground queue from this map.
			bg_queue_leave(sd);
			clif_bg_queue_entry_init(sd);
			found = true;
		}
	}

	if (found) {
		queue->state = QUEUE_STATE_SETUP; // Set back to queueing state
		queue->accepted_players = 0; // Reset acceptance count

		// Free map to avoid creating a reservation delay
		if (queue->map != nullptr) {
			queue->map->isReserved = false;
			queue->map = nullptr;
		}

		// Announce failure to remaining players
		for (const auto &sd : queue->teama_members)
			clif_messagecolor(&sd->bl, color_table[COLOR_LIGHT_GREEN], msg_txt(sd, 340), false, SELF); // Participants were unable to join. Delaying entry for more participants.

		for (const auto &sd : queue->teamb_members)
			clif_messagecolor(&sd->bl, color_table[COLOR_LIGHT_GREEN], msg_txt(sd, 340), false, SELF); // Participants were unable to join. Delaying entry for more participants.
	}

	return found;
}

/**
 * Update the Battleground queue when the player accepts the invite
 * @param queue: Battleground queue
 * @param sd: Player data
 */
void bg_queue_on_accept_invite(map_session_data *sd)
{
	nullpo_retv(sd);

	std::shared_ptr<s_battleground_queue> queue = bg_search_queue(sd->bg_queue_id);

	if (queue == nullptr) {
		ShowError("bg_queue_on_accept_invite: Couldn't find player %s in battlegrounds queue.\n", sd->status.name);
		return;
	}

	queue->accepted_players++;
	clif_bg_queue_ack_lobby(true, mapindex_id2name(queue->map->mapindex), mapindex_id2name(queue->map->mapindex), sd);

	if (queue->state == QUEUE_STATE_ACTIVE) // Battleground is already active
		bg_join_active(sd, queue);
	else if (queue->state == QUEUE_STATE_SETUP_DELAY) {
		if (queue->accepted_players == queue->required_players * 2) {
			if (queue->tid_expire != INVALID_TIMER) {
				delete_timer(queue->tid_expire, bg_on_ready_expire);
				queue->tid_expire = INVALID_TIMER;
			}

			// Check player's current position for mapflag check
			if (battle_config.bgqueue_nowarp_mapflag > 0 && bg_mapflag_check(queue))
				return;

			queue->tid_start = add_timer(gettick() + battleground_db.find(queue->id)->start_delay * 1000, bg_on_ready_start, 0, (intptr_t)queue->queue_id);
		}
	}
}

/**
 * Begin the Battleground from the given queue
 * @param queue: Battleground queue
 */
void bg_queue_start_battleground(std::shared_ptr<s_battleground_queue> queue)
{
	if (queue == nullptr)
		return;

	std::shared_ptr<s_battleground_type> bg = battleground_db.find(queue->id);

	if (!bg) {
		bg_queue_clear(queue, true);
		ShowError("bg_queue_start_battleground: Could not find battleground ID %d in battlegrounds database.\n", queue->id);
		return;
	}

	// Check player's current position for mapflag check
	if (battle_config.bgqueue_nowarp_mapflag > 0 && bg_mapflag_check(queue))
		return;

	uint16 map_idx = queue->map->mapindex;
	int bg_team_1 = bg_create(map_idx, &queue->map->team1);
	int bg_team_2 = bg_create(map_idx, &queue->map->team2);

	for (const auto &sd : queue->teama_members) {
		clif_bg_queue_entry_init(sd);
		bg_team_join(bg_team_1, sd, true, true);

		// [RomuloSM]: Battleground Expansion
		if( battle_config.bg_clothes1color ) {
			pc_changelook(sd, LOOK_CLOTHES_COLOR, battle_config.bg_clothes1color);
		}
	}

	for (const auto &sd : queue->teamb_members) {
		clif_bg_queue_entry_init(sd);
		bg_team_join(bg_team_2, sd, true, true);

		// [RomuloSM]: Battleground Expansion
		if( battle_config.bg_clothes2color ) {
			pc_changelook(sd, LOOK_CLOTHES_COLOR, battle_config.bg_clothes2color);
		}
	}

	mapreg_setreg(add_str(queue->map->team1.bg_id_var.c_str()), bg_team_1);
	mapreg_setreg(add_str(queue->map->team2.bg_id_var.c_str()), bg_team_2);
	npc_event_do(queue->map->bgcallscript.c_str());
	queue->state = QUEUE_STATE_ACTIVE;

	bg_queue_clear(queue, false);
}

/**
 * Initialize a Battleground queue
 * @param bg_id: Battleground ID
 * @param req_players: Required amount of players
 * @return s_battleground_queue*
 */
static void bg_queue_create(int bg_id, int req_players)
{
	auto queue = std::make_shared<s_battleground_queue>();

	queue->queue_id = bg_queue_count++;
	queue->id = bg_id;
	queue->required_players = req_players;
	queue->accepted_players = 0;
	queue->tid_expire = INVALID_TIMER;
	queue->tid_start = INVALID_TIMER;
	queue->tid_requeue = INVALID_TIMER;
	queue->state = QUEUE_STATE_SETUP;

	bg_queues.push_back(queue);
}

// [RomuloSM]: Battleground Expansion

/**
 * Search for a battleground based on id
 * @param name: Battleground name
 * @return s_battleground_type on success or nullptr on failure
 */
std::shared_ptr<s_battleground_type> bg_search_id(int id)
{
	for (const auto &entry : battleground_db) {
		auto bg = entry.second;

		if( bg->id == id )
			return bg;
	}

	return nullptr;
}

/**
 * [RomuloSM]: Battleground Rewards
 */
BattlegroundRewardDatabase battleground_reward_db;

const std::string BattlegroundRewardDatabase::getDefaultLocation() {
	return std::string(db_path) + "/" RSMPATH "/battleground_reward_db.yml";
}

/**
 * [RomuloSM]
 * Reads and parses an entry from the battleground_db
 * @param node: The YAML node containing the entry
 * @return count of successfully parsed rows
 */
uint64 BattlegroundRewardDatabase::parseBodyNode(const ryml::NodeRef& node) {
	uint32 id;

	if (!this->asUInt32(node, "Id", id))
		return 0;

	std::shared_ptr<s_battleground_reward> bgr = this->find(id);

	bool exists = bgr != nullptr;

	if (!exists) {
		if (!this->nodeExists(node, "Items") && !this->nodeExists(node, "Variables"))
			return 0;

		bgr = std::make_shared<s_battleground_reward>();
		bgr->id = id;
	}

	if (this->nodeExists(node, "Items")) {
		for (const auto& rewardsit : node["Items"]) {
			const auto rewards = rewardsit;
			s_battleground_e_reward_items reward_entry;

			std::string type_str;
			int type = 0;

			if (!this->nodesExist(rewards, { "Type", "Item" }))
				return 0;

			if (this->nodeExists(rewards, "Type")) {
				if (!this->asString(rewards, "Type", type_str))
					return 0;

				if (type_str.find("Winner") != std::string::npos)
					type = BGRW_WINNER;
				else if (type_str.find("Loser") != std::string::npos)
					type = BGRW_LOSER;
				else if (type_str.find("Draw") != std::string::npos)
					type = BGRW_DRAW;
				else {
					this->invalidWarning(rewardsit["Type"], "Invalid Items Reward Type '%s'.\n", type_str.c_str());
					return 0;
				}

				reward_entry.type = type;
			}

			if (this->nodeExists(rewards, "Item")) {
				std::string item_name;

				if (!this->asString(rewards, "Item", item_name))
					return 0;

				std::shared_ptr<item_data> i_data = item_db.search_aegisname( item_name.c_str() );

				if( i_data == nullptr ) {
					this->invalidWarning(rewardsit["Item"], "Invalid Item '%s'.\n", item_name.c_str());
					return 0;
				}

				reward_entry.nameid = i_data->nameid;
			}

			if (this->nodeExists(rewards, "MaxAmount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "MaxAmount", amount))
					return 0;

				if (amount < 0 || amount > MAX_AMOUNT) {
					this->invalidWarning(rewardsit["MaxAmount"], "Invalid Items Max Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.max_amount = amount;
			}
			else
				reward_entry.max_amount = 0;

			if (this->nodeExists(rewards, "Amount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "Amount", amount))
					return 0;

				if (amount <= 0) {
					this->invalidWarning(rewardsit["Amount"], "Invalid Items Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.amount = amount;
			}
			else
				reward_entry.amount = 0;

			if (this->nodeExists(rewards, "Rate")) {
				uint32 rate;

				if (!this->asUInt32(rewards, "Rate", rate))
					return 0;

				if (rate < 1 || rate > 100) {
					this->invalidWarning(rewardsit["Rate"], "Invalid Items Rate '%u'.\n", rate);
					return 0;
				}

				reward_entry.rate = rate;
			}
			else
				reward_entry.rate = 100;

			if (this->nodeExists(rewards, "HappyAmount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "HappyAmount", amount))
					return 0;

				if (amount <= 0) {
					this->invalidWarning(rewardsit["HappyAmount"], "Invalid Items Happy Hour Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.happy_amount = amount;
			}
			else
				reward_entry.happy_amount = 0;

			if (this->nodeExists(rewards, "HappyRate")) {
				uint32 rate;

				if (!this->asUInt32(rewards, "HappyRate", rate))
					return 0;

				if (rate < 1 || rate > 100) {
					this->invalidWarning(rewardsit["HappyRate"], "Invalid Items Happy Hour Rate '%u'.\n", rate);
					return 0;
				}

				reward_entry.happy_rate = rate;
			}
			else
				reward_entry.happy_rate = 100;

			if (this->nodeExists(rewards, "PremiumMaxAmount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "PremiumMaxAmount", amount))
					return 0;

				if (amount < 0 || amount > MAX_AMOUNT) {
					this->invalidWarning(rewardsit["PremiumMaxAmount"], "Invalid Items Premium Max Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.premium_max_amount = amount;
			}
			else
				reward_entry.premium_max_amount = 0;

			if (this->nodeExists(rewards, "PremiumAmount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "PremiumAmount", amount))
					return 0;

				if (amount <= 0) {
					this->invalidWarning(rewardsit["PremiumAmount"], "Invalid Items Premium Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.premium_amount = amount;
			}
			else
				reward_entry.premium_amount = 0;

			if (this->nodeExists(rewards, "PremiumRate")) {
				uint32 rate;

				if (!this->asUInt32(rewards, "PremiumRate", rate))
					return 0;

				if (rate < 1 || rate > 100) {
					this->invalidWarning(rewardsit["PremiumRate"], "Invalid Items Premium Rate '%u'.\n", rate);
					return 0;
				}

				reward_entry.premium_rate = rate;
			}
			else
				reward_entry.premium_rate = 100;

			if (this->nodeExists(rewards, "PremiumHappyAmount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "PremiumHappyAmount", amount))
					return 0;

				if (amount <= 0) {
					this->invalidWarning(rewardsit["PremiumHappyAmount"], "Invalid Items Premium Happy Hour Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.premium_happy_amount = amount;
			}
			else
				reward_entry.premium_happy_amount = 0;

			if (this->nodeExists(rewards, "PremiumHappyRate")) {
				uint32 rate;

				if (!this->asUInt32(rewards, "PremiumHappyRate", rate))
					return 0;

				if (rate < 1 || rate > 100) {
					this->invalidWarning(rewardsit["PremiumHappyRate"], "Invalid Items Premium Happy Hour Rate '%u'.\n", rate);
					return 0;
				}

				reward_entry.premium_happy_rate = rate;
			}
			else
				reward_entry.premium_happy_rate = 100;

			bgr->reward_items.push_back(reward_entry);
		}
	}

	if( this->nodeExists(node, "Variables") ) {
		for (const auto &rewardsit : node["Variables"]) {
			const auto &rewards = rewardsit;
			s_battleground_e_reward_vars reward_entry;

			std::string type_str;
			int type = 0;

			if (!this->nodesExist(rewards, { "Type", "Name", "Description", "Amount" }))
				return 0;

			if (this->nodeExists(rewards, "Type")) {
				if (!this->asString(rewards, "Type", type_str))
					return 0;

				if (type_str.find("Winner") != std::string::npos)
					type = BGRW_WINNER;
				else if (type_str.find("Loser") != std::string::npos)
					type = BGRW_LOSER;
				else if (type_str.find("Draw") != std::string::npos)
					type = BGRW_DRAW;
				else {
					this->invalidWarning(rewardsit["Type"], "Invalid Variables Reward Type '%s'.\n", type_str.c_str());
					return 0;
				}

				reward_entry.type = type;
			}

			if (this->nodeExists(rewards, "Name")) {
				if (!this->asString(rewards, "Name", reward_entry.name))
						return 0;
					
				reward_entry.name.resize(NAME_LENGTH);
			}

			if (this->nodeExists(rewards, "Description")) {
				if (!this->asString(rewards, "Description", reward_entry.desc))
						return 0;
					
				reward_entry.desc.resize(NAME_LENGTH);
			}

			if (this->nodeExists(rewards, "MaxAmount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "MaxAmount", amount))
					return 0;

				if (amount < 0 || amount > MAX_AMOUNT) {
					this->invalidWarning(rewardsit["MaxAmount"], "Invalid Items Max Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.max_amount = amount;
			}
			else
				reward_entry.max_amount = 0;

			if (this->nodeExists(rewards, "Amount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "Amount", amount))
					return 0;

				if( amount <= 0 ) {
					this->invalidWarning(rewardsit["Amount"], "Invalid Variables Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.amount = amount;
			}
			else
				reward_entry.amount = 0;

			if (this->nodeExists(rewards, "Rate")) {
				uint32 rate;

				if (!this->asUInt32(rewards, "Rate", rate))
					return 0;

				if (rate < 1 || rate > 100) {
					this->invalidWarning(rewardsit["Rate"], "Invalid Variables Rate '%u'.\n", rate);
					return 0;
				}

				reward_entry.rate = rate;
			}
			else
				reward_entry.rate = 0;

			if (this->nodeExists(rewards, "HappyAmount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "HappyAmount", amount))
					return 0;

				if (amount <= 0) {
					this->invalidWarning(rewardsit["HappyAmount"], "Invalid Items Happy Hour Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.happy_amount = amount;
			}
			else
				reward_entry.happy_amount = 0;

			if (this->nodeExists(rewards, "HappyRate")) {
				uint32 rate;

				if (!this->asUInt32(rewards, "HappyRate", rate))
					return 0;

				if (rate < 1 || rate > 100) {
					this->invalidWarning(rewardsit["HappyRate"], "Invalid Items Happy Hour Rate '%u'.\n", rate);
					return 0;
				}

				reward_entry.happy_rate = rate;
			}
			else
				reward_entry.happy_rate = 100;

			if (this->nodeExists(rewards, "PremiumMaxAmount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "PremiumMaxAmount", amount))
					return 0;

				if (amount < 0 || amount > MAX_AMOUNT) {
					this->invalidWarning(rewardsit["PremiumMaxAmount"], "Invalid Items Premium Max Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.premium_max_amount = amount;
			}
			else
				reward_entry.premium_max_amount = 0;

			if (this->nodeExists(rewards, "PremiumAmount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "PremiumAmount", amount))
					return 0;

				if (amount <= 0) {
					this->invalidWarning(rewardsit["PremiumAmount"], "Invalid Variables Premium Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.premium_amount = amount;
			}
			else
				reward_entry.premium_amount = 0;

			if (this->nodeExists(rewards, "PremiumRate")) {
				uint32 rate;

				if (!this->asUInt32(rewards, "PremiumRate", rate))
					return 0;

				if (rate < 1 || rate > 100) {
					this->invalidWarning(rewardsit["PremiumRate"], "Invalid Variables Premium Rate '%u'.\n", rate);
					return 0;
				}

				reward_entry.premium_rate = rate;
			}
			else
				reward_entry.premium_rate = 100;

			if (this->nodeExists(rewards, "PremiumHappyAmount")) {
				unsigned int amount;

				if (!this->asUInt32(rewards, "PremiumHappyAmount", amount))
					return 0;

				if (amount <= 0) {
					this->invalidWarning(rewardsit["PremiumHappyAmount"], "Invalid Items Premium Happy Hour Amount '%u'.\n", amount);
					return 0;
				}

				reward_entry.premium_happy_amount = amount;
			}
			else
				reward_entry.premium_happy_amount = 0;

			if (this->nodeExists(rewards, "PremiumHappyRate")) {
				uint32 rate;

				if (!this->asUInt32(rewards, "PremiumHappyRate", rate))
					return 0;

				if (rate < 1 || rate > 100) {
					this->invalidWarning(rewardsit["PremiumHappyRate"], "Invalid Items Premium Happy Hour Rate '%u'.\n", rate);
					return 0;
				}

				reward_entry.premium_happy_rate = rate;
			}
			else
				reward_entry.premium_happy_rate = 100;

			bgr->reward_vars.push_back(reward_entry);
		}
	}

	if( bgr->reward_items.size() <= 0 && bgr->reward_vars.size() <= 0 )
		return 0;

	if (!exists)
		this->put(id, bgr);

	return 1;
}

/**
 * Search for a battleground reward based on the given bg_id
 * @param id: Battleground bg_id
 * @return s_battleground_reward on success or nullptr on failure
 */
std::shared_ptr<s_battleground_reward> bg_reward_search(int id)
{
	for (const auto &entry : battleground_reward_db) {
		auto bgr = entry.second;

		if ( bgr->id == id )
			return bgr;
	}

	return nullptr;
}

/**
 * Executes 'func' for each bg member on the same map and in range (0:whole map)
 */
int bg_foreachsamemap(int (*func)(struct block_list*,va_list),map_session_data *sd,int range,...)
{
	int i;
	int x0,y0,x1,y1;
	struct block_list *list[MAX_PARTY];
	int blockcount=0;
	int total = 0; //Return value.

	nullpo_ret(sd);

	std::shared_ptr<s_battleground_data> bg = util::umap_find(bg_team_db, sd->bg_id);
	if( bg == nullptr )
		return 0;

	x0 = sd->bl.x-range;
	y0 = sd->bl.y-range;
	x1 = sd->bl.x+range;
	y1 = sd->bl.y+range;

	for (const auto &member : bg->members) {
		map_session_data *psd = member.sd;

		if(!psd)
			continue;

		if(psd->bl.m!=sd->bl.m || !psd->bl.prev)
			continue;

		if(range &&
			(psd->bl.x<x0 || psd->bl.y<y0 ||
			 psd->bl.x>x1 || psd->bl.y>y1 ) )
			continue;

		list[blockcount++]=&psd->bl;
	}

	map_freeblock_lock();

	for(i = 0; i < blockcount; i++) {
		va_list ap;
		va_start(ap, range);
		total += func(list[i], ap);
		va_end(ap);
	}

	map_freeblock_unlock();

	return total;
}

// [RomuloSM]: Battleground Expansion
// Compute damage in Battleground.
bool bg_score_damage(struct block_list *src, struct block_list *dst, int damage)
{
	struct block_list *s_bl, *t_bl;

	if( damage <= 0 || src == NULL || dst == NULL || !map_getmapflag(src->m,MF_BATTLEGROUND) )
		return 0;

	if( (s_bl = battle_get_master(src)) == NULL )
		s_bl = src;

	if( (t_bl = battle_get_master(dst)) == NULL )
		t_bl = dst;

	if( s_bl->type == BL_PC ) {
		map_session_data *sd = BL_CAST(BL_PC, s_bl);

		if( sd == NULL || !sd->bg_id )
			return 0;

		if( t_bl->type == BL_PC ) {
			map_session_data *tsd = BL_CAST(BL_PC, t_bl);

			if( tsd == NULL || !tsd->bg_id || tsd->bg_id == sd->bg_id )
				return 0;

			increment_limit(sd->bge.ranking.player.damage_given, (unsigned int)damage, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.player.damage_given, (unsigned int)damage, USHRT_MAX);
			increment_limit(tsd->bge.ranking.player.damage_taken, (unsigned int)damage, USHRT_MAX);
			increment_limit(tsd->status.bg_ranking.player.damage_taken, (unsigned int)damage, USHRT_MAX);
			return 1;
		}
		else if( t_bl->type == BL_MOB ) {
			struct mob_data *md = BL_CAST(BL_MOB, t_bl);

			if( md == NULL )
				return 0;

			switch(md->mob_id)
			{
				// Emperium
				case MOBID_EMPERIUM:
					increment_limit(sd->bge.ranking.emperium.damage, (unsigned int)damage, USHRT_MAX);
					increment_limit(sd->status.bg_ranking.emperium.damage, (unsigned int)damage, USHRT_MAX);
					break;
				// Rune Stones
				case MOBID_GUARDIAN_STONE1:
				case MOBID_GUARDIAN_STONE2:
					increment_limit(sd->bge.ranking.runestone.damage, (unsigned int)damage, USHRT_MAX);
					increment_limit(sd->status.bg_ranking.runestone.damage, (unsigned int)damage, USHRT_MAX);
					break;
				// Guardians
				case 1285:
				case 1286:
				case 1287:
				case 1949:
				case 1950:
					increment_limit(sd->bge.ranking.guardian.damage_given, (unsigned int)damage, USHRT_MAX);
					increment_limit(sd->status.bg_ranking.guardian.damage_given, (unsigned int)damage, USHRT_MAX);
					break;
				// Barrier
				case 1905:
				case 1906:
					increment_limit(sd->bge.ranking.barricade.damage, (unsigned int)damage, USHRT_MAX);
					increment_limit(sd->status.bg_ranking.barricade.damage, (unsigned int)damage, USHRT_MAX);
					break;
				// Objectives
				case 1909:
				case 1910:
					increment_limit(sd->bge.ranking.objective.damage, (unsigned int)damage, USHRT_MAX);
					increment_limit(sd->status.bg_ranking.objective.damage, (unsigned int)damage, USHRT_MAX);
					break;
				// Flags
				case 1911:
				case 1912:
				case 1913:
					increment_limit(sd->bge.ranking.flag.damage, (unsigned int)damage, USHRT_MAX);
					increment_limit(sd->status.bg_ranking.flag.damage, (unsigned int)damage, USHRT_MAX);
					break;
				// Crystals
				case 1914:
				case 1915:
					increment_limit(sd->bge.ranking.crystal.damage, (unsigned int)damage, USHRT_MAX);
					increment_limit(sd->status.bg_ranking.crystal.damage, (unsigned int)damage, USHRT_MAX);
					break;
				default:
					return 0;
			}
			return 1;
		}
	}
	else if( s_bl->type == BL_MOB ) {
		map_session_data *sd;
		struct mob_data *md = BL_CAST(BL_MOB, s_bl);

		if( md == NULL || t_bl->type != BL_PC || (sd = BL_CAST(BL_PC, t_bl)) == NULL )
			return 0;

		if( sd == NULL || !sd->bg_id )
			return 0;

		switch(md->mob_id)
		{
			// Emperium
			case MOBID_EMPERIUM:
				increment_limit(sd->bge.ranking.emperium.damage, (unsigned)damage, USHRT_MAX);
				increment_limit(sd->status.bg_ranking.emperium.damage, (unsigned)damage, USHRT_MAX);
				break;
			// Rune Stones
			case MOBID_GUARDIAN_STONE1:
			case MOBID_GUARDIAN_STONE2:
				increment_limit(sd->bge.ranking.runestone.damage, (unsigned)damage, USHRT_MAX);
				increment_limit(sd->status.bg_ranking.runestone.damage, (unsigned)damage, USHRT_MAX);
				break;
			// Guardians
			case 1285:
			case 1286:
			case 1287:
			case 1949:
			case 1950:
				increment_limit(sd->bge.ranking.guardian.damage_given, (unsigned)damage, USHRT_MAX);
				increment_limit(sd->status.bg_ranking.guardian.damage_given, (unsigned)damage, USHRT_MAX);
				break;
			// Barricades
			case 1905:
			case 1906:
				increment_limit(sd->bge.ranking.barricade.damage, (unsigned)damage, USHRT_MAX);
				increment_limit(sd->status.bg_ranking.barricade.damage, (unsigned)damage, USHRT_MAX);
				break;
			// Objectives
			case 1909:
			case 1910:
				increment_limit(sd->bge.ranking.objective.damage, (unsigned)damage, USHRT_MAX);
				increment_limit(sd->status.bg_ranking.objective.damage, (unsigned)damage, USHRT_MAX);
				break;
			// Flags
			case 1911:
			case 1912:
			case 1913:
				increment_limit(sd->bge.ranking.flag.damage, (unsigned)damage, USHRT_MAX);
				increment_limit(sd->status.bg_ranking.flag.damage, (unsigned)damage, USHRT_MAX);
				break;
			// Crystals
			case 1914:
			case 1915:
				increment_limit(sd->bge.ranking.crystal.damage, (unsigned)damage, USHRT_MAX);
				increment_limit(sd->status.bg_ranking.crystal.damage, (unsigned)damage, USHRT_MAX);
				break;
			default:
				break;
		}
		return 1;
	}
	return 0;
}

// Computes eliminations in the Battleground.
bool bg_score_kills(struct block_list *src, struct block_list *dst)
{
	struct block_list *s_bl;
	map_session_data *sd;

	if( !src || !dst || src == dst || !map_getmapflag(src->m,MF_BATTLEGROUND) )
		return 0;

	if( (s_bl = battle_get_master(src)) == NULL )
		s_bl = src;

	sd = BL_CAST(BL_PC, s_bl);
	switch( dst->type )
	{
		case BL_PC:
			{
				map_session_data *tsd = BL_CAST(BL_PC, dst);
				if( s_bl->type == BL_MOB )
				{
					if( tsd != NULL && tsd->bg_id ) {
						increment_limit(tsd->bge.ranking.guardian.deaths, 1, USHRT_MAX);
						increment_limit(tsd->status.bg_ranking.guardian.deaths, 1, USHRT_MAX);
					}
				}
				else {
					if( tsd != NULL && sd != NULL && tsd->bg_id && sd->bg_id && tsd->bg_id != sd->bg_id ) {
						increment_limit(tsd->bge.ranking.player.deaths, 1, USHRT_MAX);
						increment_limit(tsd->status.bg_ranking.player.deaths, 1, USHRT_MAX);
						increment_limit(sd->bge.ranking.player.kills, 1, USHRT_MAX);
						increment_limit(sd->status.bg_ranking.player.kills, 1, USHRT_MAX);
					}
				}
			}
			break;
		case BL_MOB:
			{
				struct mob_data *md = BL_CAST(BL_MOB, dst);

				if( md == NULL || sd == NULL || !sd->bg_id )
					return 0;
				
				switch(md->mob_id)
				{
					case MOBID_EMPERIUM:
						increment_limit(sd->bge.ranking.emperium.kills, 1, USHRT_MAX);
						increment_limit(sd->status.bg_ranking.emperium.kills, 1, USHRT_MAX);
						break;
					case MOBID_GUARDIAN_STONE1:
					case MOBID_GUARDIAN_STONE2:
						increment_limit(sd->bge.ranking.runestone.kills, 1, USHRT_MAX);
						increment_limit(sd->status.bg_ranking.runestone.kills, 1, USHRT_MAX);
						break;
					case 1285:
					case 1286:
					case 1287:
					case 1949:
					case 1950:
						increment_limit(sd->bge.ranking.guardian.kills, 1, USHRT_MAX);
						increment_limit(sd->status.bg_ranking.guardian.kills, 1, USHRT_MAX);
						break;
					case 1906:
						increment_limit(sd->bge.ranking.barricade.kills, 1, USHRT_MAX);
						increment_limit(sd->status.bg_ranking.barricade.kills, 1, USHRT_MAX);
						break;
					case 1909:
					case 1910:
						increment_limit(sd->bge.ranking.objective.kills, 1, USHRT_MAX);
						increment_limit(sd->status.bg_ranking.objective.kills, 1, USHRT_MAX);
						break;
					case 1911:
					case 1912:
					case 1913:
						increment_limit(sd->bge.ranking.flag.kills, 1, USHRT_MAX);
						increment_limit(sd->status.bg_ranking.flag.kills, 1, USHRT_MAX);
						break;
					case 1914:
					case 1915:
						increment_limit(sd->bge.ranking.crystal.kills, 1, USHRT_MAX);
						increment_limit(sd->status.bg_ranking.crystal.kills, 1, USHRT_MAX);
						break;
					default:
						return 0;
				}
			}
			break;
		default:
			return 0;
	}
	return 1;
}

// Compute cures in the Battleground.
bool bg_score_heal(struct block_list *bl, int hp, int sp)
{
	struct block_list *s_bl = battle_get_master(bl);
	map_session_data *sd;

	if( s_bl == NULL )
		s_bl = bl;

	sd = BL_CAST(BL_PC, s_bl);

	if( sd == NULL || !sd->bg_id || !map_getmapflag(sd->bl.m,MF_BATTLEGROUND) )
		return 0;

	if( hp > 0 ) {
		increment_limit(sd->bge.ranking.heal.hp, (unsigned int)hp, USHRT_MAX);
		increment_limit(sd->status.bg_ranking.heal.hp, (unsigned int)hp, USHRT_MAX);
	}

	if( sp > 0 ) {
		increment_limit(sd->bge.ranking.heal.sp, (unsigned int)sp, USHRT_MAX);
		increment_limit(sd->status.bg_ranking.heal.sp, (unsigned int)sp, USHRT_MAX);
	}
	return 1;
}

// Computes use of healing items in Battleground.
bool bg_score_item_heal(map_session_data *sd, int amount, int hp, int sp)
{
	if( sd == NULL || !sd->bg_id || !map_getmapflag(sd->bl.m,MF_BATTLEGROUND) )
		return 0;

	if( amount > 0 ) {
		increment_limit(sd->bge.ranking.item.heal, (unsigned int)amount, USHRT_MAX);
		increment_limit(sd->status.bg_ranking.item.heal, (unsigned int)amount, USHRT_MAX);
	}

	if( hp > 0 || sp > 0 )
		bg_score_heal(&sd->bl, hp, sp);

	return 1;
}

// Computes use of requirement items in Battleground.
// Items used in NPC's and Skills.
bool bg_score_del_item(map_session_data *sd, int item_id, int amount)
{
	if( item_id <= 0 || amount <= 0 )
		return 0;

	switch(item_id)
	{
		// Poision Bottle (Garrafa de Veneno)
		case 678:
			increment_limit(sd->bge.ranking.item.poison_bottle, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.poison_bottle, (unsigned int)amount, USHRT_MAX);
			break;
		// Yellow Gemstone (Gema Amarela)
		case 715:
			increment_limit(sd->bge.ranking.item.yellow_gemstone, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.yellow_gemstone, (unsigned int)amount, USHRT_MAX);
			break;
		// Red Gemstone (Gema Vermelha)
		case 716:
			increment_limit(sd->bge.ranking.item.red_gemstone, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.red_gemstone, (unsigned int)amount, USHRT_MAX);
			break;
		// Blue Gemstone (Gema Azul)
		case 717:
			increment_limit(sd->bge.ranking.item.blue_gemstone, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.blue_gemstone, (unsigned int)amount, USHRT_MAX);
			break;
		// Oridecon (Oridecon)
		case 984:
			increment_limit(sd->bge.ranking.item.oridecon, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.oridecon, (unsigned int)amount, USHRT_MAX);
			break;
		// Elunium (Elunium)
		case 985:
			increment_limit(sd->bge.ranking.item.elunium, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.elunium, (unsigned int)amount, USHRT_MAX);
			break;
		// Steel (Aço)
		case 999:
			increment_limit(sd->bge.ranking.item.steel, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.steel, (unsigned int)amount, USHRT_MAX);
			break;
		// Emveretarcon (Emveretarcon)
		case 1011:
			increment_limit(sd->bge.ranking.item.emveretarcon, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.emveretarcon, (unsigned int)amount, USHRT_MAX);
			break;
		// Wooden Block (Tronco)
		case 1019:
			increment_limit(sd->bge.ranking.item.wooden_block, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.wooden_block, (unsigned int)amount, USHRT_MAX);
			break;
		// Stone (Pedra)
		case 7049:
			increment_limit(sd->bge.ranking.item.stone, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.stone, (unsigned int)amount, USHRT_MAX);
			break;
		// Fire Bottle (Frasco de Fogo Grego)
		case 7135:
			increment_limit(sd->bge.ranking.item.fire_bottle, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.fire_bottle, (unsigned int)amount, USHRT_MAX);
			break;
		// Acid Bottle (Frasco de Ácido)
		case 7136:
			increment_limit(sd->bge.ranking.item.acid_bottle, (unsigned int)amount, USHRT_MAX);
			increment_limit(sd->status.bg_ranking.item.acid_bottle, (unsigned int)amount, USHRT_MAX);
			break;
		default:
			return 0;
	}
	return 1;
}

// Create Fake Guild Struct.
void bg_create_guild()
{
	int j;
	memset(&bg_guild, 0, sizeof(bg_guild));
	for( j=0; j <= 1; j++ )
	{
		FILE* fp = NULL;
		char path[256];
		int i, skill;

		bg_guild[j].emblem_id = 1;
		bg_guild[j].guild_id = SHRT_MAX-j;
		bg_guild[j].guild_lv = 50;
		bg_guild[j].max_member = MAX_BG_MEMBERS;

		for( i=0; i < MAX_GUILDSKILL; i++ )
		{
			skill = i + GD_SKILLBASE;
			bg_guild[j].skill[i].id = skill;
			switch( skill )
			{
				case GD_GLORYGUILD:
					bg_guild[j].skill[i].lv = 0;
					break;
				case GD_APPROVAL:
				case GD_KAFRACONTRACT:
				case GD_GUARDRESEARCH:
				case GD_BATTLEORDER:
				case GD_RESTORE:
				case GD_EMERGENCYCALL:
				case GD_DEVELOPMENT:
					bg_guild[j].skill[i].lv = 1;
					break;
				case GD_GUARDUP:
				case GD_REGENERATION:
					bg_guild[j].skill[i].lv = 3;
					break;
				case GD_LEADERSHIP:
				case GD_GLORYWOUNDS:
				case GD_SOULCOLD:
				case GD_HAWKEYES:
					bg_guild[j].skill[i].lv = 5;
					break;
				case GD_EXTENSION:
					bg_guild[j].skill[i].lv = 10;
					break;
			}
		}

		// Guild Data
		strncpy(bg_guild[j].name, msg_txt(NULL, (j ? 1671 : 1665)), NAME_LENGTH);
		strncpy(bg_guild[j].master, msg_txt(NULL, (j ? 1672 : 1666)), NAME_LENGTH);
		memcpy(bg_guild[j].mes1, msg_txt(NULL, (j ? 1673: 1667)), MAX_GUILDMES1);
		memcpy(bg_guild[j].mes2, msg_txt(NULL, (j ? 1674 : 1668)), MAX_GUILDMES2);
		strncpy(bg_guild[j].position[0].name, msg_txt(NULL, (j ? 1675 : 1669)), NAME_LENGTH);
		strncpy(bg_guild[j].position[1].name, msg_txt(NULL, (j ? 1676 : 1670)), NAME_LENGTH);
		bg_guild[j].emblem_len = 0;

		sprintf(path, "%s/%s/emblems/bg_%d.ebm", db_path, RSMPATH, (j+1));
		if( (fp = fopen(path, "rb")) != NULL )
		{
			fseek(fp, 0, SEEK_END);
			bg_guild[j].emblem_len = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			fread(bg_guild[j].emblem_data, 1, bg_guild[j].emblem_len, fp);
			fclose(fp);
			ShowStatus("Done reading '" CL_WHITE "%s" CL_RESET "' battleground emblem data file.\n", path);
		}
	}

	ShowStatus("Create '" CL_WHITE "%d" CL_RESET "' guild for Battleground.\n", j);
	ShowMessage(CL_WHITE"[Battleground]:" CL_RESET " Battleground Expansion (version: 2.0.00) successfully initialized.\n");
	ShowMessage(CL_WHITE"[Battleground]:" CL_RESET " by (c) RomuloSM, suport in " CL_GREEN "romulodevel@gmail.com" CL_RESET "\n");
}

// Notify Players
void bg_leave_notify(std::shared_ptr<s_battleground_data> bg, map_session_data *sd, enum bg_team_leave_type flag, bool changemaster = false)
{
	int master_change = false;
	map_session_data *pl_sd;
	char output[128];

	if( bg == nullptr )
		return;

	nullpo_retv(sd);

	for (const auto &member : bg->members) {
		pl_sd = member.sd;

		if( pl_sd == nullptr || sd == pl_sd )
			continue;

		if( changemaster && !master_change ) {
			bg->expansion.master_id = pl_sd->status.char_id;
			bg_player_data(pl_sd);
			master_change = true;
		}

		switch (flag) {
		case BGTL_QUIT:
			sprintf(output, msg_txt(pl_sd,1650), sd->status.name);
			break;
		case BGTL_LEFT:
			sprintf(output, msg_txt(pl_sd,1651), sd->status.name);
			break;
		case BGTL_AFK:
			sprintf(output, msg_txt(pl_sd,1652), sd->status.name);
			break;
		//case BGTL_KICK:
		//	sprintf(output, msg_txt(pl_sd,1653), sd->status.name, sd->bg_kick.mes);
		//	break;
		case BGTL_KICK_IDLE:
			sprintf(output, msg_txt(pl_sd,1664), sd->status.name);
			break;
		default:
			ShowError("Unknow Flag (%d) Battleground Leave!\n", flag);
			continue;
		}
		clif_messagecolor(&pl_sd->bl, BGE_COLOR, output, true, SELF);
	}
}

// Clean Guild Skills
void bg_clean_skill(std::shared_ptr<s_battleground_data> bg)
{
	if( bg != nullptr ) {
		memset(bg->expansion.skill_block_timer, 0, sizeof(bg->expansion.skill_block_timer));
	}
}

// Block Guild Skills
void bg_block_skill(map_session_data *sd, int time)
{
	std::shared_ptr<s_battleground_data> bgteam = nullptr;
	uint16 skill_id[] = { GD_BATTLEORDER, GD_REGENERATION, GD_RESTORE, GD_EMERGENCYCALL };
	int idx, i;

	nullpo_retv(sd);

	if( !sd->bg_id || (bgteam = util::umap_find(bg_team_db, sd->bg_id)) == nullptr )
		return;

	for (i = 0; i < 4; i++) {
		idx = skill_id[i] - GD_SKILLBASE;
		skill_blockpc_start(sd, skill_id[i], time);
		bgteam->expansion.skill_block_timer[idx] = gettick()+time;
	}
}

// Removes the Guild skill delays from Battleground.
void bg_block_skill_end(map_session_data *sd)
{
	uint16 skill_id[] = { GD_BATTLEORDER, GD_REGENERATION, GD_RESTORE, GD_EMERGENCYCALL };
	int i;

	for (i=0; i < 4; i++) {
		if( battle_config.display_status_timers )
			clif_skill_cooldown(sd, skill_id[i], 0);
	}
}

// Check Skill Times
bool bg_check_skills_time(std::shared_ptr<s_battleground_data> bg)
{
	if( bg == nullptr )
		return true;

	uint16 skill_id[] = { GD_BATTLEORDER, GD_REGENERATION, GD_RESTORE, GD_EMERGENCYCALL };
	t_tick time = gettick();
	int i = 0, idx;

	for( i=0; i < 4; i++ ) {
		idx = skill_id[i] - GD_SKILLBASE;
		if( bg->expansion.skill_block_timer[idx] > time )
			return false;
	}
	return true;
}

// Refresh Player Data
void bg_player_data(map_session_data *sd)
{
	std::shared_ptr<s_battleground_data> bgteam = nullptr;

	nullpo_retv(sd);

	if( battle_config.bg_expansion_enable && (bgteam = util::umap_find(bg_team_db, sd->bg_id)) && bgteam->expansion.g ) {
		clif_name_area(&sd->bl);
		clif_name_self(&sd->bl);
		clif_bg_basicinfo( sd );
		clif_guild_belonginfo( *sd );
		clif_guild_basicinfo( *sd );
		clif_guild_skillinfo(sd);
		clif_guild_memberlist( *sd );
		clif_fake_guild_emblem(sd, bgteam->expansion.g);
		clif_guild_emblem_area(&sd->bl);
#if PACKETVER >= 20181002
		map_foreachinallrange(clif_insight, &sd->bl, AREA_SIZE, BL_PC, &sd->bl); // bugfix!
#endif
	}
}

// Reload Battleground Rewards
void bg_reward_reload(void)
{
	battleground_reward_db.reload();
}

// Start BG Happy Hour
bool bg_happyhour_start(void)
{
	if( bg_happyhour_flag )
		return false;

	bg_happyhour_flag = true;

	npc_event_runall("OnBGHappyHourStart");
	return true;
}

// End BG Happy Hour
bool bg_happyhour_end(void)
{
	if( !bg_happyhour_flag )
		return false;

	bg_happyhour_flag = false;

	npc_event_runall("OnBGHappyHourEnd");
	return true;
}

bool bg_create_channel(int bg_id, int opt, int msg_delay, int color)
{
	struct Channel tmp_chan, *ch = NULL;
	char chname[CHAN_NAME_LENGTH];
	const char *chalias;

	std::shared_ptr<s_battleground_data> bg = util::umap_find(bg_team_db, bg_id);
	if( !bg )
		return false;

	// Create names
	sprintf(chname, "#bg_%s_%d", ( bg->expansion.teamId ? "croix" : "guill" ), bg_id);
	chalias = bg->expansion.teamId ? "Croix Team" : "Guillaume Team";

	if( (ch = channel_searchname((char *)chname)) != NULL ) { 
		if( channel_delete(ch,false) != 0 )
			return false;
	}

	memset(&tmp_chan, 0, sizeof(struct Channel));

	tmp_chan.type = CHAN_TYPE_BG;
	tmp_chan.bg_id = bg_id;
	tmp_chan.char_id = 0;
	safestrncpy(tmp_chan.name, chname+1, sizeof(tmp_chan.name));
	safestrncpy(tmp_chan.alias, chalias, sizeof(tmp_chan.alias));
	tmp_chan.opt = opt ? opt : CHAN_OPT_BASE;
	tmp_chan.msg_delay = msg_delay ? channel_config.private_channel.delay : msg_delay;
	tmp_chan.color = color ? color : channel_config.private_channel.color;

	if (!(ch = channel_create(&tmp_chan)))
		return false;

	bg->expansion.channel_name = chname;
	return true;
}

TIMER_FUNC(bg_countdown)
{
	int bg_id = (int)data;
	std::shared_ptr<s_battleground_data> bg = util::umap_find(bg_team_db, bg_id);
	if( !bg )
		return 0;

	bg->expansion.cd_sec--;
	for (const auto &member : bg->members) {
		if (member.sd != nullptr) {
			if( bg->expansion.cd_sec < 0 )
				bg->expansion.cd_sec = 0;

			if( member.sd->bge.countdown_hidden == false ) {
				if( bg->expansion.cd_sec > 0 )
					clif_showdigit(member.sd, (unsigned char)2, -abs(bg->expansion.cd_sec));
				else
					clif_showdigit(member.sd, (unsigned char)3, 0);
			}
		}
	}

	if( bg->expansion.cd_sec <= 0 ) {
		if (bg->expansion.cd_tid != INVALID_TIMER)
			delete_timer(bg->expansion.cd_tid, bg_countdown);
		bg->expansion.cd_tid = INVALID_TIMER;
	}
	else {
		bg->expansion.cd_tid = add_timer(gettick() + 1000, bg_countdown, 0, (intptr_t)bg_id);
	}

	return 0;
}

int bg_check_dual(map_session_data *sd)
{
	if (!battle_config.bg_check_dual)
		return false;

	nullpo_retr(false, sd);

	map_session_data* pl_sd;
	struct s_mapiterator* iter;
	int count = 0;

	iter = mapit_getallusers();
	for (pl_sd = (TBL_PC*)mapit_first(iter); mapit_exists(iter); pl_sd = (TBL_PC*)mapit_next(iter)) {
		if( !pl_sd->bg_id && !pl_sd->bg_queue_id )
			continue;
		if( battle_config.bg_check_dual&0x01 && session[sd->fd]->client_addr == session[pl_sd->fd]->client_addr )
			count++;
#ifdef GEPARD_UNIQUEID
		else if( battle_config.bg_check_dual&0x02 && session[sd->fd]->gepard_info.unique_id == session[pl_sd->fd]->gepard_info.unique_id )
			count++;
#endif
	}
	mapit_free(iter);

	return count;
}

bool bg_ranking_save(map_session_data *sd, int bg_id, bool clear)
{
	nullpo_retr(false, sd);

	if( bg_id <= 0 )
		return false;

	char* data = NULL;
	int matches;

	if (SQL_ERROR == Sql_Query(mmysql_handle, "SELECT COUNT(*) FROM `bg_ranking` WHERE `char_id`='%d' AND `bg_id`='%d'", sd->status.char_id, bg_id)) {
		Sql_ShowDebug(mmysql_handle);
		return false;
	}

	if( SQL_SUCCESS != Sql_NextRow(mmysql_handle) ) {
		Sql_FreeResult(mmysql_handle);
		return false;
	}

	Sql_GetData(mmysql_handle, 0, &data, NULL);
	matches = atoi(data);
	Sql_FreeResult(mmysql_handle);

	if( !matches) {
		if (SQL_ERROR == Sql_Query(mmysql_handle, "INSERT INTO `bg_ranking` (`char_id`, `bg_id`) VALUES (%d, %d)", sd->status.char_id, bg_id)) {
			Sql_ShowDebug(mmysql_handle);
			return false;
		}
	}

	if (SQL_ERROR == Sql_Query(mmysql_handle, "UPDATE `bg_ranking` SET `wins`=`wins`+'%d', `loss`=`loss`+'%d', `tied`=`tied`+'%d', `score_wins`=`score_wins`+'%d', `score_loss`=`score_loss`+'%d', `kills`=`kills`+'%d', `deaths`=`deaths`+'%d', "
		"`damage_given`=`damage_given`+'%d', `damage_taken`=`damage_taken`+'%d', `runestone_kills`=`runestone_kills`+'%d', `runestone_damage`=`runestone_damage`+'%d', `runestone_repair`=`runestone_repair`+'%d', `emperium_kills`=`emperium_kills`+'%d', "
		"`emperium_damage`=`emperium_damage`+'%d', `barrier_kills`=`barrier_kills`+'%d', `barrier_damage`=`barrier_damage`+'%d', `barrier_repair`=`barrier_repair`+'%d', `objective_kills`=`objective_kills`+'%d', `objective_damage`=`objective_damage`+'%d', "
		"`cristal_kills`=`cristal_kills`+'%d', `cristal_damage`=`cristal_damage`+'%d', `flag_kills`=`flag_kills`+'%d', `flag_damage`=`flag_damage`+'%d', `flag_capture`=`flag_capture`+'%d', `flag_recapture`=`flag_recapture`+'%d', `guard_kills`=`guard_kills`+'%d', "
		"`guard_deaths`=`guard_deaths`+'%d', `guard_damage_given`=`guard_damage_given`+'%d', `guard_damage_taken`=`guard_damage_taken`+'%d', `skill_support_success`=`skill_support_success`+'%d', `skill_support_fail`=`skill_support_fail`+'%d', `heal_hp`=`heal_hp`+'%d', "
		"`skill_success`=`skill_success`+'%d', `skill_fail`=`skill_fail`+'%d', `skill_damage_given`=`skill_damage_given`+'%d',  `skill_damage_taken`=`skill_damage_taken`+'%d', `heal_sp`=`heal_sp`+'%d', `item_heal_hp`=`item_heal_hp`+'%d', `item_heal_sp`=`item_heal_sp`+'%d', "
		"`item_heal`=`item_heal`+'%d', `oridecon`=`oridecon`+'%d', `elunium`=`elunium`+'%d', `steel`=`steel`+'%d', `emveretarcon`=`emveretarcon`+'%d', `wooden_block`=`wooden_block`+'%d', `stones`=`stones`+'%d', `yellow_gemstones`=`yellow_gemstones`+'%d', "
		"`red_gemstones`=`red_gemstones`+'%d', `blue_gemstones`=`blue_gemstones`+'%d', `ammos`=`ammos`+'%d', `poison_bottle`=`poison_bottle`+'%d', `fire_bottle`=`fire_bottle`+'%d', `acid_bottle`=`acid_bottle`+'%d' WHERE `char_id`='%d' AND `bg_id`='%d'",
		sd->bge.ranking.battle.wins, sd->bge.ranking.battle.loss, sd->bge.ranking.battle.tied, sd->bge.ranking.battle.score_wins, sd->bge.ranking.battle.score_loss,
		sd->bge.ranking.player.kills, sd->bge.ranking.player.deaths, sd->bge.ranking.player.damage_given, sd->bge.ranking.player.damage_taken, sd->bge.ranking.runestone.kills,
		sd->bge.ranking.runestone.damage, sd->bge.ranking.runestone.repair, sd->bge.ranking.emperium.kills, sd->bge.ranking.emperium.damage, sd->bge.ranking.barricade.kills,
		sd->bge.ranking.barricade.damage, sd->bge.ranking.barricade.repair, sd->bge.ranking.objective.kills, sd->bge.ranking.objective.damage, sd->bge.ranking.crystal.kills,
		sd->bge.ranking.crystal.damage, sd->bge.ranking.flag.kills, sd->bge.ranking.flag.damage, sd->bge.ranking.flag.capture, sd->bge.ranking.flag.recapture, sd->bge.ranking.guardian.kills,
		sd->bge.ranking.guardian.deaths, sd->bge.ranking.guardian.damage_given, sd->bge.ranking.guardian.damage_taken, sd->bge.ranking.skill.support_success, sd->bge.ranking.skill.support_fail,
		sd->bge.ranking.skill.success, sd->bge.ranking.skill.fail, sd->bge.ranking.skill.damage_given, sd->bge.ranking.skill.damage_taken, sd->bge.ranking.heal.hp, sd->bge.ranking.heal.sp,
		sd->bge.ranking.heal.item_hp, sd->bge.ranking.heal.item_sp, sd->bge.ranking.item.heal, sd->bge.ranking.item.oridecon, sd->bge.ranking.item.elunium, sd->bge.ranking.item.steel,
		sd->bge.ranking.item.emveretarcon, sd->bge.ranking.item.wooden_block, sd->bge.ranking.item.stone, sd->bge.ranking.item.yellow_gemstone, sd->bge.ranking.item.red_gemstone, sd->bge.ranking.item.blue_gemstone,
		sd->bge.ranking.item.ammos, sd->bge.ranking.item.poison_bottle, sd->bge.ranking.item.fire_bottle, sd->bge.ranking.item.acid_bottle, sd->status.char_id, bg_id
	)) {
		Sql_ShowDebug(mmysql_handle);
		return false;
	}

	if( clear )
		memset(&sd->bge.ranking, 0, sizeof(sd->bge.ranking));

	return true;
}

bool bg_player_info(map_session_data *sd, map_session_data *tsd)
{
	nullpo_retr(false, sd);

	if( tsd == NULL ) {
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,3), true, SELF);
		return false;
	}

	char output[CHAT_SIZE_MAX];
	int pos = 0, n = 0;
	char* data = NULL;

	memset(output, '\0', sizeof(output));

	if (SQL_ERROR == Sql_Query(mmysql_handle, "SELECT COUNT(*)+1 FROM `bg_ranking` WHERE CAST(`wins`-`loss` AS unsigned int)>(SELECT CAST(`wins`-`loss` AS unsigned int) FROM `bg_ranking` WHERE `char_id`='%d' AND `bg_id`='%d') AND `bg_id`='%d'", tsd->status.char_id, 0, 0 )) {
		Sql_ShowDebug(mmysql_handle);
		return false;
	}

	if( SQL_SUCCESS == Sql_NextRow(mmysql_handle) ) {
		Sql_GetData(mmysql_handle, 0, &data, NULL);
		pos = atoi(data);
	}

	Sql_FreeResult(mmysql_handle);

	if( pos ) {
		if (SQL_ERROR == Sql_Query(mmysql_handle, "SELECT COUNT(*) FROM `bg_ranking` WHERE CAST(`wins`-`loss` AS unsigned int)=(SELECT CAST(`wins`-`loss` AS unsigned int) FROM `bg_ranking` WHERE `char_id`='%d' AND `bg_id`='%d') AND `char_id`!='%d' AND `bg_id`='%d'", tsd->status.char_id, 0, tsd->status.char_id, 0 )) {
			Sql_ShowDebug(mmysql_handle);
			return false;
		}

		if( SQL_SUCCESS == Sql_NextRow(mmysql_handle) ) {
			Sql_GetData(mmysql_handle, 0, &data, NULL);
			n = atoi(data)-1;
		}

		Sql_FreeResult(mmysql_handle);
	}

	sprintf(output, "-- %s --", msg_txt(sd,1878));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	if( pos ) {
		if( n > 0 ) {
			sprintf(output, msg_txt(sd, 1880), pos, n);
		}
		else {
			sprintf(output, msg_txt(sd, 1879), pos);
		}

		clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);
	}

	sprintf(output, "%s: %d / %s: %d / %s: %d / %s: %d", msg_txt(sd, 1881), tsd->status.bg_ranking.battle.wins, msg_txt(sd, 1882), tsd->status.bg_ranking.battle.loss, msg_txt(sd, 1883), tsd->status.bg_ranking.battle.tied, msg_txt(sd, 1884), (tsd->status.bg_ranking.battle.wins - tsd->status.bg_ranking.battle.loss));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d / %s: %d\n", msg_txt(sd, 1885), tsd->status.bg_ranking.battle.score_wins, msg_txt(sd, 1886), tsd->status.bg_ranking.battle.score_loss, msg_txt(sd, 1887), (tsd->status.bg_ranking.battle.score_wins - tsd->status.bg_ranking.battle.score_loss));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output,"-- %s -- ", msg_txt(sd, 1888));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d / %s: %d", msg_txt(sd, 1889), tsd->status.bg_ranking.player.kills, msg_txt(sd, 1890), tsd->status.bg_ranking.player.deaths, msg_txt(sd, 1884), (tsd->status.bg_ranking.player.kills - tsd->status.bg_ranking.player.deaths));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);
	
	sprintf(output, "%s: %d / %s: %d / %s: %d", msg_txt(sd, 1891), tsd->status.bg_ranking.player.damage_given, msg_txt(sd, 1892), tsd->status.bg_ranking.player.damage_taken, msg_txt(sd, 1884), (tsd->status.bg_ranking.player.damage_given - tsd->status.bg_ranking.player.damage_taken));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d\n", msg_txt(sd, 1896), tsd->status.bg_ranking.heal.hp, msg_txt(sd, 1897), tsd->status.bg_ranking.heal.sp);
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "-- %s -- ", msg_txt(sd, 1893));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d / %s: %d", msg_txt(sd, 1894), tsd->status.bg_ranking.skill.support_success, msg_txt(sd, 1895), tsd->status.bg_ranking.skill.support_fail, msg_txt(sd, 1884), (tsd->status.bg_ranking.skill.support_success - tsd->status.bg_ranking.skill.support_fail));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d / %s: %d", msg_txt(sd, 1912), tsd->status.bg_ranking.skill.success, msg_txt(sd, 1913), tsd->status.bg_ranking.skill.fail, msg_txt(sd, 1884), (tsd->status.bg_ranking.skill.success - tsd->status.bg_ranking.skill.fail));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d / %s: %d\n", msg_txt(sd, 1891), tsd->status.bg_ranking.skill.damage_given, msg_txt(sd, 1892), tsd->status.bg_ranking.skill.damage_taken, msg_txt(sd, 1893), (tsd->status.bg_ranking.skill.damage_given - tsd->status.bg_ranking.skill.damage_taken));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "-- %s -- ", msg_txt(sd, 1900));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d / %s: %d", msg_txt(sd, 1889), tsd->status.bg_ranking.guardian.kills, msg_txt(sd, 1890), tsd->status.bg_ranking.guardian.deaths, msg_txt(sd, 1884), (tsd->status.bg_ranking.guardian.kills - tsd->status.bg_ranking.guardian.deaths));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d / %s: %d\n", msg_txt(sd, 1891), tsd->status.bg_ranking.guardian.damage_given, msg_txt(sd, 1892), tsd->status.bg_ranking.guardian.damage_taken, msg_txt(sd, 1884), (tsd->status.bg_ranking.guardian.damage_given - tsd->status.bg_ranking.guardian.damage_taken));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "-- %s -- ", msg_txt(sd, 1901));
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d", msg_txt(sd, 1902), tsd->status.bg_ranking.crystal.kills, msg_txt(sd, 1903), tsd->status.bg_ranking.crystal.damage);
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d", msg_txt(sd, 1904), tsd->status.bg_ranking.objective.kills, msg_txt(sd, 1903), tsd->status.bg_ranking.objective.damage);
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d", msg_txt(sd, 1905), tsd->status.bg_ranking.flag.kills, msg_txt(sd, 1903), tsd->status.bg_ranking.flag.damage);
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d", msg_txt(sd, 1906), tsd->status.bg_ranking.flag.capture, msg_txt(sd, 1907), tsd->status.bg_ranking.flag.recapture);
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d", msg_txt(sd, 1908), tsd->status.bg_ranking.emperium.kills, msg_txt(sd, 1903), tsd->status.bg_ranking.emperium.damage);
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d / %s: %d", msg_txt(sd, 1909), tsd->status.bg_ranking.barricade.kills, msg_txt(sd, 1903), tsd->status.bg_ranking.barricade.damage, msg_txt(sd, 1910), tsd->status.bg_ranking.barricade.repair);
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	sprintf(output, "%s: %d / %s: %d / %s: %d\n", msg_txt(sd, 1911), tsd->status.bg_ranking.runestone.kills, msg_txt(sd, 1903), tsd->status.bg_ranking.runestone.damage, msg_txt(sd, 1910), tsd->status.bg_ranking.runestone.repair);
	clif_messagecolor(&sd->bl, color_table[COLOR_WHITE], output, true, SELF);

	return true;
}

/**
 * Initialize the Battleground data
 */
void do_init_battleground(void)
{
	// [RomuloSM]: Fake Guild
	bg_create_guild();

	if (battle_config.feature_bgqueue) {
		battleground_db.load();

		for (const auto &bg : battleground_db)
			bg_queue_create(bg.first, bg.second->required_players);
	}

	// [RomuloSM]: Battleground Database
	battleground_reward_db.load();

	add_timer_func_list(bg_send_xy_timer, "bg_send_xy_timer");
	add_timer_func_list(bg_on_ready_loopback, "bg_on_ready_loopback");
	add_timer_func_list(bg_on_ready_expire, "bg_on_ready_expire");
	add_timer_func_list(bg_on_ready_start, "bg_on_ready_start");
	add_timer_func_list(bg_countdown, "bg_countdown");
	add_timer_interval(gettick() + battle_config.bg_update_interval, bg_send_xy_timer, 0, 0, battle_config.bg_update_interval);
}

/**
 * Clear the Battleground data from memory
 */
void do_final_battleground(void)
{
	memset(&bg_guild,0,sizeof(bg_guild));
}
