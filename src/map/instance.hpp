// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <common/cbasetypes.hpp>
#include <common/database.hpp>
#include <common/mmo.hpp>

#include "script.hpp"
#include "status.hpp"

enum send_target : uint8;
struct block_list;

extern int16 instance_start;
extern int instance_count;

#define INSTANCE_NAME_LENGTH (60+1)

enum e_instance_state : uint8 {
	INSTANCE_IDLE,
	INSTANCE_BUSY
};

enum e_instance_mode : uint8 {
	IM_NONE,
	IM_CHAR,
	IM_PARTY,
	IM_GUILD,
	IM_CLAN,
	IM_MAX,
};

enum e_instance_enter : uint8 {
	IE_OK,
	IE_NOMEMBER,
	IE_NOINSTANCE,
	IE_OTHER
};

enum e_instance_notify : uint8 {
	IN_NOTIFY = 0,
	IN_DESTROY_LIVE_TIMEOUT,
	IN_DESTROY_ENTER_TIMEOUT,
	IN_DESTROY_USER_REQUEST,
	IN_CREATE_FAIL,
};

enum e_instance_difficulty : uint8 {
	ID_EASY = 1,
	ID_NORMAL,
	ID_HARD,
	ID_INSANE,
	ID_MAX,
};

struct s_instance_map {
	int16 m, src_m;
};

/// Instance data
struct s_instance_data {
	int id; ///< Instance DB ID
	e_instance_state state; ///< State of instance
	e_instance_mode mode; ///< Mode of instance
	e_instance_difficulty difficulty; ///< Mode of instance
	int owner_id; ///< Owner ID of instance
	int64 keep_limit; ///< Life time of instance
	int keep_timer; ///< Life time ID
	int64 idle_limit; ///< Idle time of instance
	int idle_timer; ///< Idle timer ID
	bool nonpc;
	bool nomapflag;
	struct reg_db regs; ///< Instance variables for scripts
	std::vector<s_instance_map> map; ///< Array of maps in instance
	std::unordered_map<enum sc_type, int> sc_penalties;

	s_instance_data() :
		id(0),
		state(INSTANCE_IDLE),
		mode(IM_PARTY),
		owner_id(0),
		keep_limit(0),
		keep_timer(INVALID_TIMER),
		idle_limit(0),
		idle_timer(INVALID_TIMER),
		nonpc(false),
		nomapflag(false),
		regs(),
		map() { }
};

/// Instance DB entry
struct s_instance_db {
	int id; ///< Instance DB ID
	std::string name; ///< Instance name
	int64 limit, ///< Duration limit
		timeout; ///< Timeout limit
	bool nonpc;
	bool nomapflag;
	bool destroyable; ///< Destroyable flag
	e_instance_difficulty difficulty;
	bool infinite_limit; ///< Infinite limit flag
	bool infinite_timeout; ///< Infinite timeout limit flag
	struct point enter; ///< Instance entry point
	std::vector<int16> maplist; ///< Maps in instance
};

class InstanceDatabase : public TypesafeYamlDatabase<int32, s_instance_db> {
public:
	InstanceDatabase() : TypesafeYamlDatabase("INSTANCE_DB", 2, 1) {

	}

	const std::string getDefaultLocation() override;
	uint64 parseBodyNode(const ryml::NodeRef& node) override;
};

extern InstanceDatabase instance_db;

extern std::unordered_map<int, std::shared_ptr<s_instance_data>> instances;

/// Instance Difficulty DB entry [Easycore]
struct s_instance_mode_db {
	e_instance_difficulty id; ///< Instance Mode DB ID
	int exp_rate;
	int drop_rate;
	int hp;
	int speed;
	int str;
	int agi;
	int vit;
	int int_;
	int dex;
	int luk;
	int atk;
	int atk2;
	int matk_min;
	int matk_max;
	int def;
	int mdef;
	int hit;
	int flee;
	int flee2;
	int cri;
	int amotion;
	int adelay;
	int dmotion;
};

class InstanceModeDatabase : public TypesafeYamlDatabase<int32, s_instance_mode_db> {
public:
	InstanceModeDatabase() : TypesafeYamlDatabase("INSTANCE_MODE_DB", 1) {

	}

	const std::string getDefaultLocation();
	uint64 parseBodyNode(const ryml::NodeRef& node);
};

extern InstanceModeDatabase instance_mode_db;

extern std::unordered_map<int, std::shared_ptr<s_instance_mode_db>> instance_mode;

std::shared_ptr<s_instance_db> instance_search_db_name(const char* name);
void instance_getsd(int instance_id, map_session_data *&sd, enum send_target *target);

int instance_create(int owner_id, const char *name, e_instance_mode mode, e_instance_difficulty difficulty);
bool instance_destroy(int instance_id);
void instance_destroy_command(map_session_data *sd);
e_instance_enter instance_enter(map_session_data *sd, int instance_id, const char *name, short x, short y);
bool instance_reqinfo(map_session_data *sd, int instance_id);
bool instance_addusers(int instance_id);
bool instance_delusers(int instance_id);
void instance_generate_mapname(int map_id, int instance_id, char outname[MAP_NAME_LENGTH]);
int16 instance_mapid(int16 m, int instance_id);
int instance_addmap(int instance_id);

void instance_addnpc(std::shared_ptr<s_instance_data> idata);

std::shared_ptr<s_instance_mode_db> instance_mode_search(e_instance_difficulty mode);
void instance_setpenalty(class map_session_data *sd);

void do_reload_instance(void);
void do_init_instance(void);
void do_final_instance(void);

#endif /* INSTANCE_HPP */
