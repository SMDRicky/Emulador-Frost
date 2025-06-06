// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef BATTLEGROUND_HPP
#define BATTLEGROUND_HPP

#include <memory>
#include <string>
#include <vector>

#include <common/cbasetypes.hpp>
#include <common/database.hpp>
#include <common/mmo.hpp> // struct party

#define MAX_BG_MEMBERS 30

// [RomuloSM]: Battleground Expansion
#define BG_CLOTHERSCOLOR_VAR "BGE_CLOTHERSCOLOR"
#define BG_LISTEN_VAR "BGE_LISTEN"

extern struct guild bg_guild[2];
extern bool bg_happyhour_flag;

enum bg_team_leave_type {
	BGTL_LEFT = 0x0,
	BGTL_QUIT = 0x1,
	BGTL_AFK  = 0x2,
	BGTL_KICK = 0x3,
	BGTL_KICK_IDLE = 0x4,
};

enum bg_reward_type {
	BGRW_ALL = -1,
	BGRW_WINNER,
	BGRW_LOSER,
	BGRW_DRAW
};

enum bg_ranking_type {
	BGR_WINS = 0,
	BGR_LOSS,
	BGR_TIED,
	BGR_SCORE_WINS,
	BGR_SCORE_LOSS,
	BGR_SKILLSUP_SUCCESS,
	BGR_SKILLSUP_FAIL,
	BGR_SKILLUSE_SUCCESS,
	BGR_SKILLUSE_FAIL,
	BGR_SKILLDMG_GIVEN,
	BGR_SKILLDMG_TAKEN,
	BGR_HEAL_HP,
	BGR_HEAL_SP,
	BGR_ITEMHEAL_HP,
	BGR_ITEMHEAL_SP,
	BGR_ITEMHEAL,
	BGR_AMMO,
	BGR_POISON_BOTTLE,
	BGR_FIRE_BOTTLE,
	BGR_ACID_BOTTLE,
	BGR_ORIDECON,
	BGR_ELUNIUM,
	BGR_STEEL,
	BGR_EMVERETARCON,
	BGR_WOODEN_BLOCK,
	BGR_STONE,
	BGR_YELLOW_GEMSTONE,
	BGR_RED_GEMSTONE,
	BGR_BLUE_GEMSTONE,
	BGR_PLAYER_KILL,
	BGR_PLAYER_DEATH,
	BGR_PLAYER_DMG_GIVEN,
	BGR_PLAYER_DMG_TAKEN,
	BGR_RUNESTONE_KILL,
	BGR_RUNESTONE_DMG,
	BGR_RUNESTONE_REPAIR,
	BGR_EMPERIUM_KILL,
	BGR_EMPERIUM_DMG,
	BGR_BARRICADE_KILL,
	BGR_BARRICADE_DMG,
	BGR_BARRICADE_REPAIR,
	BGR_OBJECTIVE_KILL,
	BGR_OBJECTIVE_DMG,
	BGR_FLAG_KILL,
	BGR_FLAG_DMG,
	BGR_FLAG_CAPTURE,
	BGR_FLAG_RECAPTURE,
	BGR_CRYSTAL_KILL,
	BGR_CRYSTAL_DMG,
	BGR_GUARD_KILL,
	BGR_GUARD_DEATH,
	BGR_GUARD_DMG_GIVEN,
	BGR_GUARD_DMG_TAKEN,
};

#ifndef BGE_COLOR
	#define	BGE_COLOR	0xFFA500
#endif

struct s_battleground_e_reward_items {
	t_itemid nameid;
	unsigned int amount, happy_amount, max_amount, premium_amount, premium_happy_amount, premium_max_amount;
	int type;
	uint32 rate, happy_rate, premium_rate, premium_happy_rate;
};

struct s_battleground_e_reward_vars {
	std::string name, desc;
	unsigned int amount, happy_amount, max_amount, premium_amount, premium_happy_amount, premium_max_amount;
	int type;
	uint32 rate, premium_rate, happy_rate, premium_happy_rate;
};

struct s_battleground_e_data {
	int master_id;
	int guild_id;
	int teamId;
	int cd_sec;
	int cd_tid;
	struct guild *g;
	std::string channel_name;
	t_tick skill_block_timer[MAX_GUILDSKILL];
};
// End Battleground Expansion

struct s_battleground_member_data {
	unsigned short x, y;
	map_session_data *sd;
	unsigned afk : 1;
	struct point entry_point; ///< Battleground queue entry point
};

struct s_battleground_data {
	int id; ///< Battleground ID
	std::vector<s_battleground_member_data> members; ///< List of players in battleground
	struct point cemetery; ///< Respawn point for players who die
	std::string logout_event; ///< NPC Event to call on log out events
	std::string die_event; ///< NPC Event to call on death events
	std::string active_event; ///< NPC Event to call on players joining an active battleground

	// [RomuloSM]: Battleground Expansion
	struct s_battleground_e_data expansion; ///< Expansion Data
};

struct s_battleground_team {
	uint16 warp_x, warp_y; ///< Team respawn coordinates
	std::string quit_event, ///< Team NPC Event to call on log out events
		death_event, ///< Team NPC Event to call on death events
		active_event, ///< Team NPC Event to call on players joining an active battleground
		bg_id_var; ///< Team NPC variable name

	// [RomuloSM]: Battleground Expansion
	int teamId;
};

struct s_battleground_map {
	int id; ///< Battleground ID
	uint16 mapindex; ///< Index of the map
	s_battleground_team team1, team2; ///< Team data
	std::string bgcallscript; ///< Script to be called when players join the battleground
	bool isReserved; ///< Reserve BG maps that are used so that the system won't create multiple BG instances on the same map
};

/// Enum for queue state tracking
enum e_queue_state : uint16 {
	QUEUE_STATE_SETUP = 0, ///< The initial setup of a queue (a required amount of players hasn't been met)
	QUEUE_STATE_SETUP_DELAY, ///< The initial setup of a queue but a required amount of players have accepted and the delay timer is active
	QUEUE_STATE_ACTIVE, ///< The queue is active script side and more players can join (players may or may not be on the field)
	QUEUE_STATE_ENDED, ///< The queue is no longer joinable (players are getting prizes)
};

/// Battlegrounds client interface queue system [MasterOfMuppets]
struct s_battleground_queue {
	int queue_id; ///< Battlegrounds Queue ID
	int id; ///< Battlegrounds database ID
	std::vector<map_session_data *> teama_members; ///< List of members on team A
	std::vector<map_session_data *> teamb_members; ///< List of members on team B
	int required_players; ///< Amount of players required on each side to start
	int max_players; ///< Maximum amount of players on each side
	int accepted_players; ///< Amount of players who accepted the offer to enter the battleground
	e_queue_state state; ///< See @e_queue_state
	int tid_expire; ///< Timer ID associated with the time out at the ready to enter window
	int tid_start; ///< Timer ID associated with the start delay
	int tid_requeue; ///< Timer ID associated with requeuing this group if all BG maps are reserved
	s_battleground_map *map; ///< Map this BG queue has been assigned to
};

struct s_battleground_type {
	int id; ///< Battlegrounds database ID
	std::string name; ///< Name of the battleground type
	std::string display_name; ///< [RomuloSM]: Battleground Expansion, Display Name of the battleground
	int required_players; ///< Amount of players required on each side to start
	int max_players; ///< Maximum amount of players on each side
	int min_lvl; ///< Minimum level to participate in this battleground type
	int max_lvl; ///< Maximum level to participate in this battleground type
	std::vector<s_battleground_map> maps; ///< List of battleground locations
	uint32 deserter_time; ///< Amount of time a player is marked deserter (seconds)
	uint32 start_delay; ///< Amount of time before the start message is sent to players (seconds)
	bool solo; ///< Ability to join a queue as an individual.
	bool party; ///< Ability to join a queue as a party.
	bool guild; ///< Ability to join a queue as a guild.
	std::vector<int32> job_restrictions; ///< List of jobs that are unable to join.
};

/// Enum of responses when applying for a Battleground
enum e_bg_queue_apply_ack : uint16 {
	BG_APPLY_NONE = 0,
	BG_APPLY_ACCEPT, ///< Accept
	BG_APPLY_QUEUE_FINISHED, ///< Queuing has finished
	BG_APPLY_INVALID_NAME, ///< Invalid name of Battleground
	BG_APPLY_INVALID_APP, ///< Invalid application
	BG_APPLY_PLAYER_COUNT, ///< Too many players in party/guild
	BG_APPLY_PLAYER_LEVEL, ///< Level too low/high
	BG_APPLY_DUPLICATE, ///< Duplicate application
	BG_APPLY_RECONNECT, ///< Reconnect then apply
	BG_APPLY_PARTYGUILD_LEADER, ///< Only party/guild leader can apply
	BG_APPLY_PLAYER_CLASS, ///< Your class can't apply
};

/// Enum of script command bg_info types
enum e_bg_info : uint16 {
	BG_INFO_ID = 0,
	BG_INFO_REQUIRED_PLAYERS,
	BG_INFO_MAX_PLAYERS,
	BG_INFO_MIN_LEVEL,
	BG_INFO_MAX_LEVEL,
	BG_INFO_MAPS,
	BG_INFO_DESERTER_TIME,
};

class BattlegroundDatabase : public TypesafeYamlDatabase<uint32, s_battleground_type> {
public:
	BattlegroundDatabase() : TypesafeYamlDatabase("BATTLEGROUND_DB", 1) {

	}

	const std::string getDefaultLocation() override;
	uint64 parseBodyNode(const ryml::NodeRef& node) override;
};

extern BattlegroundDatabase battleground_db;
extern std::unordered_map<int, std::shared_ptr<s_battleground_data>> bg_team_db;
extern std::vector<std::shared_ptr<s_battleground_queue>> bg_queues;

std::shared_ptr<s_battleground_type> bg_search_name(const char *name);
std::shared_ptr<s_battleground_queue> bg_search_queue(int queue_id);
void bg_send_dot_remove(map_session_data *sd);
int bg_team_get_id(struct block_list *bl);
map_session_data *bg_getavailablesd(s_battleground_data *bg);

bool bg_queue_reservation(const char *name, bool state, bool ended);
#define bg_queue_reserve(name, end) (bg_queue_reservation(name, true, end))
#define bg_queue_unbook(name) (bg_queue_reservation(name, false, false))

int bg_create(uint16 mapindex, s_battleground_team* team);
bool bg_team_join(int bg_id, map_session_data *sd, bool is_queue, bool flag);
bool bg_team_delete(int bg_id);
int bg_team_leave(map_session_data *sd, bool quit, bool deserter, enum bg_team_leave_type flag);
bool bg_team_warp(int bg_id, unsigned short mapindex, short x, short y);
bool bg_player_is_in_bg_map(map_session_data *sd);
bool bg_queue_check_joinable(std::shared_ptr<s_battleground_type> bg, map_session_data *sd, const char *name);
void bg_queue_join_solo(const char *name, map_session_data *sd);
void bg_queue_join_party(const char *name, map_session_data *sd);
void bg_queue_join_guild(const char *name, map_session_data *sd);
uint16 bg_queue_join_multi(const char *name, map_session_data *sd, std::vector<map_session_data *> list);
void bg_queue_clear(std::shared_ptr<s_battleground_queue> queue, bool ended);
bool bg_queue_leave(map_session_data *sd, bool apply_sc = true);
bool bg_queue_on_ready(const char *name, std::shared_ptr<s_battleground_queue> queue);
void bg_queue_on_accept_invite(map_session_data *sd);
void bg_queue_start_battleground(std::shared_ptr<s_battleground_queue> queue);
bool bg_member_respawn(map_session_data *sd);
void bg_send_message(map_session_data *sd, const char *mes, int len);

// [RomuloSM]: Battleground Expansion
/// Enum of script command queue info types
enum e_bg_queue_info : uint16 {
	BG_QUEUE_ID = 0,
	BG_QUEUE_NAME,
	BG_QUEUE_ARENA_NAME,
	BG_QUEUE_REQUIRED_PLAYERS,
	BG_QUEUE_ACCEPTED_PLAYERS,
	BG_QUEUE_MAX_PLAYERS,
	BG_QUEUE_MIN_LEVEL,
	BG_QUEUE_MAX_LEVEL,
	BG_QUEUE_STATE,
	BG_QUEUE_TEAMA_SIZE,
	BG_QUEUE_TEAMB_SIZE,
};

// [RomuloSM]: Battleground Rewards
struct s_battleground_reward {
	int id; ///< Battleground ID
	std::vector<s_battleground_e_reward_items> reward_items; ///< Reward Items Data
	std::vector<s_battleground_e_reward_vars> reward_vars; ///< Reward Items Data
	unsigned int reward_items_count; ///< Reward Items Count
	unsigned int reward_vars_count; ///< Reward Vars Count
};

class BattlegroundRewardDatabase : public TypesafeYamlDatabase<uint32, s_battleground_reward> {
public:
	BattlegroundRewardDatabase() : TypesafeYamlDatabase("BATTLEGROUND_REWARD_DB", 1) {

	}

	const std::string getDefaultLocation();
	uint64 parseBodyNode(const ryml::NodeRef& node);
};

extern BattlegroundRewardDatabase battleground_reward_db;

std::shared_ptr<s_battleground_type> bg_search_id(int id);
std::shared_ptr<s_battleground_reward> bg_reward_search(int id);
int bg_foreachsamemap(int (*func)(struct block_list *,va_list),map_session_data *sd,int range,...);

bool bg_score_damage(struct block_list *src, struct block_list *dst, int damage);
bool bg_score_kills(struct block_list *src, struct block_list *dst);
bool bg_score_heal(struct block_list *bl, int hp, int sp);
bool bg_score_item_heal(map_session_data *sd, int amount, int hp, int sp);
bool bg_score_del_item(map_session_data *sd, int item_id, int amount);
void bg_create_guild();
void bg_leave_notify(std::shared_ptr<s_battleground_data> bg, map_session_data *sd, enum bg_team_leave_type flag, bool changemaster);
void bg_clean_skill(std::shared_ptr<s_battleground_data> bg);
void bg_block_skill(map_session_data *sd, int time);
void bg_block_skill_end(map_session_data *sd);
bool bg_check_skills_time(std::shared_ptr<s_battleground_data> bg);
void bg_player_data(map_session_data *sd);
void bg_reward_reload(void);
bool bg_happyhour_start(void);
bool bg_happyhour_end(void);
bool bg_create_channel(int bg_id, int opt = 0, int msg_delay = 0, int color = 0);
TIMER_FUNC(bg_countdown);
int bg_check_dual(map_session_data *sd);
bool bg_ranking_save(map_session_data *sd, int bg_id, bool clear = false);
bool bg_player_info(map_session_data *sd, map_session_data *tsd);

void do_init_battleground(void);
void do_final_battleground(void);

#endif /* BATTLEGROUND_HPP */
