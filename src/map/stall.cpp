// Copyright (c) RomuloSM
// Email: romulodevel@gmail.com - romulo@rsmdeveloper.com.br
// Site: https://www.rsmdeveloper.com.br
// Facebook: https://www.facebook.com/RomuloDevel
// rAthena: https://rathena.org/board/profile/6097-sbk_/

#include "stall.hpp"

#include <stdlib.h> // atoi
#include <sstream>

#include <common/malloc.hpp> // aMalloc, aFree
#include <common/nullpo.hpp>
#include <common/showmsg.hpp> // ShowInfo
#include <common/strlib.hpp>
#include <common/timer.hpp>  // DIFF_TICK

#include "achievement.hpp"
#include "battle.hpp"
#include "chrif.hpp"
#include "clif.hpp"
#include "intif.hpp" //mail send
#include "itemdb.hpp"
#include "log.hpp"
#include "npc.hpp"
#include "pc.hpp"
#include "pc_groups.hpp"
#include "vending.hpp"

//Stall
static int stall_id=START_STALL_NUM;
std::vector<s_stall_data *> stall_db;
std::vector<mail_message> stall_mail_db;

/// failure constants for clif functions
enum e_buyingstore_failure
{
	BUYINGSTORE_CREATE               = 1,  // "Failed to open buying store."
	BUYINGSTORE_CREATE_OVERWEIGHT    = 2,  // "Total amount of then possessed items exceeds the weight limit by %d. Please re-enter."
	BUYINGSTORE_TRADE_BUYER_ZENY     = 3,  // "All items within the buy limit were purchased."
	BUYINGSTORE_TRADE_BUYER_NO_ITEMS = 4,  // "All items were purchased."
	BUYINGSTORE_TRADE_SELLER_FAILED  = 5,  // "The deal has failed."
	BUYINGSTORE_TRADE_SELLER_COUNT   = 6,  // "The trade failed, because the entered amount of item %s is higher, than the buyer is willing to buy."
	BUYINGSTORE_TRADE_SELLER_ZENY    = 7,  // "The trade failed, because the buyer is lacking required balance."
	BUYINGSTORE_CREATE_NO_INFO       = 8,  // "No sale (purchase) information available."
};

static const t_itemid buyingstore_blankslots[MAX_SLOTS] = { 0 };

/**
 * Create an unique vending shop id.
 * @return the next vending_id
 */
static int stall_getuid(void)
{
	if( stall_id >= START_STALL_NUM && !map_blid_exists(stall_id) )
		return stall_id++;// available
	else {// find next id
		int base_id = stall_id;
		while( base_id != ++stall_id ) {
			if( stall_id < START_STALL_NUM )
				stall_id = START_STALL_NUM;
			if( !map_blid_exists(stall_id) )
				return stall_id++;// available
		}
		// full loop, nothing available
		ShowFatalError("stall_get_new_stall_id: All ids are taken. Exiting...");
		exit(1);
	}
}

/**
* Open stall UI for vendor
* @param sd Player
* @param skill_lv level of skill used
* @param type 0 = vending - 1 = buying
*/
int8 stall_ui_open(map_session_data* sd, short type){
	nullpo_retr(1, sd);

	if (sd->state.vending || sd->state.buyingstore || sd->state.trading) {
		return 1;
	}

	if( sd->sc.getSCE(SC_NOCHAT) && (sd->sc.getSCE(SC_NOCHAT)->val1&MANNER_NOROOM) ) {// custom: mute limitation
		return 2;
	}

	if( map_getmapflag(sd->bl.m, MF_NOVENDING) ) {// custom: no vending maps
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,276), false, SELF); // "You can't open a shop on this map"
		return 3;
	}

	if(map_getmapflag(sd->bl.m, MF_NOSTALL)) {
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1952), false, SELF);
		return 3;
	}

	if(type == 0)
		clif_stall_vending_open(sd);
	else
		clif_stall_buying_open(sd);

	return 0;
}

/**
 * Player setup a new vending stall
 * @param sd : player opening the shop
 * @param message : shop title
 * @param xPos : pos X
 * @param yPos : pos Y
 * @param data : itemlist data
 *	data := {<index>.w <amount>.w <value>.l}[count]
 * @param count : number of different items
 * @return 0 If success, 1 - Cannot open (die, not state.prevend, trading), 2 - No cart, 3 - Count issue, 4 - Cart data isn't saved yet, 5 - No valid item found
 */
int8 stall_vending_setup(map_session_data* sd, const char* message, const int16 xPos, const int16 yPos, uint8 *data, int count)
{
	int i, j, k, l;
	char message_sql[MESSAGE_SIZE*2];
	StringBuf buf;
	struct block_list npc_near_bl;

	nullpo_retr(false,sd);

	if ( pc_isdead(sd) || !sd->state.prevend || pc_istrading(sd) ) { //add check existing stall
		clif_stall_ui_close(sd,100,STALLSTORE_OK);
		return 1; // can't open stalls lying dead || didn't use via the skill (wpe/hack) || can't have 2 shops at once
	}

	// Test if shop is already set for this char - Has been check before but use to avoid wpe / packets manipulation
	if(stall_isStallOpen(sd->status.char_id)){
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1950), false, SELF);
		clif_stall_ui_close(sd,100,STALLSTORE_OK);
		return 1;
	}

	if(map_getmapflag(sd->bl.m, MF_NOSTALL)) {
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1952), false, SELF);
		clif_stall_ui_close(sd,100,STALLSTORE_OK);
		return 1;
	}

	// check number of items in shop
	if( count < 1 || count > 2 + sd->stall.slots ) { // invalid item count
		clif_skill_fail(sd, ALL_ASSISTANT_VENDING, USESKILL_FAIL_LEVEL, 0);
		clif_stall_ui_close(sd,100,STALLSTORE_OK);
		return 3;
	}

	// check if shop is allow on the cell
	if( map_getcell(sd->bl.m,xPos,yPos,CELL_CHKNOVENDING) || map_getcell(sd->bl.m,xPos,yPos,CELL_CHKNOSTALL) ) {
		clif_stall_ui_close(sd,100,2);
		return 1;
	}

	if(map_getmapflag(sd->bl.m, MF_NOSTALL)) {
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1952), false, SELF);
		clif_stall_ui_close(sd,100,STALLSTORE_OK);
		return 1;
	}

	npc_near_bl.m = sd->bl.m;
	npc_near_bl.x = xPos;
	npc_near_bl.y = yPos;
	if( npc_isnear(&npc_near_bl) ) {
		char output[150];
		sprintf(output, msg_txt(sd,662), battle_config.min_npc_vendchat_distance);
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], output, false, SELF);
		clif_stall_ui_close(sd,100,2);
		return true;
	}

	struct s_stall_data *stl = (struct s_stall_data*)aCalloc(1, sizeof(struct s_stall_data));
	stl->char_id = sd->status.char_id; // Got it now to send items back in case something wrong

	if (save_settings&CHARSAVE_VENDING) // Avoid invalid data from saving
		chrif_save(sd, CSAVE_INVENTORY);

	// filter out invalid items
	i = 0;
	for( j = 0; j < count; j++ ) {
		short index        = *(uint16*)(data + 8*j + 0);
		short amount       = *(uint16*)(data + 8*j + 2);
		unsigned int value = *(uint32*)(data + 8*j + 4);

		index = index - 2; // TODO: clif::server_index

		if( index < 0 || index >= MAX_INVENTORY // invalid position
		||  sd->inventory.u.items_inventory[index].amount < 0 // invalid item or insufficient quantity
		//NOTE: official server does not do any of the following checks!
		||  !sd->inventory.u.items_inventory[index].identify // unidentified item
		||  sd->inventory.u.items_inventory[index].attribute == 1 // broken item
		||  sd->inventory.u.items_inventory[index].expire_time // It should not be in the cart but just in case
		||  (sd->inventory.u.items_inventory[index].bound && !pc_can_give_bounded_items(sd)) // can't trade account bound items and has no permission
		||  !itemdb_cantrade(&sd->inventory.u.items_inventory[index], pc_get_group_level(sd), pc_get_group_level(sd)) ) // untradeable item
			continue;

		memcpy(&stl->items_inventory[i],&sd->inventory.u.items_inventory[index],sizeof(struct item));

		pc_delitem(sd, index, amount, 0, 0, LOG_TYPE_VENDING);

		stl->items_inventory[i].amount = amount;
		stl->price[i] = min(value, (unsigned int)battle_config.vending_max_value);
		i++; // item successfully added
	}

	stl->vend_num = i;

	if (i != j || j > MAX_STALL_SLOT) {
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,266), false, SELF); //"Some of your items cannot be vended and were removed from the shop."
		clif_skill_fail(sd, ALL_ASSISTANT_VENDING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
		clif_stall_ui_close(sd,100,STALLSTORE_OK);
		stall_vending_getbackitems(stl);
		aFree(stl);
		return 5;
	}

	if( i == 0 ) { // no valid item found
		clif_skill_fail(sd, ALL_ASSISTANT_VENDING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
		clif_stall_ui_close(sd,100,STALLSTORE_OK);
		aFree(stl);
		return 5;
	}

	if( sd->stall.itemindex != -1 ) {
		if( sd->inventory.u.items_inventory[sd->stall.itemindex].nameid <= 0 || sd->inventory.u.items_inventory[sd->stall.itemindex].amount <= 0 ) {
			std::shared_ptr<item_data> i_data = item_db.find(sd->stall.itemid);
			if (i_data != nullptr) {
				char output[CHAT_SIZE_MAX];
				sprintf(output, msg_txt(sd, 1964), i_data->ename.c_str());
				clif_messagecolor(&sd->bl, color_table[COLOR_RED], output, false, SELF);
			}
			clif_skill_fail(sd, ALL_ASSISTANT_VENDING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
			clif_stall_ui_close(sd,100,STALLSTORE_OK);
			stall_vending_getbackitems(stl);
			aFree(stl);
			return 5;
		}

		pc_delitem(sd, sd->stall.itemindex, 1, 0, 0, LOG_TYPE_VENDING);
	}

	stl->type = 0; // TODO vending
	stl->vender_id = stall_getuid();
	stl->expire_time = 0;
	stl->mapindex = sd->mapindex;
	stl->timer = INVALID_TIMER;
	safestrncpy(stl->message, message, MESSAGE_SIZE);
	safestrncpy(stl->name, sd->status.name, NAME_LENGTH);

	if( sd->stall.seconds )
		stl->expire_time = (unsigned int)(time(nullptr) + sd->stall.seconds);

	stl->bl.id = stl->vender_id;
	stl->bl.type = BL_STALL;
	stl->bl.m = sd->bl.m;
	stl->bl.x = xPos;
	stl->bl.y = yPos;

	stl->vd.class_ = sd->vd.class_;
	stl->vd.weapon = sd->vd.weapon;
	stl->vd.shield = sd->vd.shield;
	stl->vd.head_top = sd->vd.head_top;
	stl->vd.head_mid = sd->vd.head_mid;
	stl->vd.head_bottom = sd->vd.head_bottom;
	stl->vd.hair_style = sd->vd.hair_style;
	stl->vd.hair_color = sd->vd.hair_color;
	stl->vd.cloth_color = sd->vd.cloth_color;
	stl->vd.body_style = sd->vd.body_style;
	stl->vd.sex = sd->vd.sex;

	// [RomuloSM]: Extended Stall
	stl->extended_vending = battle_config.extended_stall_enable ? sd->stall.nameid : 0;

	Sql_EscapeString( mmysql_handle, message_sql, stl->message );

	if( Sql_Query( mmysql_handle, "INSERT INTO `%s`(`id`, `char_id`, `type`, `class`, `sex`, `map`, `x`, `y`,"
								  "`title`, `hair`, `hair_color`, `body`, `weapon`, `shield`, `head_top`, `head_mid`, `head_bottom`,"
								  "`clothes_color`, `name`, `expire_time`, `extended_vending_item`) "
		"VALUES( %d, %d, %d, %d, '%c', '%s', %d, %d, '%s', %d, %d, %d, %d, %d, %d, %d, %d, %d, '%s', %u, %u)",
		stalls_table, stl->vender_id, stl->char_id, stl->type, stl->vd.class_, stl->vd.sex == SEX_FEMALE ? 'F' : 'M', mapindex_id2name(stl->mapindex), stl->bl.x, stl->bl.y,
		message_sql, stl->vd.hair_style, stl->vd.hair_color, stl->vd.body_style, stl->vd.weapon, stl->vd.shield, stl->vd.head_top, stl->vd.head_mid, stl->vd.head_bottom,
		stl->vd.cloth_color, stl->name, stl->expire_time, stl->extended_vending) != SQL_SUCCESS ) {
		Sql_ShowDebug(mmysql_handle);
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1953), false, SELF);
		clif_skill_fail(sd, ALL_ASSISTANT_VENDING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
		clif_stall_ui_close(sd,100,STALLSTORE_OK);
		stall_vending_getbackitems(stl);
		aFree(stl);
		return 5;
	}

	StringBuf_Init(&buf);
	StringBuf_Printf(&buf, "INSERT INTO `%s`(`stalls_id`,`index`,`nameid`,`amount`,`identify`,`refine`,`attribute`",stalls_vending_items_table);
	for( l = 0; l < MAX_SLOTS; ++l )
		StringBuf_Printf(&buf, ", `card%d`", l);
	for( l = 0; l < MAX_ITEM_RDM_OPT; ++l ) {
		StringBuf_Printf(&buf, ", `option_id%d`", l);
		StringBuf_Printf(&buf, ", `option_val%d`", l);
		StringBuf_Printf(&buf, ", `option_parm%d`", l);
	}
	StringBuf_Printf(&buf, ",`expire_time`,`bound`,`unique_id`,`enchantgrade`,`price`) VALUES", stalls_vending_items_table);
	for (j = 0; j < i; j++) {
		StringBuf_Printf(&buf, "(%d, %d, %u, %d, %d, %d, %d",
			stl->vender_id, j, stl->items_inventory[j].nameid, stl->items_inventory[j].amount, stl->items_inventory[j].identify, stl->items_inventory[j].refine, stl->items_inventory[j].attribute);
		for( k = 0; k < MAX_SLOTS; ++k )
			StringBuf_Printf(&buf, ", %u", stl->items_inventory[j].card[k]);
		for( k = 0; k < MAX_ITEM_RDM_OPT; ++k ) {
			StringBuf_Printf(&buf, ", %d", stl->items_inventory[j].option[k].id);
			StringBuf_Printf(&buf, ", %d", stl->items_inventory[j].option[k].value);
			StringBuf_Printf(&buf, ", %d", stl->items_inventory[j].option[k].param);
		}
		StringBuf_Printf(&buf, ", %u, %d , '%" PRIu64 "', %d, %d)",
			stl->items_inventory[j].expire_time, stl->items_inventory[j].bound, stl->items_inventory[j].unique_id, stl->items_inventory[j].enchantgrade, stl->price[j]);
		if (j < i-1)
			StringBuf_AppendStr(&buf, ",");
	}
	if (SQL_ERROR == Sql_QueryStr(mmysql_handle, StringBuf_Value(&buf))){
		Sql_ShowDebug(mmysql_handle);
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1954), false, SELF);
		clif_skill_fail(sd, ALL_ASSISTANT_VENDING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
		clif_stall_ui_close(sd,100,STALLSTORE_OK);
		stall_vending_getbackitems(stl);
		aFree(stl);
		return 5;
	}
	StringBuf_Destroy(&buf);

	if( stl->expire_time ) {
		stl->timer = add_timer(gettick() + (stl->expire_time - time(NULL)) * 1000,
					stall_timeout, stl->bl.id, 0);
	}

	if(map_addblock(&stl->bl))
		return -1;
	status_change_init(&stl->bl);
	map_addiddb(&stl->bl);
	stall_db.push_back(stl);

	clif_stall_showunit(sd,stl);
	clif_showstallboard(&sd->bl,stl->vender_id,stl->message);
	clif_stall_ui_close(sd,100,STALLSTORE_OK);
	return 0;
}

/**
 * Player setup a new vending stall
 * @param sd : player opening the shop
 * @param message : shop title
 * @param xPos : pos X
 * @param yPos : pos Y
 * @param data : itemlist data
 *	data := {<index>.w <amount>.w <value>.l}[count]
 * @param count : number of different items
 * @return 0 If success, 1 - Cannot open (die, not state.prevend, trading), 2 - No cart, 3 - Count issue, 4 - Cart data isn't saved yet, 5 - No valid item found
 */
int8 stall_buying_setup(map_session_data* sd, const char* message, const int16 xPos, const int16 yPos, const struct STALL_BUYING_SET_sub* itemlist, int count, uint64 total_price)
{
	int i, j, weight, listidx;
	char message_sql[MESSAGE_SIZE*2];
	StringBuf buf;
	struct block_list npc_near_bl;

	nullpo_retr(false,sd);

	if ( pc_isdead(sd) || !sd->state.prevend || pc_istrading(sd) ) { //add check existing stall
		clif_stall_ui_close(sd,101,STALLSTORE_OK);
		return 1; // can't open stalls lying dead || didn't use via the skill (wpe/hack) || can't have 2 shops at once
	}

	// Test if shop is already set for this char - Has been check before but use to avoid wpe / packets manipulation
	if(stall_isStallOpen(sd->status.char_id)){
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1950), false, SELF);
		clif_stall_ui_close(sd,101,STALLSTORE_OK);
		return 1;
	}

	npc_near_bl.m = sd->bl.m;
	npc_near_bl.x = xPos;
	npc_near_bl.y = yPos;
	if( npc_isnear(&npc_near_bl) ) {
		char output[150];
		sprintf(output, msg_txt(sd,662), battle_config.min_npc_vendchat_distance);
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], output, false, SELF);
		clif_stall_ui_close(sd,101,STALLSTORE_POSITION);
		return true;
	}

	// check number of items in shop
	if( count < 1 || count > 2 + sd->stall.slots ) { // invalid item count
		clif_stall_ui_close(sd,101,STALLSTORE_OK);
		return 3;
	}

	// check if shop is allow on the cell
	if( map_getcell(sd->bl.m,xPos,yPos,CELL_CHKNOVENDING) || map_getcell(sd->bl.m,xPos,yPos,CELL_CHKNOSTALL) ) {
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,204), false, SELF); // "You can't open a shop on this cell."
		clif_stall_ui_close(sd,101,STALLSTORE_POSITION);
		return 1;
	}

	if(map_getmapflag(sd->bl.m, MF_NOSTALL)) {
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1952), false, SELF);
		clif_stall_ui_close(sd,101,STALLSTORE_OK);
		return 1;
	}

	if(total_price <= 0){
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1955), false, SELF);
		clif_stall_ui_close(sd,101,STALLSTORE_OK);
		return 1;
	}

	struct s_stall_data *stl = (struct s_stall_data*)aCalloc(1, sizeof(struct s_stall_data));
	stl->char_id = sd->status.char_id; // Got it now to send items back in case something wrong

	if (save_settings&CHARSAVE_VENDING) // Avoid invalid data from saving
		chrif_save(sd, CSAVE_INVENTORY);

	weight = sd->weight;

	// filter out invalid items
	i = 0;
	uint32 temp_price = 0;
	for( j = 0; j < count; j++ ) {
		const struct STALL_BUYING_SET_sub *item = &itemlist[i];

		struct item_data* id = itemdb_search( item->itemId );

		if( id == NULL || item->count == 0 // invalid input
		||  item->price <= 0 || item->price > BUYINGSTALL_MAX_PRICE // invalid price: unlike vending, items cannot be bought at 0 Zeny
		||  !id->flag.buyingstore || !itemdb_cantrade_sub( id, pc_get_group_level( sd ), pc_get_group_level( sd ) ) ) // untradeable item
			continue;

		int idx = pc_search_inventory( sd, item->itemId );

		// At least one must be owned
		if( idx < 0 ){
			break;
		}

		// too many items of same kind
		if( sd->inventory.u.items_inventory[idx].amount + item->count > BUYINGSTALL_MAX_AMOUNT ){
			break;
		}

		// duplicate check. as the client does this too, only malicious intent should be caught here
		if( j ){
			ARR_FIND( 0, j, listidx, sd->buyingstore.items[listidx].nameid == item->itemId );

			// duplicate
			if( listidx != j ){
				ShowWarning( "stall_buying_setup: Found duplicate item on buying list (nameid=%u, amount=%hu, account_id=%d, char_id=%d).\n", item->itemId, item->count, sd->status.account_id, sd->status.char_id );
				break;
			}
		}

		weight+= id->weight*item->count;
		stl->itemId[i] = item->itemId;
		stl->amount[i] = item->count;
		stl->price[i]  = item->price;

		uint32 price = item->count * item->price;
		if(item->price > MAX_ZENY)
			price = price * (100 + STALL_TAX) / 100;
		temp_price += price;

		i++; // item successfully added
	}

	stl->vend_num = i;

	if (i != j || j > MAX_STALL_SLOT) {
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,266), false, SELF); //"Some of your items cannot be vended and were removed from the shop."
		clif_skill_fail(sd, ALL_ASSISTANT_BUYING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
		clif_stall_ui_close(sd,101,STALLSTORE_OK);
		//stall_buying_getbackzeny(stl);
		aFree(stl);
		return 5;
	}

	if( (sd->max_weight*90)/100 < weight )
	{// not able to carry all wanted items without getting overweight (90%)
		clif_stall_ui_close(sd,101,STALLSTORE_OVERWEIGHT);
		//stall_buying_getbackzeny(stl);
		aFree(stl);
		return 7;
	}

	if( i == 0 ) { // no valid item found
		clif_skill_fail(sd, ALL_ASSISTANT_BUYING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
		clif_stall_ui_close(sd,101,STALLSTORE_OK);
		aFree(stl);
		return 5;
	}

	if( sd->stall.itemindex != -1 ) {
		if( sd->inventory.u.items_inventory[sd->stall.itemindex].nameid <= 0 || sd->inventory.u.items_inventory[sd->stall.itemindex].amount <= 0 ) {
			std::shared_ptr<item_data> i_data = item_db.find(sd->inventory.u.items_inventory[sd->stall.itemindex].nameid);
			if (i_data != nullptr) {
				char output[CHAT_SIZE_MAX];
				sprintf(output, msg_txt(sd, 1964), i_data->ename.c_str());
				clif_messagecolor(&sd->bl, color_table[COLOR_RED], output, false, SELF);
			}
			clif_skill_fail(sd, ALL_ASSISTANT_BUYING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
			clif_stall_ui_close(sd,100,STALLSTORE_OK);
			//stall_buying_getbackzeny(stl);
			aFree(stl);
			return 5;
		}

		pc_delitem(sd, sd->stall.itemindex, 1, 0, 0, LOG_TYPE_VENDING);
	}

	if( battle_config.extended_stall_enable ) {
		if( !sd->stall.nameid || sd->stall.nameid == battle_config.item_zeny ) {
			if( sd->status.zeny < temp_price  || temp_price < 0 || MAX_ZENY < temp_price ) {
				char output[1024];
				sprintf(output, msg_txt(sd,1758), itemdb_ename(stl->extended_vending));
				clif_messagecolor(&sd->bl, color_table[COLOR_RED], output, false, SELF);
				clif_skill_fail(sd, ALL_ASSISTANT_BUYING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
				clif_stall_ui_close(sd,101,STALLSTORE_OK);
				//stall_buying_getbackzeny(stl);
				aFree(stl);
				return 8;
			}

			pc_payzeny(sd, (int)temp_price, LOG_TYPE_BUYING_STORE, sd->status.char_id);
		}
		else if( sd->stall.nameid == battle_config.item_cash ) {
			if( temp_price > sd->cashPoints || temp_price < 0 || MAX_ZENY < temp_price ) {
				char output[1024];
				sprintf(output, msg_txt(sd,1758), itemdb_ename(sd->stall.nameid));
				clif_messagecolor(&sd->bl, color_table[COLOR_RED], output, false, SELF);
				clif_skill_fail(sd, ALL_ASSISTANT_BUYING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
				clif_stall_ui_close(sd,101,STALLSTORE_OK);
				//stall_buying_getbackzeny(stl);
				aFree(stl);
				return 8;
			}

			pc_paycash(sd, (int)temp_price, 0, LOG_TYPE_BUYING_STORE);
		}
		else {
			int k, loot_count = 0;
			for( k = 0; k < MAX_INVENTORY; k++ ) {
				if( sd->inventory.u.items_inventory[k].bound ) {
					clif_skill_fail(sd, ALL_ASSISTANT_BUYING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
					clif_stall_ui_close(sd, 101, STALLSTORE_OK);
					//stall_buying_getbackzeny(stl);
					aFree(stl);
					return 8;
				}

				if( sd->inventory.u.items_inventory[k].nameid == sd->stall.nameid )
					loot_count += sd->inventory.u.items_inventory[k].amount;
			}
 
			if(temp_price > loot_count || temp_price < 0 ) {
				char output[1024];
				sprintf(output, msg_txt(sd,1758), itemdb_ename(sd->stall.nameid));
				clif_messagecolor(&sd->bl, color_table[COLOR_RED], output, false, SELF);
				clif_skill_fail(sd, ALL_ASSISTANT_BUYING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
				clif_stall_ui_close(sd,101,STALLSTORE_OK);
				//stall_buying_getbackzeny(stl);
				aFree(stl);
				return 8;
			}

			int get_items = 0;
			for( k = 0; k < MAX_INVENTORY && get_items < temp_price; k++ ) {
				int qnty = 0;
				if( sd->inventory.u.items_inventory[k].bound )
					continue;

				if( sd->inventory.u.items_inventory[k].nameid != sd->stall.nameid )
					continue;

				if( (get_items+sd->inventory.u.items_inventory[k].amount) > temp_price )
					qnty = temp_price - get_items;
				else
					qnty = sd->inventory.u.items_inventory[k].amount;

				pc_delitem(sd, k, qnty, 0, 6, LOG_TYPE_NONE);
				get_items += qnty;
			}

			log_pick_pc(sd, LOG_TYPE_BUYING_STORE, -(int)temp_price, &sd->inventory.u.items_inventory[k]);
		}
	}
	else {
		if( sd->status.zeny < temp_price ) {
			clif_skill_fail(sd, ALL_ASSISTANT_BUYING, USESKILL_FAIL_LEVEL, 0); // custom reply packet
			clif_stall_ui_close(sd,101,STALLSTORE_OK);
			//stall_buying_getbackzeny(stl);
			aFree(stl);
			return 8;
		}
	}

	stl->type = 1;
	stl->vender_id = stall_getuid();
	stl->expire_time = 0;
	stl->mapindex = sd->mapindex;
	stl->timer = INVALID_TIMER;
	safestrncpy(stl->message, message, MESSAGE_SIZE);
	safestrncpy(stl->name, sd->status.name, NAME_LENGTH);

	if( sd->stall.seconds )
		stl->expire_time = (unsigned int)(time(nullptr) + sd->stall.seconds);

	stl->bl.id = stl->vender_id;
	stl->bl.type = BL_STALL;
	stl->bl.m = sd->bl.m;
	stl->bl.x = xPos;
	stl->bl.y = yPos;

	stl->vd.class_ = sd->vd.class_;
	stl->vd.weapon = sd->vd.weapon;
	stl->vd.shield = sd->vd.shield;
	stl->vd.head_top = sd->vd.head_top;
	stl->vd.head_mid = sd->vd.head_mid;
	stl->vd.head_bottom = sd->vd.head_bottom;
	stl->vd.hair_style = sd->vd.hair_style;
	stl->vd.hair_color = sd->vd.hair_color;
	stl->vd.cloth_color = sd->vd.cloth_color;
	stl->vd.body_style = sd->vd.body_style;
	stl->vd.sex = sd->vd.sex;

	// [RomuloSM]: Extended Stall
	stl->extended_vending = battle_config.extended_stall_enable ? sd->stall.nameid : 0;

	Sql_EscapeString( mmysql_handle, message_sql, stl->message );

	if( Sql_Query( mmysql_handle, "INSERT INTO `%s` (`id`, `char_id`, `type`, `class`, `sex`, `map`, `x`, `y`,"
								  "`title`, `hair`, `hair_color`, `body`, `weapon`, `shield`, `head_top`, `head_mid`, `head_bottom`,"
								  "`clothes_color`, `name`, `expire_time`, `extended_vending_item`) "
		"VALUES (%d, %d, %d, %d, '%c', '%s', %d, %d, '%s', %d, %d, %d, %d, %d, %d, %d, %d, %d, '%s', %u, %u);",
		stalls_table, stl->vender_id, stl->char_id, stl->type, stl->vd.class_, stl->vd.sex == SEX_FEMALE ? 'F' : 'M', mapindex_id2name(stl->mapindex), stl->bl.x, stl->bl.y,
		message_sql, stl->vd.hair_style, stl->vd.hair_color, stl->vd.body_style, stl->vd.weapon, stl->vd.shield, stl->vd.head_top, stl->vd.head_mid, stl->vd.head_bottom,
		stl->vd.cloth_color, stl->name, stl->expire_time, stl->extended_vending) != SQL_SUCCESS ) {
		Sql_ShowDebug(mmysql_handle);
	}

	StringBuf_Init(&buf);
	StringBuf_Printf(&buf, "INSERT INTO `%s`(`stalls_id`,`nameid`,`amount`,`price`) VALUES",stalls_buying_items_table);
	for (j = 0; j < i; j++) {
		StringBuf_Printf(&buf, "(%d, %u, %d, %d)",
			stl->vender_id, stl->itemId[j], stl->amount[j], stl->price[j]);
		if (j < i-1)
			StringBuf_AppendStr(&buf, ",");
	}
	if (SQL_ERROR == Sql_QueryStr(mmysql_handle, StringBuf_Value(&buf)))
		Sql_ShowDebug(mmysql_handle);
	StringBuf_Destroy(&buf);

	if( stl->expire_time ) {
		stl->timer = add_timer(gettick() + (stl->expire_time - time(NULL)) * 1000,
					stall_timeout, stl->bl.id, 0);
	}

	if(map_addblock(&stl->bl))
		return -1;
	status_change_init(&stl->bl);
	map_addiddb(&stl->bl);
	stall_db.push_back(stl);

	clif_stall_showunit(sd,stl);
	clif_stall_ui_close(sd,101,STALLSTORE_OK);
	clif_buyingstall_entry(&sd->bl,stl->vender_id,stl->message);
	return 0;
}

bool stall_isStallOpen(unsigned int CID) {
	auto itStalls = std::find_if(stall_db.begin(), stall_db.end(), [&](s_stall_data *const & itstl) {
						return CID == itstl->char_id;
					});

	if(itStalls != 	stall_db.end()){
		return true;
	}

	return false;
}

/**
 * Player request a stall's item list (a vending stall)
 * @param sd : player requestion the list
 * @param id : vender account id (gid)
 */
void stall_vending_listreq(map_session_data* sd, int id)
{
	nullpo_retv(sd);
	struct s_stall_data* stl;

	if( (stl = map_id2st(id)) == NULL )
		return;

	if (!pc_can_give_items(sd)) { //check if both GMs are allowed to trade
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,246), false, SELF);
		return;
	}

	// [RomuloSM] Extended Stall
	if( battle_config.extended_stall_enable && stl->extended_vending ) {
		char output[1024];
		sprintf(output, msg_txt(sd,1751), stl->name, itemdb_ename(stl->extended_vending));
		if( battle_config.extended_stall_broadcast )
			clif_broadcast(&sd->bl, output, (int)strlen(output) + 1, 0x10, SELF);
		else
			clif_messagecolor(&sd->bl, color_table[COLOR_CYAN], output, false, SELF);
	}

	clif_stall_vending_list( sd, stl );
}

/**
 * Player request a stall's item list (a buying stall)
 * @param sd : player requestion the list
 * @param id : vender account id (gid)
 */
void stall_buying_listreq(map_session_data* sd, int id)
{
	nullpo_retv(sd);
	struct s_stall_data* stl;

	if( !battle_config.feature_buying_store || pc_istrading(sd) )
	{// not allowed to sell
		return;
	}

	if( (stl = map_id2st(id)) == NULL )
		return;

	if( !pc_can_give_items(sd) )
	{// custom: GM is not allowed to sell
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,246), false, SELF);
		return;
	}

	// [RomuloSM] Extended Stall
	if( battle_config.extended_stall_enable && stl->extended_vending ) {
		char output[1024];
		sprintf(output, msg_txt(sd,1752), stl->name, itemdb_ename(stl->extended_vending));
		if( battle_config.extended_stall_broadcast )
			clif_broadcast(&sd->bl, output, (int)strlen(output) + 1, 0x10, SELF);
		else
			clif_messagecolor(&sd->bl, color_table[COLOR_CYAN], output, false, SELF);
	}

	clif_stall_buying_list( sd, stl );
}

/**
 * Purchase item(s) from a stall
 * @param sd : buyer player session
 * @param aid : char id of vender
 * @param uid : stall unique id
 * @param data : items data who would like to purchase \n
 *	data := {<index>.w <amount>.w }[count]
 * @param count : number of different items he's trying to buy
 */
void stall_vending_purchasereq(map_session_data* sd, int aid, int uid, const uint8* data, int count)
{
	int i, w;
	double z;
	struct s_stall_data* stl = map_id2st(uid);

	nullpo_retv(sd);
	if( stl == NULL )
		return; // invalid shop

	if(!stall_isStallOpen(stl->char_id)){
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1951), false, SELF);
		return;
	}

	if( stl->vender_id != uid || stl->char_id != aid ) { // shop has changed
		clif_buyvending(sd, 0, 0, 6);  // store information was incorrect
		return;
	}

	if( !searchstore_queryremote(sd, stl->char_id) && (sd->bl.m != stl->bl.m || !check_distance_bl(&sd->bl, &stl->bl, AREA_SIZE)) )
		return; // shop too far away

	if( count < 1 || count > MAX_STALL_SLOT || count > stl->vend_num )
		return; // invalid amount of purchased items

	// some checks
	z = 0.; // zeny counter
	w = 0;  // weight counter
	for( i = 0; i < count; i++ ) {
		short amount = *(uint16*)(data + 4*i + 0);
		short idx    = *(uint16*)(data + 4*i + 2);
		idx -= 1;

		if( amount <= 0 )
			return;

		// check of item index
		if( idx < 0 || idx >= stl->vend_num )
			return;

		// items has been bought by anyone else
		if( stl->items_inventory[idx].amount <= 0 )
			return;

		z += ((double)stl->price[idx] * (double)amount);
		if( z > (double)sd->status.zeny || z < 0. || z > (double)MAX_ZENY ) {
			clif_buyvending(sd, idx, amount, 1); // you don't have enough zeny
			return;
		}

		w += itemdb_weight(stl->items_inventory[idx].nameid) * amount;
		if( w + sd->weight > sd->max_weight ) {
			clif_buyvending(sd, idx, amount, 2); // you can not buy, because overweight
			return;
		}

		//Check to see if cart/vend info is in sync.
		if( amount > stl->items_inventory[idx].amount ){
			clif_buyvending(sd, idx, stl->items_inventory[idx].amount, 4); // not enough quantity
			return;
		}
	}

	if( battle_config.extended_stall_enable ) {
		if( !stl->extended_vending || stl->extended_vending == battle_config.item_zeny ) {
			if( (double)sd->status.zeny < z  || z < 0. || (double)MAX_ZENY < z ) {
				char output[1024];
				sprintf(output, msg_txt(sd,1758), itemdb_ename(stl->extended_vending));
				clif_messagecolor(&sd->bl, color_table[COLOR_RED], output, false, SELF);
				return;
			}

			pc_payzeny(sd, (int)z, LOG_TYPE_VENDING, sd->status.char_id);
			achievement_update_objective(sd, AG_SPEND_ZENY, 1, (int)z);
		}
		else if(stl->extended_vending == battle_config.item_cash ) {
			if( z > sd->cashPoints || z < 0. || (double)MAX_ZENY < z ) {
				char output[1024];
				sprintf(output, msg_txt(sd,1758), itemdb_ename(stl->extended_vending));
				clif_messagecolor(&sd->bl, color_table[COLOR_RED], output, false, SELF);
				return;
			}

			pc_paycash(sd, (int)z, 0, LOG_TYPE_VENDING);
		}
		else {
			int loot_count = pc_countitem(*sd, stl->extended_vending, true, false);
 
			if(z > loot_count || z < 0 ) {
				char output[1024];
				sprintf(output, msg_txt(sd,1758), itemdb_ename(stl->extended_vending));
				clif_messagecolor(&sd->bl, color_table[COLOR_RED], output, false, SELF);
				return;
			}

			pc_delitem_sub(*sd, stl->extended_vending, (int)z, 0, 6, LOG_TYPE_NONE);
		}
	}
	else {
		pc_payzeny(sd, (int)z, LOG_TYPE_VENDING, sd->status.char_id);
		achievement_update_objective(sd, AG_SPEND_ZENY, 1, (int)z);
	}

	struct mail_message msg_buyer = {};
	msg_buyer.dest_id = sd->status.char_id;
	safestrncpy( msg_buyer.send_name, msg_txt(sd,1956), NAME_LENGTH );
	safestrncpy( msg_buyer.title, msg_txt(sd,1959), MAIL_TITLE_LENGTH );

	msg_buyer.status = MAIL_NEW;
	msg_buyer.type = MAIL_INBOX_NORMAL;
	msg_buyer.timestamp = time( nullptr );

	struct mail_message msg_vendor = {};
	msg_vendor.dest_id = stl->char_id;
	msg_vendor.zeny = 0;
	safestrncpy( msg_vendor.send_name, msg_txt(nullptr,1956), NAME_LENGTH );
	safestrncpy( msg_vendor.title, msg_txt(nullptr,1958), MAIL_TITLE_LENGTH );

	msg_vendor.status = MAIL_NEW;
	msg_vendor.type = MAIL_INBOX_NORMAL;
	msg_vendor.timestamp = time( nullptr );

	char timestring[23];
	time_t curtime;
	time(&curtime);
	strftime(timestring, 22, msg_txt(sd, 1976), localtime(&curtime));

	std::ostringstream stream;
	stream << msg_txt(nullptr, 1968) << " " << timestring << "\r\n\r\n";

	uint32 total_price = 0;
	std::shared_ptr<item_data> id = nullptr;

	for( i = 0; i < count; i++ ) {
		short amount = *(uint16*)(data + 4*i + 0);
		short idx    = *(uint16*)(data + 4*i + 2);
		uint32 price = 0, total = 0;
		idx -= 1;
		z = 0.; // zeny counter

		stl->items_inventory[idx].amount -= amount;

		price = stl->price[idx];
		total = (uint32)price * amount;

		memcpy(&msg_buyer.item[i],&stl->items_inventory[idx],sizeof(struct item));
		msg_buyer.item[i].amount = amount;

		id = item_db.find(stl->items_inventory[idx].nameid);
		stream << msg_txt(nullptr, 1970) << " " << item_db.create_item_link(id).c_str() << "\r\n";

		if( battle_config.extended_stall_enable && stl->extended_vending && stl->extended_vending != battle_config.item_zeny ) {
			id = item_db.find(stl->extended_vending);
			stream << msg_txt(nullptr, 1971) << " " << price << " " << item_db.create_item_link(id).c_str() <<"\r\n";
			stream << msg_txt(nullptr, 1972) << " " << amount <<"\r\n";
			stream << msg_txt(nullptr, 1973) << " " << total << item_db.create_item_link(id).c_str() << "\r\n";
		}
		else {
			stream << msg_txt(nullptr, 1971) << " " << price << "z\r\n";
			stream << msg_txt(nullptr, 1972) << " " << amount <<"\r\n";
			stream << msg_txt(nullptr, 1973) << " " << total << "z\r\n";
		}

		total_price += total;
	}

	id = item_db.find(stl->extended_vending);
	if( battle_config.extended_stall_enable ) {
		if( stl->extended_vending ) {
			char output[150];
			sprintf(output, msg_txt(nullptr, 1755), item_db.create_item_link(id));
			stream << "\r\n" << output << "\r\n";
		}

		if( battle_config.extended_stall_showtotal ) {
			int tax = total_price - (total_price * STALL_TAX / 100);
			if( stl->extended_vending )
				stream << "\r\n" << msg_txt(nullptr, 1974) << " " << total_price - tax << " " << item_db.create_item_link(id).c_str() << "\r\n";
			else
				stream << "\r\n" << msg_txt(nullptr, 1974) << " " << total_price - tax << "z\r\n";
			total_price = tax;
		}
	}
	stream << "\0";

	msg_vendor.zeny = total_price;

	// Save in Bank Item
	if( battle_config.extended_stall_enable ) {
		if( vending_update_bank(stl->char_id, VENDING_BANK_STALL_SELL, stl->extended_vending, msg_vendor.zeny) ) {
			msg_vendor.zeny = 0;
			map_session_data *tsd = map_charid2sd(stl->char_id);
			if( tsd && !tsd->state.autotrade )
				npc_event(tsd, "Vending System#bank::OnPayment", 0);
		}
	}

	stream.str().resize(MAIL_BODY_LENGTH);
	safestrncpy( msg_buyer.body, const_cast<char*>(stream.str().c_str()), MAIL_BODY_LENGTH );
	if(!intif_Mail_send( 0, &msg_buyer )){
		stall_mail_db.push_back(msg_buyer);
	}

	safestrncpy( msg_vendor.body, const_cast<char*>(stream.str().c_str()), MAIL_BODY_LENGTH );
	if(!intif_Mail_send( 0, &msg_vendor )){
		stall_mail_db.push_back(msg_vendor);
	}

	bool remain_items = false;
	for( i = 0; i < stl->vend_num; i++ ){
		if(stl->items_inventory[i].amount > 0){
			remain_items = true;
			break;
		}
	}

	//Always save BOTH: customer (buyer) and vender
	if( save_settings&CHARSAVE_VENDING ) {
		chrif_save(sd, CSAVE_INVENTORY|CSAVE_CART);
		if(!remain_items){
			stall_remove(stl);
		} else
			stall_vending_save(stl);
	}
}

/**
 * Sell item(s) to a buying a stall
 * @param sd : buyer player session
 * @param aid : char id of vender
 * @param uid : stall unique id
 * @param data : items data who would like to purchase \n
 *	data := {<index>.w <amount>.w }[count]
 * @param count : number of different items he's trying to buy
 */
void stall_buying_purchasereq(map_session_data* sd, int aid, int uid, const struct PACKET_CZ_REQ_TRADE_BUYING_STORE_sub* itemlist, unsigned int count )
{
	int zeny = 0;
	struct s_stall_data* stl = map_id2st(uid);

	nullpo_retv(sd);
	if( stl == NULL )
		return; // invalid shop

	if(!stall_isStallOpen(stl->char_id)){
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,1951), false, SELF);
		return;
	}

	if( stl->vender_id != uid || stl->char_id != aid ) { // shop has changed
		clif_buyingstore_trade_failed_seller(sd, BUYINGSTORE_TRADE_SELLER_FAILED, 0);
		return;
	}

	if( !battle_config.feature_buying_store || pc_istrading(sd) )
	{// not allowed to sell
		clif_buyingstore_trade_failed_seller(sd, BUYINGSTORE_TRADE_SELLER_FAILED, 0);
		return;
	}

	if( !pc_can_give_items(sd) )
	{// custom: GM is not allowed to sell
		clif_messagecolor(&sd->bl, color_table[COLOR_RED], msg_txt(sd,246), false, SELF);
		clif_buyingstore_trade_failed_seller(sd, BUYINGSTORE_TRADE_SELLER_FAILED, 0);
		return;
	}

	if( !searchstore_queryremote(sd, stl->char_id) && (sd->bl.m != stl->bl.m || !check_distance_bl(&sd->bl, &stl->bl, AREA_SIZE)) ){
		clif_buyingstore_trade_failed_seller(sd, BUYINGSTORE_TRADE_SELLER_FAILED, 0);
		return; // shop too far away
	}

	if( count < 1 || count > MAX_STALL_SLOT || count > stl->vend_num )
		return; // invalid amount of purchased items

	// some checks
	for( int i = 0; i < count; i++ ){
		const struct PACKET_CZ_REQ_TRADE_BUYING_STORE_sub* item = &itemlist[i];

		// duplicate check. as the client does this too, only malicious intent should be caught here
		for( int k = 0; k < i; k++ ){
			// duplicate
			if( itemlist[k].index == item->index && k != i ){
				ShowWarning( "stall_buying_purchasereq: Found duplicate item on selling list (prevnameid=%u, prevamount=%hu, nameid=%u, amount=%hu, account_id=%d, char_id=%d).\n", itemlist[k].itemId, itemlist[k].amount, item->itemId, item->amount, sd->status.account_id, sd->status.char_id );
				clif_buyingstore_trade_failed_seller( sd, BUYINGSTORE_TRADE_SELLER_FAILED, item->itemId );
				return;
			}
		}

		int index = item->index - 2; // TODO: clif::server_index

		if( item->amount <= 0 )
			return;

		// invalid input
		if( index < 0 || index >= ARRAYLENGTH( sd->inventory.u.items_inventory ) || sd->inventory_data[index] == NULL || sd->inventory.u.items_inventory[index].nameid != item->itemId || sd->inventory.u.items_inventory[index].amount < item->amount ){
			clif_buyingstore_trade_failed_seller( sd, BUYINGSTORE_TRADE_SELLER_FAILED, item->itemId );
			return;
		}

		// non-tradable item
		if( sd->inventory.u.items_inventory[index].expire_time || ( sd->inventory.u.items_inventory[index].bound && !pc_can_give_bounded_items( sd ) ) || memcmp( sd->inventory.u.items_inventory[index].card, buyingstore_blankslots, sizeof( buyingstore_blankslots ) ) ){
			clif_buyingstore_trade_failed_seller( sd, BUYINGSTORE_TRADE_SELLER_FAILED, item->itemId );
			return;
		}

		int listidx;
		for(int k = 0 ; k < stl->vend_num; k++){
			if(item->itemId == stl->itemId[k]){
				listidx = k;
				break;
			}
		}

		// items has been sell by anyone else
		if( stl->amount[listidx] <= 0 || stl->amount[listidx] < item->amount){
			clif_buyingstore_trade_failed_seller( sd, BUYINGSTORE_TRADE_SELLER_COUNT, item->itemId );
			return;
		}

		zeny += item->amount * stl->price[listidx];
	}

	struct mail_message msg_vendor = {};
	msg_vendor.dest_id = sd->status.char_id;
	msg_vendor.zeny = 0;
	safestrncpy( msg_vendor.send_name, msg_txt(sd,1956), NAME_LENGTH );
	safestrncpy( msg_vendor.title, msg_txt(sd,1958), MAIL_TITLE_LENGTH );

	msg_vendor.status = MAIL_NEW;
	msg_vendor.type = MAIL_INBOX_NORMAL;
	msg_vendor.timestamp = time( nullptr );

	struct mail_message msg_buyer = {};
	msg_buyer.dest_id = stl->char_id;
	safestrncpy( msg_buyer.send_name, msg_txt(sd,1956), NAME_LENGTH );
	safestrncpy( msg_buyer.title, msg_txt(nullptr,1959), MAIL_TITLE_LENGTH );

	msg_buyer.status = MAIL_NEW;
	msg_buyer.type = MAIL_INBOX_NORMAL;
	msg_buyer.timestamp = time( nullptr );

	char timestring[23];
	time_t curtime;
	time(&curtime);
	strftime(timestring, 22, msg_txt(sd,1976), localtime(&curtime));

	std::ostringstream stream;
	stream << msg_txt(nullptr,1969) << " " << timestring << "\r\n";
	stream << "\r\n" << msg_txt(nullptr,1965) << "\r\n\r\n";

	// process item list
	uint32 total_price = 0;
	std::shared_ptr<item_data> id = nullptr;
	for( int i = 0; i < count; i++ ){
		const struct PACKET_CZ_REQ_TRADE_BUYING_STORE_sub* item = &itemlist[i];

		int listidx;
		for(int k = 0 ; k < stl->vend_num; k++){
			if(item->itemId == stl->itemId[k]){
				listidx = k;
				break;
			}
		}

		int index = item->index - 2; // TODO: clif::server_index

		// move item
		memcpy(&msg_buyer.item[i],&sd->inventory.u.items_inventory[index],sizeof(struct item));
		msg_buyer.item[i].amount = item->amount;

		pc_delitem(sd, index, item->amount, 0, 0, LOG_TYPE_BUYING_STORE);
		stl->amount[listidx] -= item->amount;

		std::shared_ptr<item_data> id = item_db.find(item->itemId);


		stream << msg_txt(nullptr, 1970) << " " << item_db.create_item_link(id).c_str() << "\r\n";

		if( battle_config.extended_stall_enable && stl->extended_vending && stl->extended_vending != battle_config.item_zeny ) {
			id = item_db.find(stl->extended_vending);
			stream << msg_txt(nullptr, 1971) << " " << stl->price[listidx] << " " << item_db.create_item_link(id).c_str() <<"\r\n";
			stream << msg_txt(nullptr, 1972) << " " << item->amount <<"\r\n";
			stream << msg_txt(nullptr, 1973) << " " << stl->price[listidx] * item->amount << item_db.create_item_link(id).c_str() << "\r\n";
		}
		else {
			stream << msg_txt(nullptr, 1971) << " " << stl->price[listidx] << "z\r\n";
			stream << msg_txt(nullptr, 1972) << " " << item->amount <<"\r\n";
			stream << msg_txt(nullptr, 1973) << " " << stl->price[listidx] * item->amount << "z\r\n";
		}

		msg_vendor.zeny += item->amount * stl->price[listidx];
		total_price += item->amount * stl->price[listidx];
	}

	id = item_db.find(stl->extended_vending);
	if( battle_config.extended_stall_enable ) {
		if( stl->extended_vending ) {
			char output[150];
			sprintf(output, msg_txt(nullptr, 1755), item_db.create_item_link(id));
			stream << "\r\n" << output << "\r\n";
		}

		if( battle_config.extended_stall_showtotal ) {
			if( stl->extended_vending )
				stream << "\r\n" << msg_txt(nullptr, 1974) << " " << total_price << " " << item_db.create_item_link(id).c_str() << "\r\n";
			else
				stream << "\r\n" << msg_txt(nullptr, 1974) << " " << total_price << "z\r\n";
		}
	}
	stream << "\0";

	// Save in Bank Item
	if( battle_config.extended_stall_enable && stl->extended_vending ) {
		if( vending_update_bank(sd->status.char_id, VENDING_BANK_STALL_BUYINGSTORE_OK, stl->extended_vending, msg_vendor.zeny) ) {
			msg_vendor.zeny = 0;
			npc_event(sd, "Vending System#bank::OnPayment", 0);
		}
	}

	stream.str().resize(MAIL_BODY_LENGTH);
	safestrncpy( msg_vendor.body, const_cast<char*>(stream.str().c_str()), MAIL_BODY_LENGTH );
	if(!intif_Mail_send( 0, &msg_vendor )){
		stall_mail_db.push_back(msg_vendor);
	}

	safestrncpy( msg_buyer.body, const_cast<char*>(stream.str().c_str()), MAIL_BODY_LENGTH );
	if(!intif_Mail_send( 0, &msg_buyer )){
		stall_mail_db.push_back(msg_buyer);
	}

	bool remain_items = false;
	for( int i = 0; i < stl->vend_num; i++ ){
		if(stl->amount[i] > 0){
			remain_items = true;
			break;
		}
	}

	//Always save BOTH: customer (buyer) and vender
	if( save_settings&CHARSAVE_VENDING ) {
		chrif_save(sd, CSAVE_INVENTORY|CSAVE_CART);
		if(!remain_items){
			stall_remove(stl);
		} else
			stall_buying_save(stl);
	}
}

void stall_close(map_session_data* sd){
	auto itStalls = std::find_if(stall_db.begin(), stall_db.end(), [&](s_stall_data *const & itstl) {
						return sd->status.char_id == itstl->char_id;
					});

	if(itStalls != stall_db.end()){
		switch((*itStalls)->type){
			case 0:
				stall_vending_getbackitems(*itStalls);
				clif_stall_ui_close(sd,100,STALLSTORE_OK);
				break;
			case 1:
				stall_buying_getbackzeny(*itStalls);
				clif_stall_ui_close(sd,101,STALLSTORE_OK);
				break;
		}

		stall_remove(*itStalls);
	}
}

void stall_close_from_gm(uint32 vender_id, bool report){
	if( report )
		ShowError("stall_close from gm \n");
	auto itStalls = std::find_if(stall_db.begin(), stall_db.end(), [&](s_stall_data *const & itstl) {
						return vender_id == itstl->vender_id;
					});

	if(itStalls != stall_db.end()){
		switch((*itStalls)->type){
			case 0:
				stall_vending_getbackitems(*itStalls);
				break;
			case 1:
				stall_buying_getbackzeny(*itStalls);
				break;
		}

		stall_remove(*itStalls);
	}
}

void stall_vending_save(struct s_stall_data* stl){
	for(int i = 0; i < stl->vend_num; i++){
		if( Sql_Query( mmysql_handle, "UPDATE `%s` SET `amount` = %d WHERE `stalls_id` = %d AND `index` = %d;",
			stalls_vending_items_table, stl->items_inventory[i].amount, stl->vender_id, i) != SQL_SUCCESS ) {
			Sql_ShowDebug(mmysql_handle);
		}
	}
}

void stall_buying_save(struct s_stall_data* stl){
	for(int i = 0; i < stl->vend_num; i++){
		if( Sql_Query( mmysql_handle, "UPDATE `%s` SET `amount` = %d WHERE `stalls_id` = %d AND `nameid` = %d;",
			stalls_buying_items_table, stl->amount[i], stl->vender_id, stl->itemId[i]) != SQL_SUCCESS ) {
			Sql_ShowDebug(mmysql_handle);
		}
	}
}

void stall_vending_getbackitems(struct s_stall_data* stl){
	struct mail_message msg_vendor = {};

	msg_vendor.dest_id = stl->char_id;
	safestrncpy( msg_vendor.send_name, msg_txt(nullptr,1956), NAME_LENGTH );
	safestrncpy( msg_vendor.title, msg_txt(nullptr,1957), MAIL_TITLE_LENGTH );

	msg_vendor.status = MAIL_NEW;
	msg_vendor.type = MAIL_INBOX_NORMAL;
	msg_vendor.timestamp = time( nullptr );

	char timestring[23];
	time_t curtime;
	time(&curtime);
	strftime(timestring, 22, msg_txt(nullptr,1976), localtime(&curtime));

	std::ostringstream stream;
	stream << msg_txt(nullptr,1969) << " " << timestring << "\r\n";
	stream << "\r\n" << msg_txt(nullptr,1965) << "\r\n\r\n";

	int mail_index = 0;
	std::shared_ptr<item_data> id;
	for( int i = 0; i < stl->vend_num; i++ ) {
		if(stl->items_inventory[i].amount > 0){
			memcpy(&msg_vendor.item[mail_index],&stl->items_inventory[i],sizeof(struct item));
			msg_vendor.item[mail_index].amount = stl->items_inventory[i].amount;

			id = item_db.find(stl->items_inventory[i].nameid);
			stream << msg_txt(nullptr,1966) << " " << id->ename.c_str() << "\r\n";
			stream << msg_txt(nullptr,1967) << " " << stl->items_inventory[i].amount << "\r\n";
			mail_index++;
		}
	}

	if( battle_config.extended_stall_enable && stl->extended_vending ) {
		char output[150];
		id = item_db.find(stl->extended_vending);
		sprintf(output, msg_txt(nullptr, 1755), id->ename.c_str());
		stream << "\r\n" << output << "\r\n";
	}
	stream << "\0";

	stream.str().resize(MAIL_BODY_LENGTH);
	safestrncpy( msg_vendor.body, const_cast<char*>(stream.str().c_str()), MAIL_BODY_LENGTH );
	if(!intif_Mail_send( 0, &msg_vendor )){
		stall_mail_db.push_back(msg_vendor);
	}
}

void stall_buying_getbackzeny(struct s_stall_data* stl){
	struct mail_message msg_buyer = {};

	msg_buyer.dest_id = stl->char_id;
	safestrncpy( msg_buyer.send_name, msg_txt(nullptr, 1956), NAME_LENGTH );
	safestrncpy( msg_buyer.title, msg_txt(nullptr, 1960), MAIL_TITLE_LENGTH );

	msg_buyer.status = MAIL_NEW;
	msg_buyer.type = MAIL_INBOX_NORMAL;
	msg_buyer.timestamp = time( nullptr );

	char timestring[23];
	time_t curtime;
	time(&curtime);
	strftime(timestring, 22, msg_txt(nullptr, 1976), localtime(&curtime));

	std::ostringstream stream;
	stream << msg_txt(nullptr, 1969) << " " << timestring << "\r\n";

	for( int i = 0; i < stl->vend_num; i++ ) {
		uint32 price = stl->amount[i] * stl->price[i];
		if(stl->price[i] > MAX_ZENY) // not tax if not bought
			price = price * (100 + STALL_TAX) / 100;
		msg_buyer.zeny += price;
	}

	if( battle_config.extended_stall_enable && stl->extended_vending ) {
		std::shared_ptr<item_data> id = item_db.find(stl->extended_vending);
		if( vending_update_bank(stl->char_id, VENDING_BANK_STALL_BUYINGSTORE_FAIL, stl->extended_vending, msg_buyer.zeny) ) {
			map_session_data *sd = map_charid2sd(stl->char_id);
			if( sd ) {
				stream << "\r\n" << msg_txt(sd,1975) << " " << msg_buyer.zeny << " " << item_db.create_item_link(id) << "\r\n";
				if( !sd->state.autotrade )
					npc_event(sd, "Vending System#bank::OnPayment", 0);
			}
			else
				stream << "\r\n" << msg_txt(nullptr,1975) << " " << msg_buyer.zeny << " " << item_db.create_item_link(id) << "\r\n";
		}
		else {
			stream << "\r\n" << msg_txt(nullptr, 1975) << " " << msg_buyer.zeny << "z\r\n";
		}
		msg_buyer.zeny = 0;
	}
	else
		stream << "\r\n" << msg_txt(nullptr, 1975) << " " << msg_buyer.zeny << "z\r\n";
	
	stream << "\0";

	stream.str().resize(MAIL_BODY_LENGTH);
	safestrncpy( msg_buyer.body, const_cast<char*>(stream.str().c_str()), MAIL_BODY_LENGTH );
	if(!intif_Mail_send( 0, &msg_buyer )){
		stall_mail_db.push_back(msg_buyer);
	}
}

void stall_remove(struct s_stall_data* stl){
	if( Sql_Query( mmysql_handle, "DELETE FROM `%s` WHERE `id` = %d;", stalls_table, stl->vender_id ) != SQL_SUCCESS ) {
			Sql_ShowDebug(mmysql_handle);
	}
	switch(stl->type){
		case 0:
			if( Sql_Query( mmysql_handle, "DELETE FROM `%s` WHERE `stalls_id` = %d;", stalls_vending_items_table, stl->vender_id ) != SQL_SUCCESS ) {
				Sql_ShowDebug(mmysql_handle);
			}
			break;
		case 1:
			if( Sql_Query( mmysql_handle, "DELETE FROM `%s` WHERE `stalls_id` = %d;", stalls_buying_items_table, stl->vender_id ) != SQL_SUCCESS ) {
				Sql_ShowDebug(mmysql_handle);
			}
			break;
	}
	if(stl->timer != INVALID_TIMER )
		delete_timer(stl->timer, stall_timeout);

	stall_db.erase(
	std::remove_if(stall_db.begin(), stall_db.end(), [&](s_stall_data * const & itstl) {
		return stl->vender_id == itstl->vender_id;
	}),
	stall_db.end());

	clif_clearunit_area(&stl->bl,CLR_OUTSIGHT);
	map_delblock(&stl->bl);
	map_freeblock(&stl->bl);
}

TIMER_FUNC(stall_timeout) {
	struct s_stall_data* stl = map_id2st(id);
	nullpo_ret(stl);

	// Remove the current timer
	stl->timer = INVALID_TIMER;

	auto itStalls = std::find_if(stall_db.begin(), stall_db.end(), [&](s_stall_data *const & itstl) {
						return stl->vender_id == itstl->vender_id;
					});

	if(itStalls != 	stall_db.end()){
		switch((*itStalls)->type){
			case 0:
				stall_vending_getbackitems(*itStalls);
				break;
			case 1:
				stall_buying_getbackzeny(*itStalls);
				break;
		}
		stall_remove(*itStalls);
	}

	return 0;
}

/**
 * Searches for all items in a stall, that match given ids, price and possible cards.
 * @param sd : The vender session to search into
 * @param s : parameter of the search (see s_search_store_search)
 * @return Whether or not the search should be continued.
 */
bool stall_searchall(map_session_data* sd, const struct s_search_store_search* s, const struct s_stall_data* stl, short type)
{
	int c, slot;
	unsigned int cidx;

	if( !stl->type == type ) // not good type
		return true;

	if(stl->type == 0){
		for( int j = 0; j < s->item_count; j++ ) {
			for( int i = 0; i < stl->vend_num; i++ ) {
				if(stl->items_inventory[i].amount > 0
					&& s->itemlist[j].itemId == stl->items_inventory[i].nameid){

					if( s->min_price && s->min_price > stl->price[i] ) { // too low price
						continue;
					}

					if( s->max_price && s->max_price < stl->price[i] ) { // too high price
						continue;
					}

					if( s->card_count ) { // check cards
						if( itemdb_isspecial(stl->items_inventory[i].card[0]) ) { // something, that is not a carded
							continue;
						}
						slot = itemdb_slots(stl->items_inventory[i].nameid);

						for( c = 0; c < slot && stl->items_inventory[i].card[c]; c ++ ) {
							ARR_FIND( 0, s->card_count, cidx, s->cardlist[cidx].itemId == stl->items_inventory[i].card[c] );
							if( cidx != s->card_count ) { // found
								break;
							}
						}

						if( c == slot || !stl->items_inventory[i].card[c] ) { // no card match
							continue;
						}
					}

					// Check if the result set is full
					if( s->search_sd->searchstore.items.size() >= (unsigned int)battle_config.searchstore_maxresults ){
						return false;
					}

					std::shared_ptr<s_search_store_info_item> ssitem = std::make_shared<s_search_store_info_item>();

					ssitem->store_id = stl->vender_id;
					ssitem->account_id = stl->char_id;
					if( battle_config.extended_stall_enable && stl->extended_vending ) {
						char out_msg[1024];
						std::shared_ptr<item_data> item = item_db.find(stl->extended_vending);
						memset(out_msg, '\0', sizeof(out_msg));
						strcat(strcat(strcat(strcat(out_msg, "["), item->ename.c_str()), "] "), stl->message);
						safestrncpy(ssitem->store_name, out_msg, sizeof(ssitem->store_name));
					}
					else
						safestrncpy( ssitem->store_name, stl->message, sizeof( ssitem->store_name ) );
					ssitem->nameid = stl->items_inventory[i].nameid;
					ssitem->amount = stl->items_inventory[i].amount;
					ssitem->price = stl->price[i];
					for( int j = 0; j < MAX_SLOTS; j++ ){
						ssitem->card[j] = stl->items_inventory[i].card[j];
					}
					ssitem->refine = stl->items_inventory[i].refine;
					ssitem->enchantgrade = stl->items_inventory[i].enchantgrade;

					s->search_sd->searchstore.items.push_back( ssitem );
				}
			}
		}
	}
	else if(stl->type == 1){
		for( int j = 0; j < s->item_count; j++ ) {
			for( int i = 0; i < stl->vend_num; i++ ) {
				if(stl->amount[i] > 0
					&& s->itemlist[j].itemId == stl->itemId[i]){
					if( s->min_price && s->min_price > stl->price[i] ) { // too low price
						continue;
					}

					if( s->max_price && s->max_price < stl->price[i] ) { // too high price
						continue;
					}

					if( s->card_count ) { // check cards
						;// ignore cards, as there cannot be any
					}

					// Check if the result set is full
					if( s->search_sd->searchstore.items.size() >= (unsigned int)battle_config.searchstore_maxresults ){
						return false;
					}

					std::shared_ptr<s_search_store_info_item> ssitem = std::make_shared<s_search_store_info_item>();

					ssitem->store_id = stl->vender_id;
					ssitem->account_id = stl->char_id;
					if( battle_config.extended_stall_enable && stl->extended_vending ) {
						char out_msg[1024];
						std::shared_ptr<item_data> item = item_db.find(stl->extended_vending);
						memset(out_msg, '\0', sizeof(out_msg));
						strcat(strcat(strcat(strcat(out_msg, "["), item->ename.c_str()), "] "), stl->message);
						safestrncpy(ssitem->store_name, out_msg, sizeof(ssitem->store_name));
					}
					else
						safestrncpy( ssitem->store_name, stl->message, sizeof( ssitem->store_name ) );
					ssitem->nameid = stl->itemId[i];
					ssitem->amount = stl->amount[i];
					ssitem->price = stl->price[i];
					for( int j = 0; j < MAX_SLOTS; j++ ){
						ssitem->card[j] = 0;
					}
					ssitem->refine = 0;
					ssitem->enchantgrade = 0;

					s->search_sd->searchstore.items.push_back( ssitem );
				}
			}
		}
	}

	return true;
}

TIMER_FUNC(stall_mail_queue){
	if(stall_mail_db.size() > 0){
		for (auto it = stall_mail_db.begin(); it != stall_mail_db.end(); it++){
			// remove odd numbers
			if (intif_Mail_send( 0, &(*it) )){
				// Notice that the iterator is decremented after it is passed
				// to `erase()` but before `erase()` is executed
				stall_mail_db.erase(it--);
			}
		}
	}

	add_timer(gettick() + 60000, stall_mail_queue, 0, 0); // check queue every minute

	return 0;
}

TIMER_FUNC(stall_init){
	DBIterator *iter = NULL;
	struct s_stall_data *stl = NULL;
	int i;
	std::vector<int> stall_remove_list;

	if (Sql_Query(mmysql_handle,
		"SELECT `id`, `char_id`, `type`, `class`, `sex`, `map`, `x`, `y`,"
		"`title`, `hair`, `hair_color`, `body`, `weapon`, `shield`, `head_top`, `head_mid`, `head_bottom`,"
		"`clothes_color`, `name`, `expire_time`, `extended_vending_item` "
		"FROM `%s` ",
		stalls_table ) != SQL_SUCCESS )
	{
		Sql_ShowDebug(mmysql_handle);
		return 1;
	}

	// Init each stalls data
	while (SQL_SUCCESS == Sql_NextRow(mmysql_handle)) {
		size_t len;
		char* data;
		stl = NULL;
		stl = (struct s_stall_data*)aCalloc(1, sizeof(struct s_stall_data));
		Sql_GetData(mmysql_handle, 0, &data, NULL); stl->vender_id = atoi(data);
		Sql_GetData(mmysql_handle, 1, &data, NULL); stl->char_id = atoi(data); stl->bl.id = stl->vender_id;
		Sql_GetData(mmysql_handle, 2, &data, NULL); stl->type = atoi(data);
		Sql_GetData(mmysql_handle, 3, &data, NULL); stl->vd.class_ = atoi(data);
		Sql_GetData(mmysql_handle, 4, &data, NULL); stl->vd.sex = (data[0] == 'F') ? SEX_FEMALE : SEX_MALE;
		Sql_GetData(mmysql_handle, 5, &data, &len); stl->mapindex = mapindex_name2id(data); stl->bl.m = map_mapindex2mapid(stl->mapindex);
		Sql_GetData(mmysql_handle, 6, &data, NULL); stl->bl.x = atoi(data);
		Sql_GetData(mmysql_handle, 7, &data, NULL); stl->bl.y = atoi(data);
		Sql_GetData(mmysql_handle, 8, &data, &len); safestrncpy(stl->message, data, zmin(len + 1, MESSAGE_SIZE));
		Sql_GetData(mmysql_handle, 9, &data, NULL); stl->vd.hair_style = atoi(data);
		Sql_GetData(mmysql_handle, 10, &data, NULL); stl->vd.hair_color = atoi(data);
		Sql_GetData(mmysql_handle, 11, &data, NULL); stl->vd.body_style = atoi(data);
		Sql_GetData(mmysql_handle, 12, &data, NULL); stl->vd.weapon = atoi(data);
		Sql_GetData(mmysql_handle, 13, &data, NULL); stl->vd.shield = atoi(data);
		Sql_GetData(mmysql_handle, 14, &data, NULL); stl->vd.head_top = atoi(data);
		Sql_GetData(mmysql_handle, 15, &data, NULL); stl->vd.head_mid = atoi(data);
		Sql_GetData(mmysql_handle, 16, &data, NULL); stl->vd.head_bottom = atoi(data);
		Sql_GetData(mmysql_handle, 17, &data, NULL); stl->vd.cloth_color = atoi(data);
		Sql_GetData(mmysql_handle, 18, &data, &len); safestrncpy(stl->name, data, zmin(len + 1, MESSAGE_SIZE));
		Sql_GetData(mmysql_handle, 19, &data, NULL); stl->expire_time = strtoul(data, nullptr, 10);
		Sql_GetData(mmysql_handle, 20, &data, NULL); stl->extended_vending = atoi(data); // [RomuloSM] Extended Stall
		stl->bl.type = BL_STALL;
		stl->timer = INVALID_TIMER;
		stall_db.push_back(stl);
	}

	for (auto& itStalls : stall_db){
		int item_count = 0;

		switch(itStalls->type){
			case 0:{
				if (Sql_Query(mmysql_handle,
					"SELECT `nameid`,`amount`,`identify`,`refine`,`attribute`"
					",`card0`,`card1`,`card2`,`card3`"
					",`option_id0`,`option_val0`,`option_parm0`,`option_id1`,`option_val1`,`option_parm1`,`option_id2`,`option_val2`,`option_parm2`,`option_id3`,`option_val3`,`option_parm3`,`option_id4`,`option_val4`,`option_parm4`"
					",`expire_time`,`bound`,`unique_id`,`enchantgrade`,`price` "
					"FROM `%s` WHERE stalls_id = %d ",
					stalls_vending_items_table, itStalls->vender_id ) != SQL_SUCCESS )
				{
					Sql_ShowDebug(mmysql_handle);
					return 1;
				}

				while (SQL_SUCCESS == Sql_NextRow(mmysql_handle)) {
					char* data;
					struct item item;
					Sql_GetData(mmysql_handle, 0, &data, NULL); item.nameid = strtoul(data, nullptr, 10);
					Sql_GetData(mmysql_handle, 1, &data, NULL); item.amount = atoi(data);
					Sql_GetData(mmysql_handle, 2, &data, NULL); item.identify = atoi(data);
					Sql_GetData(mmysql_handle, 3, &data, NULL); item.refine = atoi(data);
					Sql_GetData(mmysql_handle, 4, &data, NULL); item.attribute = atoi(data);
					for( i = 0; i < MAX_SLOTS; ++i ){
						Sql_GetData(mmysql_handle, 5+i, &data, NULL);
						item.card[i] = atoi(data);
					}
					for( i = 0; i < MAX_ITEM_RDM_OPT; ++i ) {
						Sql_GetData(mmysql_handle, 5+MAX_SLOTS+i*3, &data, NULL); item.option[i].id = atoi(data);
						Sql_GetData(mmysql_handle, 6+MAX_SLOTS+i*3, &data, NULL); item.option[i].value = atoi(data);
						Sql_GetData(mmysql_handle, 7+MAX_SLOTS+i*3, &data, NULL); item.option[i].param = atoi(data);
					}
					Sql_GetData(mmysql_handle, 5+MAX_SLOTS+MAX_ITEM_RDM_OPT*3, &data, NULL); item.expire_time = strtoul(data, nullptr, 10);
					Sql_GetData(mmysql_handle, 6+MAX_SLOTS+MAX_ITEM_RDM_OPT*3, &data, NULL); item.bound = atoi(data);
					Sql_GetData(mmysql_handle, 7+MAX_SLOTS+MAX_ITEM_RDM_OPT*3, &data, NULL); item.unique_id = strtoull(data, nullptr, 10);;
					Sql_GetData(mmysql_handle, 8+MAX_SLOTS+MAX_ITEM_RDM_OPT*3, &data, NULL); item.enchantgrade = atoi(data);

					Sql_GetData(mmysql_handle, 9+MAX_SLOTS+MAX_ITEM_RDM_OPT*3, &data, NULL); itStalls->price[item_count] = atoi(data);
					memcpy(&itStalls->items_inventory[item_count],&item,sizeof(struct item));
					item_count++;
				}
			} break;
			case 1: {
				if (Sql_Query(mmysql_handle,
					"SELECT `nameid`,`amount`,`price` "
					"FROM `%s` WHERE stalls_id = %d ",
					stalls_buying_items_table, itStalls->vender_id ) != SQL_SUCCESS )
				{
					Sql_ShowDebug(mmysql_handle);
					return 1;
				}

				while (SQL_SUCCESS == Sql_NextRow(mmysql_handle)) {
					char* data;

					Sql_GetData(mmysql_handle, 0, &data, NULL); itStalls->itemId[item_count] = strtoul(data, nullptr, 10);
					Sql_GetData(mmysql_handle, 1, &data, NULL); itStalls->amount[item_count] = atoi(data);
					Sql_GetData(mmysql_handle, 2, &data, NULL); itStalls->price[item_count] = atoi(data);

					item_count++;
				}
			} break;
		}
		itStalls->vend_num = item_count;
		long int remain_time = static_cast<long int>(itStalls->expire_time - time(NULL));

		if( item_count == 0 || (itStalls->expire_time && remain_time <= 0) ){

			if( Sql_Query( mmysql_handle, "DELETE FROM `%s` WHERE `id` = %d;", stalls_table, itStalls->vender_id ) != SQL_SUCCESS ) {
					Sql_ShowDebug(mmysql_handle);
			}

			stall_remove_list.push_back(itStalls->vender_id);

			if(remain_time < 0 && item_count > 0){
				switch(itStalls->type){
					case 0:
						stall_vending_getbackitems(itStalls);
						if( Sql_Query( mmysql_handle, "DELETE FROM `%s` WHERE `stalls_id` = %d;", stalls_vending_items_table, itStalls->vender_id ) != SQL_SUCCESS ) {
								Sql_ShowDebug(mmysql_handle);
						}
						break;
					case 1:
						stall_buying_getbackzeny(itStalls);
						if( Sql_Query( mmysql_handle, "DELETE FROM `%s` WHERE `stalls_id` = %d;", stalls_buying_items_table, itStalls->vender_id ) != SQL_SUCCESS ) {
								Sql_ShowDebug(mmysql_handle);
						}
						break;
				}
			}
			aFree(itStalls);
			continue;
		}

		if( itStalls->expire_time ) {
			itStalls->timer = add_timer(gettick() + (remain_time) * 1000,
				stall_timeout, itStalls->bl.id, 0);
		}

		if(map_addblock(&itStalls->bl))
			continue;
		status_change_init(&itStalls->bl);
		map_addiddb(&itStalls->bl);
	}

	if(stall_remove_list.size() > 0){
		for (auto& itStalls : stall_remove_list){
			stall_db.erase(
			std::remove_if(stall_db.begin(), stall_db.end(), [&](s_stall_data * const & itstl) {
				return itStalls == itstl->vender_id;
			}),
			stall_db.end());
		}
	}
	stall_remove_list.clear();

	ShowStatus("Done loading '" CL_WHITE "%d" CL_RESET "' vending stalls.\n", stall_db.size());

	return 0;
}

void do_init_stall(void)
{
	add_timer(gettick() + 1, stall_init, 0, 0); // need to delay for send mails if timeout because it doesn't see the char server up...
	add_timer(gettick() + 60000, stall_mail_queue, 0, 0); // check mail queue every minute in case something goes wrong with char server
}

void do_final_stall(void)
{
	if(stall_db.size() > 0){
		for (auto& i : stall_db){
			if(i != nullptr)
				aFree(i);
		}
	}

	stall_db.clear();
	stall_mail_db.clear();
}
