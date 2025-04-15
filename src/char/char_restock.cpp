// Copyright (c) RomuloSM
// Email: romulodevel@gmail.com
// Facebook: https://www.facebook.com/RomuloDevel
// rAthena: https://rathena.org/board/profile/6097-sbk_/

#ifndef _WIN32
#include <unistd.h>
#else
#include "../common/winapi.hpp"
#endif
#include "../common/cbasetypes.hpp"
#include "../common/sql.hpp"
#include "../common/mmo.hpp"

#include "char_restock.hpp"
#include "inter.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Restock System
bool char_restock_load(uint32 char_id, struct item *item)
{
	SqlStmt* stmt;
	struct item tmp_item;
	int i = 0;

	stmt = SqlStmt_Malloc(sql_handle);
	if( stmt == NULL )
	{
		SqlStmt_ShowDebug(stmt);
		return 0;
	}

	memset(&tmp_item, 0, sizeof(tmp_item));
	if (SQL_ERROR == SqlStmt_Prepare(stmt, "SELECT `nameid`, `amount` FROM `restock` WHERE `char_id`=? LIMIT %d", MAX_RESTOCK)
		|| SQL_ERROR == SqlStmt_BindParam(stmt, 0, SQLDT_INT, &char_id, 0)
		|| SQL_ERROR == SqlStmt_Execute(stmt)
		|| SQL_ERROR == SqlStmt_BindColumn(stmt, 0, SQLDT_UINT,  &tmp_item.nameid, 0, NULL, NULL)
		|| SQL_ERROR == SqlStmt_BindColumn(stmt, 1, SQLDT_SHORT, &tmp_item.amount, 0, NULL, NULL)
		) {
		SqlStmt_ShowDebug(stmt);
		SqlStmt_Free(stmt);
		return 0;
	}

	while (SQL_SUCCESS == SqlStmt_NextRow(stmt)) {
		memcpy(&item[i], &tmp_item, sizeof(tmp_item));
		i++;
	}
	SqlStmt_Free(stmt);
	return 1;
}

bool char_restock_save(uint32 char_id, struct mmo_charstatus* status)
{
	int i, errors = 0;

	if( SQL_ERROR == Sql_Query(sql_handle, "DELETE FROM `restock` WHERE `char_id`='%d'", char_id) )
	{
		Sql_ShowDebug(sql_handle);
		return 0;
	}

	for( i = 0; i < MAX_RESTOCK; i++ )
	{
		if( status->restock[i].nameid > 0 && status->restock[i].amount > 0 ) {
			if( SQL_ERROR == Sql_Query(sql_handle, "INSERT INTO `restock` (`char_id`, `nameid`, `amount`) VALUES (%d, %d, %d)", char_id, status->restock[i].nameid, status->restock[i].amount) ) {
				Sql_ShowDebug(sql_handle);
				errors++;
			}
		}
	}
	return errors ? false : true;
}
