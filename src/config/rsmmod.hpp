// Copyright (c) RomuloSM
// Email: romulodevel@gmail.com
// Facebook: https://www.facebook.com/RomuloDevel

#ifndef CONFIG_RSMMOD_HPP
#define CONFIG_RSMMOD_HPP

// Path of the rACSDMod relatedfiles.
#define RSMPATH "rsm-mod"

// Default Language
// Check that it is enabled in 'src/common/msg_conf.hpp' in the 'LANG_ENABLE' definition.
//	0: English (ENG)
//	1: Russkiy (RUS)
//	2: Espanol (SPN)
//	3: Deutsch (GRM)
//	4: Hanyu (CHN)
//	5: Bahasa Malaysia (MAL)
//	6: Bahasa Indonesia (IDN)
//	7: Francais (FRN)
//	8: Português Brasileiro (POR)
//	9: Thai (THA)
//#define MSG_CONF_DEFAULT 8

// Restock System
// Maximum item for restock
//#define MAX_RESTOCK 120

// Enable BG Expansion Shared Code
// Disabling this option and not disabling it in battle config can result in bad behavior of the emulator.
#define BATTLEGROUND_EXPANSION

// Enable Ranked System Shared Code
// Disabling this option and not disabling it in battle config can result in bad behavior of the emulator.
//#define RANKED_SYSTEM

// Enable Stuff System Shared Code
// Disabling this option and not disabling it in battle config can result in bad behavior of the emulator.
//#define STUFF_SYSTEM

// Enable Presence System Shared Code
// Disabling this option and not disabling it in battle config can result in bad behavior of the emulator.
//#define PRESENCE_SYSTEM

// Enable Auto Vend System Shared Code
// Disabling this option and not disabling it in battle config can result in bad behavior of the emulator.
//#define AUTOVEND_SYSTEM

// Extend name in party buff package
//#define PARTYBUFF_PACKET 4

// Enable Guild Ranking Shared Code
// Disabling this option and not disabling it in battle config can result in bad behavior of the emulator.
// Not Implemented!
//#define GUILD_RANKING

// Presence Points Variable
//#define PRESENCEPOINT_VAR "#PRESENCEPOINTS"

// Presence Tick Variable
//#define PRESENCETICK_VAR "#PRESENCETICK"

// Presence Rest Tick Variable
//#define PRESENCEREST_VAR "#PRESENCEREST"

// Pack Guild Label
// Enable Special Guild Enter or Leave Labels?
//#define GUILD_SCRIPT_LABEL

#ifdef GUILD_SCRIPT_LABEL
	// Label called when joining a Guild.
	#define GUILD_JOIN_LABEL "OnGuildEnter"

	// Label called when leaving a Guild.
	#define GUILD_LEAVE_LABEL "OnGuildLeave"

	// Label called when expelled from a Guild.
	#define GUILD_EXPULSION_LABEL "OnGuildExpulsion"
#endif

// Enable Gepard Unique ID?
// Check in Battleground Expansion, Presence System...
//#define GEPARD_UNIQUE_ID

// Event Room
//#define MAP_EVENTROOM "event_room"
//#define MAP_EVENTROOM_X 99
//#define MAP_EVENTROOM_Y 82

#endif
