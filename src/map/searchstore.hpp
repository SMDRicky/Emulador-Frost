// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef SEARCHSTORE_HPP
#define SEARCHSTORE_HPP

#include <memory>
#include <vector>

#include <common/cbasetypes.hpp>
#include <common/mmo.hpp>

#include "clif.hpp"
#include "map.hpp"

#define SEARCHSTORE_RESULTS_PER_PAGE 10

/// Search type constants
enum e_searchstore_searchtype : uint16
{
	// Search for vending stores
	SEARCHTYPE_VENDING = 0,

	// Search for buying stores
	SEARCHTYPE_BUYING_STORE,
};

/// Search effect constants
enum e_searchstore_effecttype : uint16
{
	// Displays the coordinates of the store
	SEARCHSTORE_EFFECT_NORMAL = 0,

	// Opens the store remotely
	SEARCHSTORE_EFFECT_REMOTE,

	SEARCHSTORE_EFFECT_MAX
};

/// information about the search being performed
struct s_search_store_search {
	map_session_data* search_sd;  // sd of the searching player
	const struct PACKET_CZ_SEARCH_STORE_INFO_item* itemlist;
	const struct PACKET_CZ_SEARCH_STORE_INFO_item* cardlist;
	unsigned int item_count;
	unsigned int card_count;
	unsigned int min_price;
	unsigned int max_price;
};

struct s_search_store_info_item {
	int store_id;
	uint32 account_id;
	char store_name[MESSAGE_SIZE];
	t_itemid nameid;
	unsigned short amount;
	unsigned int price;
	t_itemid card[MAX_SLOTS];
	unsigned char refine;
	uint8 enchantgrade;
};

struct s_search_store_info {
	std::vector<std::shared_ptr<s_search_store_info_item>> items;
	unsigned int pages;  // amount of pages already sent to client
	unsigned int uses;
	int remote_id;
	time_t nextquerytime;
	unsigned short effect;  // 0 = Normal (display coords), 1 = Cash (remote open store)
	unsigned char type;  // 0 = Vending, 1 = Buying Store
	bool open;
};

bool searchstore_open(map_session_data* sd, unsigned int uses, unsigned short effect);
void searchstore_query(map_session_data* sd, unsigned char type, unsigned int min_price, unsigned int max_price, const struct PACKET_CZ_SEARCH_STORE_INFO_item* itemlist, unsigned int item_count, const struct PACKET_CZ_SEARCH_STORE_INFO_item* cardlist, unsigned int card_count);
bool searchstore_querynext(map_session_data* sd);
void searchstore_next(map_session_data* sd);
void searchstore_clear(map_session_data* sd);
void searchstore_close(map_session_data* sd);
void searchstore_click(map_session_data* sd, uint32 account_id, int store_id, t_itemid nameid);
bool searchstore_queryremote(map_session_data* sd, uint32 account_id);
void searchstore_clearremote(map_session_data* sd);

#endif /* SEARCHSTORE_HPP */
