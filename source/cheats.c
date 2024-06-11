#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <orbis/SaveData.h>
#include <cjson/cJSON.h>
#include <mxml.h>
#include <sqlite3.h>
#include <polarssl/aes.h>

#include "cheats.h"
#include "common.h"
#include "settings.h"
#include "util.h"

#define UTF8_CHAR_GROUP		"\xe2\x97\x86"
#define UTF8_CHAR_ITEM		"\xe2\x94\x97"
#define UTF8_CHAR_STAR		"\xE2\x98\x85"

#define CHAR_ICON_ZIP		"\x0C"
#define CHAR_ICON_COPY		"\x0B"
#define CHAR_ICON_SIGN		"\x06"
#define CHAR_ICON_USER		"\x07"
#define CHAR_ICON_LOCK		"\x08"
#define CHAR_ICON_WARN		"\x0F"

#define MC4_AES256CBC_KEY   "304c6528f659c766110239a51cl5dd9c"
#define MC4_AES256CBC_IV    "u@}kzW2u[u(8DWar"

static sqlite3* open_sqlite_db(const char* db_path)
{
	sqlite3 *db;
	static int init = -1;

	if (init > 0 || (init < 0 && (init = sqlite3_os_init()) != SQLITE_OK))
	{
		LOG("Error loading extension: %s", "orbis_rw");
		return NULL;
	}

	LOG("Opening '%s'...", db_path);
	if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, "orbis_rw") != SQLITE_OK)
	{
		LOG("Error open memvfs: %s", sqlite3_errmsg(db));
		return NULL;
	}

	return db;
}

void check_game_appdb(list_t* list)
{
	sqlite3* db;
	sqlite3_stmt *res;
	list_node_t* node;
	game_entry_t* item;

	if ((db = open_sqlite_db("/system_data/priv/mms/app.db")) == NULL)
	{
		LOG("Failed to open app.db");
		return;
	}

	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		char* query = sqlite3_mprintf("SELECT A.titleId, A.val, B.val FROM tbl_appinfo AS A INNER JOIN tbl_appinfo AS B"
			" WHERE A.key = 'APP_VER' AND B.key = 'VERSION' AND A.titleId = %Q AND B.titleId = %Q"
			" AND (((A.val >= B.val) AND A.val = %Q) OR ((A.val < B.val) AND B.val = %Q))",
			item->title_id, item->title_id, item->version, item->version);

		if (sqlite3_prepare_v2(db, query, -1, &res, NULL) == SQLITE_OK && sqlite3_step(res) == SQLITE_ROW)
		{
			LOG("Found game: %s %s", item->title_id, item->version);
			item->flags |= CHEAT_FLAG_OWNER;
		}
		if (startsWith(item->version, "mask") || startsWith(item->version, "all"))
		{
			char* query = sqlite3_mprintf("SELECT A.titleId, A.val, B.val FROM tbl_appinfo AS A INNER JOIN tbl_appinfo AS B"
							" WHERE A.key = 'APP_VER' AND B.key = 'VERSION' AND A.titleId = %Q AND B.titleId = %Q",
							item->title_id, item->title_id);
			if ((sqlite3_prepare_v2(db, query, -1, &res, NULL) == SQLITE_OK && sqlite3_step(res) == SQLITE_ROW))
			{
				LOG("Found game: %s %s", item->title_id, item->version);
				item->flags |= CHEAT_FLAG_OWNER;
			}
		}
		
		sqlite3_free(query);
	}

	sqlite3_close(db);
	return;
}

int orbis_SaveUmount(const char* mountPath)
{
	OrbisSaveDataMountPoint umount;

	memset(&umount, 0, sizeof(OrbisSaveDataMountPoint));
	strncpy(umount.data, mountPath, sizeof(umount.data));

	int32_t umountErrorCode = sceSaveDataUmount(&umount);
	
	if (umountErrorCode < 0)
		LOG("UMOUNT_ERROR (%X)", umountErrorCode);

	return (umountErrorCode == SUCCESS);
}

int orbis_UpdateSaveParams(const char* mountPath, const char* title, const char* subtitle, const char* details)
{
	OrbisSaveDataParam saveParams;
	OrbisSaveDataMountPoint mount;

	memset(&saveParams, 0, sizeof(OrbisSaveDataParam));
	memset(&mount, 0, sizeof(OrbisSaveDataMountPoint));

	strncpy(mount.data, mountPath, sizeof(mount.data));
	strncpy(saveParams.title, title, ORBIS_SAVE_DATA_TITLE_MAXSIZE);
	strncpy(saveParams.subtitle, subtitle, ORBIS_SAVE_DATA_SUBTITLE_MAXSIZE);
	strncpy(saveParams.details, details, ORBIS_SAVE_DATA_DETAIL_MAXSIZE);
	saveParams.mtime = time(NULL);

	int32_t setParamResult = sceSaveDataSetParam(&mount, ORBIS_SAVE_DATA_PARAM_TYPE_ALL, &saveParams, sizeof(OrbisSaveDataParam));
	if (setParamResult < 0) {
		LOG("sceSaveDataSetParam error (%X)", setParamResult);
		return 0;
	}

	return (setParamResult == SUCCESS);
}

/*
 * Function:		endsWith()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Checks to see if a ends with b
 * Arguments:
 *	a:				String
 *	b:				Potential end
 * Return:			pointer if true, NULL if false
 */
static char* endsWith(const char * a, const char * b)
{
	int al = strlen(a), bl = strlen(b);
    
	if (al < bl)
		return NULL;

	a += (al - bl);
	while (*a)
		if (toupper(*a++) != toupper(*b++)) return NULL;

	return (char*) (a - bl);
}

/*
 * Function:		readFile()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		reads the contents of a file into a new buffer
 * Arguments:
 *	path:			Path to file
 * Return:			Pointer to the newly allocated buffer
 */
char * readTextFile(const char * path, long* size)
{
	FILE *f = fopen(path, "rb");

	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fsize <= 0)
	{
		fclose(f);
		return NULL;
	}

	char * string = malloc(fsize + 1);
	fread(string, fsize, 1, f);
	fclose(f);

	string[fsize] = 0;
	if (size)
		*size = fsize;

	return string;
}

static code_entry_t* _createCmdCode(uint8_t type, const char* name, char code)
{
	code_entry_t* entry = (code_entry_t *)calloc(1, sizeof(code_entry_t));
	entry->type = type;
	entry->name = strdup(name);
	asprintf(&entry->codes, "%c", code);

	return entry;
}

/*
static option_entry_t* _initOptions(int count)
{
	option_entry_t* options = (option_entry_t*)malloc(sizeof(option_entry_t));

	options->id = 0;
	options->sel = -1;
	options->size = count;
	options->line = NULL;
	options->value = malloc (sizeof(char *) * count);
	options->name = malloc (sizeof(char *) * count);

	return options;
}

static option_entry_t* _createOptions(int count, const char* name, char value)
{
	option_entry_t* options = _initOptions(count);

	asprintf(&options->name[0], "%s %d", name, 0);
	asprintf(&options->value[0], "%c%c", value, 0);
	asprintf(&options->name[1], "%s %d", name, 1);
	asprintf(&options->value[1], "%c%c", value, 1);

	return options;
}
*/

static game_entry_t* createGameEntry(uint16_t flag, const char* name)
{
	game_entry_t* entry = (game_entry_t *)calloc(1, sizeof(game_entry_t));
	entry->flags = flag;
	entry->name = strdup(name);

	return entry;
}

int set_json_codes(game_entry_t* item)
{
	code_entry_t* cmd;

	LOG("Parsing \"%s\"...", item->path);

	char *buffer = readTextFile(item->path, NULL);
	cJSON *cheat = cJSON_Parse(buffer);

	if (!cheat)
	{
		LOG("JSON parse Error: %s\n", buffer);
		list_free(item->codes);
		free(buffer);

		return 0;
	}

	const cJSON *mod;
	const cJSON *mods = cJSON_GetObjectItemCaseSensitive(cheat, "mods");
	const cJSON *proc = cJSON_GetObjectItemCaseSensitive(cheat, "process");
	cJSON_ArrayForEach(mod, mods)
	{
		cJSON *mod_name = cJSON_GetObjectItemCaseSensitive(mod, "name");
		cJSON *mod_type = cJSON_GetObjectItemCaseSensitive(mod, "type");

		if (!cJSON_IsString(mod_name) || !cJSON_IsString(mod_type))
			continue;

		cmd = (code_entry_t *)calloc(1, sizeof(code_entry_t));
		cmd->type = PATCH_VIEW;
		cmd->name = strdup(mod_name->valuestring);
		cmd->file = strdup(cJSON_IsString(proc) ? proc->valuestring : "");
		
		const cJSON *mem = cJSON_GetObjectItemCaseSensitive(mod, "memory");
		char* code = cJSON_Print(mem);
		asprintf(&cmd->codes, "Path: %s\n\nCode: %s\n", item->path, code);
		free(code);

		LOG("Added '%s' (%s)", mod_name->valuestring, mod_type->valuestring);
		list_append(item->codes, cmd);
	}

	cJSON_Delete(cheat);
	free(buffer);

	return list_count(item->codes);
}

static const char* xml_whitespace_cb(mxml_node_t *node, int where)
{
	if (where == MXML_WS_AFTER_OPEN || where == MXML_WS_AFTER_CLOSE)
		return ("\n");

	return (NULL);
}

static const char* GetXMLAttr(mxml_node_t *node, const char *name)
{
	const char* AttrData = mxmlElementGetAttr(node, name);
	if (AttrData == NULL) AttrData = "";
	return AttrData;
}

char* mc4_decrypt(const char* data)
{
	uint8_t iv[16];
	size_t enc_size;
	aes_context aes_ctx;

	uint8_t* enc_data = dbg_base64_decode(data, &enc_size);
	if (!enc_data)
		return NULL;

	// check if size is multiple of 16 (AES block size)
	if (enc_size % 16)
	{
		enc_size = align_to_pow2(enc_size, 16);
		enc_data = realloc(enc_data, enc_size);
		LOG("AES: padding to 0x%X", enc_size);
	}

	memcpy(iv, MC4_AES256CBC_IV, sizeof(iv));
	aes_init(&aes_ctx);
	aes_setkey_dec(&aes_ctx, (uint8_t*) MC4_AES256CBC_KEY, 256);
	aes_crypt_cbc(&aes_ctx, AES_DECRYPT, enc_size, iv, enc_data, enc_data);

	// replace html entities in the decrypted XML data
	for (size_t i = 0; i < (enc_size - 3); i++)
	{
		if (strncmp((char*) &enc_data[i], "&lt;", 4) == 0)
			memcpy(enc_data + i, "<   ", 4);

		else if (strncmp((char*) &enc_data[i], "&gt;", 4) == 0)
			memcpy(enc_data + i, ">   ", 4);
	}

	return (char*) enc_data;
}

int set_shn_codes(game_entry_t* item)
{
	code_entry_t* cmd;

	LOG("Parsing %s...", item->path);
	char *buffer = readTextFile(item->path, NULL);
	mxml_node_t *node, *tree = NULL;

	if (buffer && (item->flags & CHEAT_FLAG_MC4))
	{
		char* dec_data = mc4_decrypt(buffer);
		free(buffer);
		buffer = dec_data;

		if (!dec_data)
		{
			LOG("Error: could not decrypt %s", item->path);
			list_free(item->codes);
			return 0;
		}

		cmd = _createCmdCode(PATCH_COMMAND, " " UTF8_CHAR_STAR " Decrypt MC4 file to XML " UTF8_CHAR_STAR, CMD_DECRYPT_MC4);
		list_append(item->codes, cmd);
	}

	/* parse the file and get the DOM */
	tree = mxmlLoadString(NULL, buffer, MXML_NO_CALLBACK);
	if (!tree)
	{
		LOG("XML: could not parse '%s'", item->path);
		free(buffer);

		if (item->flags & CHEAT_FLAG_MC4)
			return list_count(item->codes);

		list_free(item->codes);
		return 0;
	}

	node = mxmlFindElement(tree, tree, "Trainer", "Game", NULL, MXML_DESCEND);
	const char* author = GetXMLAttr(node, "Moder");
	const char* procfn = GetXMLAttr(node, "Process");

	/* Get the cheat element nodes */
	for (node = mxmlFindElement(tree, tree, "Cheat", "Text", NULL, MXML_DESCEND); node != NULL;
		node = mxmlFindElement(node, tree, "Cheat", "Text", NULL, MXML_DESCEND))
	{
		mxml_node_t *value = mxmlFindElement(node, node, "Cheatline", NULL, NULL, MXML_DESCEND);
		char* code = value ? mxmlSaveAllocString(node, &xml_whitespace_cb) : calloc(1, 1);

		cmd = (code_entry_t *)calloc(1, sizeof(code_entry_t));
		cmd->type = PATCH_VIEW;
		cmd->name = strdup(GetXMLAttr(node, "Text"));
		cmd->file = strdup(procfn);
		asprintf(&cmd->codes, "Path: %s\nAuthor: %s\n\n%s", item->path, author, code);
		free(code);

		LOG("Added '%s' (%d)", cmd->name, cmd->type);
		list_append(item->codes, cmd);
	}

	/* free the document */
	mxmlDelete(tree);
	free(buffer);

	return list_count(item->codes);
}

/*
 * Function:		ReadLocalCodes()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Reads an entire NCL file into an array of code_entry
 * Arguments:
 *	path:			Path to ncl
 *	_count_count:	Pointer to int (set to the number of codes within the ncl)
 * Return:			Returns an array of code_entry, null if failed to load
 */
int ReadCodes(game_entry_t * save)
{
	save->codes = list_alloc();
	list_append(save->codes, _createCmdCode(PATCH_COMMAND,
		(save->flags & CHEAT_FLAG_LOCKED) ? CHAR_ICON_LOCK " Enable Cheat File" : CHAR_ICON_LOCK "Disable Cheat File",
		CMD_TOGGLE_CHEAT));

	if (save->flags & CHEAT_FLAG_JSON)
		return set_json_codes(save);

	if (save->flags & (CHEAT_FLAG_SHN | CHEAT_FLAG_MC4))
		return set_shn_codes(save);

	save->codes = list_alloc();

	return list_count(save->codes);
}

/*
 * Function:		ReadOnlineCodes()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Downloads an entire NCL file into an array of code_entry
 * Arguments:
 *	filename:		File name ncl
 *	_count_count:	Pointer to int (set to the number of codes within the ncl)
 * Return:			Returns an array of code_entry, null if failed to load
 */
int ReadOnlineCodes(game_entry_t * game)
{
//	code_entry_t* item;
	char path[256];
	snprintf(path, sizeof(path), CHEATSMGR_LOCAL_CACHE "%s", strrchr(game->path, '/') + 1);

	if (file_exists(path) == SUCCESS)
	{
		struct stat stats;
		stat(path, &stats);
		// re-download if file is +1 day old
		if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL))
			http_download(game->path, "", path, 0);
	}
	else
	{
		if (!http_download(game->path, "", path, 0))
			return -1;
	}

	char *tmp = game->path;
	game->path = path;
	game->codes = list_alloc();

	if (game->flags & CHEAT_FLAG_JSON)
		set_json_codes(game);

	if (game->flags & (CHEAT_FLAG_SHN | CHEAT_FLAG_MC4))
		set_shn_codes(game);

	game->path = tmp;

	return (list_count(game->codes));
}

static uint32_t find_zip(list_t *item_list, const char *prefix, const char *name, const char *path, const char cmd_type)
{
	code_entry_t *cmd;
	uint32_t file_count = 0;
	char cmdName[256] = {0};
	struct dirent *dir;
	DIR *d = opendir(path);

	if (!d)
	{
		LOG("DIR* to %s is null.", path);
		return 0;
	}

	LOG("Searching zip path: %s", path);
	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0 ||
			!startsWith(dir->d_name, prefix) || !endsWith(dir->d_name, ".zip"))
			continue;

		file_count++;
		snprintf(cmdName, sizeof(cmdName), "Update %s from %s (%s)", name, (startsWith(path, CHEATSMGR_PATH)) ? "HDD" : "USB", dir->d_name);
		cmd = _createCmdCode(PATCH_COMMAND, cmdName, cmd_type);
		asprintf(&cmd->file, "%s%s", path, dir->d_name);
		list_append(item_list, cmd);
		LOG("File %s (%u) added", cmd->file, file_count);
	}
	closedir(d);

	return file_count;
}

list_t * ReadBackupList(const char* userPath)
{
	char devicePath[64] = {0};
	code_entry_t * cmd;
	game_entry_t * item;
	list_t *list = list_alloc();

	item = createGameEntry(CHEAT_FLAG_PS4, "Remove Cheats, Patches, Plugins");
	item->title_id = strdup("HDD");
	item->type = FILE_TYPE_MENU;
	item->codes = list_alloc();

	cmd = _createCmdCode(PATCH_COMMAND, "Remove all Cheats from HDD", CMD_REMOVE_CHEATS);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, "Remove all Patches from HDD", CMD_REMOVE_PATCHES);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, "Remove all Plugins from HDD", CMD_REMOVE_PLUGINS);
	list_append(item->codes, cmd);
	list_append(list, item);

	item = createGameEntry(CHEAT_FLAG_PS4 | CHEAT_FLAG_ONLINE, "Update Cheats, Patches, Plugins");
	item->title_id = strdup("Internet");
	item->type = FILE_TYPE_MENU;
	item->codes = list_alloc();

	cmd = _createCmdCode(PATCH_COMMAND, "Update Cheats from GitHub", CMD_UPD_INTERNET_CHEATS);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, "Update Patches from GitHub", CMD_UPD_INTERNET_PATCHES);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, "Update Plugins from GitHub", CMD_UPD_INTERNET_PLUGINS);
	list_append(item->codes, cmd);
	list_append(list, item);

	item = createGameEntry(CHEAT_FLAG_PS4, "Update Cheats, Patches, Plugins");
	item->path = strdup(CHEATSMGR_PATH);
	item->title_id = strdup("HDD");
	item->type = FILE_TYPE_MENU;
	list_append(list, item);

	for (int i = 0; i < MAX_USB_DEVICES; i++)
	{
		snprintf(devicePath, sizeof(devicePath), USB_PATH, i);
		if (dir_exists(devicePath) != SUCCESS)
			continue;

		item = createGameEntry(CHEAT_FLAG_PS4, "Update Cheats, Patches, Plugins");
		item->path = strdup(devicePath);
		item->type = FILE_TYPE_MENU;
		asprintf(&item->title_id, "USB %d", i);
		list_append(list, item);
	}

	item = createGameEntry(CHEAT_FLAG_PS4, "Backup Cheats, Patches, Plugins");
	item->title_id = strdup("HDD/USB");
	item->type = FILE_TYPE_MENU;
	item->codes = list_alloc();

	// TODO: Iterate through usb partitions
	// and let user select which partition they want to backup files to
	// if possible, show available space on each partition as well
	cmd = _createCmdCode(PATCH_COMMAND, "Backup Cheats to .Zip (HDD)", CMD_BACKUP_CHEATS_HDD);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, "Backup Cheats to .Zip (USB)", CMD_BACKUP_CHEATS_USB);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, "Backup Patches to .Zip (HDD)", CMD_BACKUP_PATCHES_HDD);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, "Backup Patches to .Zip (USB)", CMD_BACKUP_PATCHES_USB);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, "Backup Plugins to .Zip (HDD)", CMD_BACKUP_PLUGINS_HDD);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, "Backup Plugins to .Zip (USB)", CMD_BACKUP_PLUGINS_USB);
	list_append(item->codes, cmd);
	list_append(list, item);

	return list;
}

int ReadBackupCodes(game_entry_t * item)
{
	code_entry_t * cmd;
	char local_path[256] = {0};
	uint32_t entry_count = 0;
	const char* search_paths[] = {"",
		"cheats/", "patches/", "plugins/",
		"Cheats/", "Patches/", "Plugins/",
		"backup/cheats/", "backup/patches/", "backup/plugins/",
		"Backup/Cheats/", "Backup/Patches/", "Backup/Plugins/", NULL
	};

	item->codes = list_alloc();
	for (const char** search = search_paths; search[0] != NULL; search++)
	{
		snprintf(local_path, sizeof(local_path), "%s%s", item->path, search[0]);
		if (dir_exists(local_path) != SUCCESS)
			continue;

		// find backups
		entry_count += find_zip(item->codes, GOLDCHEATS_BACKUP_PREFIX, "Cheats", local_path, CMD_UPD_LOCAL_CHEATS);
		entry_count += find_zip(item->codes, GOLDPATCH_BACKUP_PREFIX, "Patches", local_path, CMD_UPD_LOCAL_PATCHES);
		entry_count += find_zip(item->codes, GOLDPLUGINS_BACKUP_PREFIX, "Plugins", local_path, CMD_UPD_LOCAL_PLUGINS);
		// find downloaded files
		entry_count += find_zip(item->codes, "cheats", "Cheats", local_path, CMD_UPD_LOCAL_CHEATS);
		entry_count += find_zip(item->codes, "GoldHEN_Cheat_Repository", "Cheats", local_path, CMD_UPD_LOCAL_CHEATS);
		entry_count += find_zip(item->codes, "patches", "Patches", local_path, CMD_UPD_LOCAL_PATCHES);
		entry_count += find_zip(item->codes, "patch1", "Patches", local_path, CMD_UPD_LOCAL_PATCHES);
		entry_count += find_zip(item->codes, "GoldPlugins", "Plugins", local_path, CMD_UPD_LOCAL_PLUGINS);
		entry_count += find_zip(item->codes, "plugins", "Plugins", local_path, CMD_UPD_LOCAL_PLUGINS);
	}

	if (!entry_count)
	{
		cmd = _createCmdCode(PATCH_NULL, "No .Zip files found!", CMD_CODE_NULL);
		list_append(item->codes, cmd);
	}

	return list_count(item->codes);
}

static int is_patch_enabled(uint64_t hash)
{
	char hash_path[256];
	uint8_t settings[2];

	snprintf(hash_path, sizeof(hash_path), GOLDCHEATS_PATCH_PATH "settings/0x%016lx.txt", hash);

	if(read_file(hash_path, settings, sizeof(settings)) < 0 || settings[0] != 0x31)
		return 0;

	return 1;
}

list_t * ReadPatchList(const char* userPath)
{
	DIR *d;
	list_t* list;
	struct dirent *dir;
	game_entry_t *item;
	char fullPath[256];

	d = opendir(userPath);

	if (!d)
		return NULL;

	list = list_alloc();

	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0 || endsWith(dir->d_name, ".xml") == NULL)
			continue;

		snprintf(fullPath, sizeof(fullPath), "%s%s", userPath, dir->d_name);
		if (file_exists(fullPath) != SUCCESS)
			continue;

		LOG("Reading %s...", fullPath);

		char *ver = fullPath;
		char *app = fullPath;
		char *buffer = readTextFile(fullPath, NULL);
		mxml_node_t *node, *tree = NULL;
		tree = mxmlLoadString(NULL, buffer, MXML_NO_CALLBACK);

		if (!tree)
		{
			LOG("XML: could not parse XML:\n%s\n", buffer);
			mxmlDelete(tree);
			free(buffer);
			continue;
		}

		for (node = mxmlFindElement(tree, tree, "Metadata", NULL, NULL, MXML_DESCEND); node != NULL;
			node = mxmlFindElement(node, tree, "Metadata", NULL, NULL, MXML_DESCEND)) {
			const char *TitleData = GetXMLAttr(node, "Title");
			const char *AppVerData = GetXMLAttr(node, "AppVer");

			if ((strcmp(TitleData, app) == 0 && strcmp(AppVerData, ver) == 0))
				continue;

			ver = strdup(AppVerData);
			app = strdup(TitleData);
			item = createGameEntry(CHEAT_FLAG_PS4 | CHEAT_FLAG_HDD | CHEAT_FLAG_PATCH, TitleData);
			item->type = FILE_TYPE_PS4;
			item->path = strdup(fullPath);
			item->version = strdup(AppVerData);
			item->title_id = strdup(dir->d_name);
			*strrchr(item->title_id, '.') = 0;

			LOG("[%s] F(%X) '%s' %s", item->title_id, item->flags, item->name, item->version);
			list_append(list, item);
		}

		mxmlDelete(node);
		mxmlDelete(tree);
		free(buffer);
	}
	
	closedir(d);

	LOG("%d items loaded", list_count(list));
	if (!list_count(list))
	{
		list_free(list);
		return NULL;
	}

	check_game_appdb(list);
	return list;
}

int ReadPatches(game_entry_t * game)
{
	code_entry_t* cmd;
	game->codes = list_alloc();

	LOG("Loading patches from '%s'...", game->path);

	char *buffer = readTextFile(game->path, NULL);
	mxml_node_t *node, *tree = NULL;
	tree = mxmlLoadString(NULL, buffer, MXML_NO_CALLBACK);

	if (!tree)
	{
		LOG("XML: could not parse XML:\n%s\n", buffer);
		mxmlDelete(tree);
		free(buffer);

		return 0;
	}

	for (node = mxmlFindElement(tree, tree, "Metadata", NULL, NULL, MXML_DESCEND); node != NULL;
		 node = mxmlFindElement(node, tree, "Metadata", NULL, NULL, MXML_DESCEND)) {
		const char *TitleData = GetXMLAttr(node, "Title");
		const char *AppVerData = GetXMLAttr(node, "AppVer");
		const char *NameData = GetXMLAttr(node, "Name");
		const char *AuthorData = GetXMLAttr(node, "Author");
		const char *NoteData = GetXMLAttr(node, "Note");
		const char *AppElfData = GetXMLAttr(node, "AppElf");

		if (strcmp(TitleData, game->name) != 0 || strcmp(AppVerData, game->version) != 0)
			continue;

		cmd = (code_entry_t *)calloc(1, sizeof(code_entry_t));
		cmd->type = PATCH_VIEW;
		cmd->name = strdup(NameData);
		cmd->file = strdup(AppElfData);

		mxml_node_t *Patchlist_node = mxmlFindElement(node, node, "PatchList", NULL, NULL, MXML_DESCEND);
		char *code = Patchlist_node ? mxmlSaveAllocString(Patchlist_node, &xml_whitespace_cb) : calloc(1, 1);
		cmd->activated = is_patch_enabled(patch_hash_calc(game, cmd));

		asprintf(&cmd->codes,
			"Game Title: %s\n"
			"Game Version: %s\n"
			"Patch Name: %s\n"
			"Patch Author: %s\n"
			"Patch Elf: %s\n"
			"Patch Path: %s\n"
			"Patch Note: %s\n"
			"Patch Code:\n\n%s\n",
			game->name, game->version, cmd->name, AuthorData, cmd->file, game->path, NoteData, code);
		free(code);

		list_append(game->codes, cmd);
		LOG("Added '%s' (%s)", cmd->name, cmd->file);
	}

	mxmlDelete(node);
	mxmlDelete(tree);
	free(buffer);

	LOG("%d items loaded", list_count(game->codes));

	return list_count(game->codes);
}

/*
 * Function:		UnloadGameList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Free entire array of game_entry
 * Arguments:
 *	list:			Array of game_entry to free
 *	count:			number of game entries
 * Return:			void
 */
void UnloadGameList(list_t * list)
{
	list_node_t *node, *nc;
	game_entry_t *item;
	code_entry_t *code;

	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		if (item->name)
		{
			free(item->name);
			item->name = NULL;
		}

		if (item->path)
		{
			free(item->path);
			item->path = NULL;
		}

		if (item->version)
		{
			free(item->version);
			item->version = NULL;
		}

		if (item->title_id)
		{
			free(item->title_id);
			item->title_id = NULL;
		}
		
		if (item->codes)
		{
			for (nc = list_head(item->codes); (code = list_get(nc)); nc = list_next(nc))
			{
				if (code->codes)
				{
					free (code->codes);
					code->codes = NULL;
				}
				if (code->name)
				{
					free (code->name);
					code->name = NULL;
				}
				if (code->options && code->options_count > 0)
				{
					for (int z = 0; z < code->options_count; z++)
					{
						if (code->options[z].line)
							free(code->options[z].line);
						if (code->options[z].name)
							free(code->options[z].name);
						if (code->options[z].value)
							free(code->options[z].value);
					}
					
					free (code->options);
				}
			}
			
			list_free(item->codes);
			item->codes = NULL;
		}
	}

	list_free(list);
	
	LOG("UnloadGameList() :: Successfully unloaded game list");
}

int sortCodeList_Compare(const void* a, const void* b)
{
	return strcasecmp(((code_entry_t*) a)->name, ((code_entry_t*) b)->name);
}

int sortGameList_Exists(const void* a, const void* b)
{
	return ((((game_entry_t*) a)->flags & CHEAT_FLAG_OWNER) < (((game_entry_t*) b)->flags & CHEAT_FLAG_OWNER));
}

/*
 * Function:		qsortSaveList_Compare()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Compares two game_entry for QuickSort
 * Arguments:
 *	a:				First code
 *	b:				Second code
 * Return:			1 if greater, -1 if less, or 0 if equal
 */
int sortGameList_Compare(const void* a, const void* b)
{
	return strcasecmp(((game_entry_t*) a)->name, ((game_entry_t*) b)->name);
}

int sortGameList_Compare_TitleID(const void* a, const void* b)
{
	char* ta = ((game_entry_t*) a)->title_id;
	char* tb = ((game_entry_t*) b)->title_id;

	if (!ta)
		return (-1);

	if (!tb)
		return (1);

	return strcasecmp(ta, tb);
}

static void read_shn_games(const char* userPath, list_t *list)
{
	DIR *d;
	struct dirent *dir;
	game_entry_t *item;
	char fullPath[256];

	d = opendir(userPath);
	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!endsWith(dir->d_name, ".shn") && !endsWith(dir->d_name, ".shn-disabled"))
			continue;

		snprintf(fullPath, sizeof(fullPath), "%s%s", userPath, dir->d_name);
		char *buffer = readTextFile(fullPath, NULL);
		if (!buffer)
			continue;

		LOG("Reading %s...", dir->d_name);
		mxml_node_t *node, *tree = NULL;

		/* parse the file and get the DOM */
		tree = mxmlLoadString(NULL, buffer, MXML_NO_CALLBACK);
		if (!tree)
		{
			LOG("XML: could not parse '%s'", fullPath);
			free(buffer);
			continue;
		}

		node = mxmlFindElement(tree, tree, "Trainer", "Game", NULL, MXML_DESCEND);
		if (!node)
		{
			LOG("Invalid SHN file %s", fullPath);
			free(buffer);
			continue;
		}

		item = createGameEntry(CHEAT_FLAG_SHN | CHEAT_FLAG_PS4 | CHEAT_FLAG_HDD, GetXMLAttr(node, "Game"));
		item->type = FILE_TYPE_PS4;
		item->path = strdup(fullPath);
		item->version = strdup(GetXMLAttr(node, "Version"));
		item->title_id = strdup(GetXMLAttr(node, "Cusa"));

		/* free the document */
		mxmlDelete(tree);
		free(buffer);

		if (endsWith(dir->d_name, ".shn-disabled"))
			item->flags |= CHEAT_FLAG_LOCKED;

		LOG("[%s] F(%d) '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}
	
	closedir(d);
	return;
}

static void read_mc4_games(const char* userPath, list_t *list)
{
	DIR *d;
	struct dirent *dir;
	game_entry_t *item;
	char fullPath[256];

	d = opendir(userPath);
	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!endsWith(dir->d_name, ".mc4") && !endsWith(dir->d_name, ".mc4-disabled"))
			continue;

		snprintf(fullPath, sizeof(fullPath), "%s%s", userPath, dir->d_name);
		char *buffer = readTextFile(fullPath, NULL);
		if (!buffer)
			continue;

		LOG("Reading %s...", dir->d_name);
		char *dec_data = mc4_decrypt(buffer);
		free(buffer);
		buffer = dec_data;

		if (!dec_data)
		{
			LOG("Error: could not decrypt %s", fullPath);
			continue;
		}

		mxml_node_t *node, *tree = NULL;

		/* parse the file and get the DOM */
		tree = mxmlLoadString(NULL, buffer, MXML_NO_CALLBACK);
		if (!tree)
		{
			LOG("XML: could not parse '%.16s...'", buffer);
			free(buffer);

			item = createGameEntry(CHEAT_FLAG_MC4 | CHEAT_FLAG_PS4 | CHEAT_FLAG_HDD, fullPath);
			sprintf(item->name, "%s%s", UTF8_CHAR_GROUP, dir->d_name);
			asprintf(&item->title_id, "%.9s", dir->d_name);
			item->path = strdup(fullPath);
			item->version = strdup("?");
			list_append(list, item);
			continue;
		}

		node = mxmlFindElement(tree, tree, "Trainer", "Game", NULL, MXML_DESCEND);
		if (!node)
		{
			LOG("Invalid MC4 file %s", fullPath);
			free(buffer);
			continue;
		}

		item = createGameEntry(CHEAT_FLAG_MC4 | CHEAT_FLAG_PS4 | CHEAT_FLAG_HDD, GetXMLAttr(node, "Game"));
		item->type = FILE_TYPE_PS4;
		item->path = strdup(fullPath);
		item->version = strdup(GetXMLAttr(node, "Version"));
		item->title_id = strdup(GetXMLAttr(node, "Cusa"));

		/* free the document */
		mxmlDelete(tree);
		free(buffer);

		if (endsWith(dir->d_name, ".mc4-disabled"))
			item->flags |= CHEAT_FLAG_LOCKED;

		LOG("[%s] F(%d) '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}
	
	closedir(d);
	return;
}

static void read_json_games(const char* userPath, list_t *list)
{
	DIR *d;
	struct dirent *dir;
	game_entry_t *item;
	char fullPath[256];

	d = opendir(userPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!endsWith(dir->d_name, ".json") && !endsWith(dir->d_name, ".json-disabled"))
			continue;

		snprintf(fullPath, sizeof(fullPath), "%s%s", userPath, dir->d_name);
		char *buffer = readTextFile(fullPath, NULL);
		if (!buffer)
			continue;

		LOG("Reading %s...", dir->d_name);
		cJSON *cheat = cJSON_Parse(buffer);
		const cJSON *jobject;

		if (!cheat)
		{
			LOG("JSON parse Error: %s\n", buffer);
			free(buffer);
			continue;
		}

		jobject = cJSON_GetObjectItemCaseSensitive(cheat, "name");
		item = createGameEntry(CHEAT_FLAG_JSON | CHEAT_FLAG_PS4 | CHEAT_FLAG_HDD, cJSON_IsString(jobject) ? jobject->valuestring : "");
		item->type = FILE_TYPE_PS4;
		item->path = strdup(fullPath);

		jobject = cJSON_GetObjectItemCaseSensitive(cheat, "version");
		item->version = strdup(cJSON_IsString(jobject) ? jobject->valuestring : "");

		jobject = cJSON_GetObjectItemCaseSensitive(cheat, "id");
		item->title_id = strdup(cJSON_IsString(jobject) ? jobject->valuestring : "");

		cJSON_Delete(cheat);
		free(buffer);

		if (endsWith(dir->d_name, ".json-disabled"))
			item->flags |= CHEAT_FLAG_LOCKED;

		LOG("[%s] F(%d) '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}
	
	closedir(d);
	return;
}

/*
 * Function:		ReadUserList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Reads the entire userlist folder into a game_entry array
 * Arguments:
 *	gmc:			Set as the number of games read
 * Return:			Pointer to array of game_entry, null if failed
 */
list_t * ReadUserList(const char* userPath)
{
//	game_entry_t *item;
//	code_entry_t *cmd;
	list_t *list;
	char fullPath[256];

	if (file_exists(userPath) != SUCCESS)
		return NULL;

	list = list_alloc();

	LOG("Loading JSON files...");
	snprintf(fullPath, sizeof(fullPath), "%sjson/", userPath);
	read_json_games(fullPath, list);

	LOG("Loading SHN files...");
	snprintf(fullPath, sizeof(fullPath), "%sshn/", userPath);
	read_shn_games(fullPath, list);

	LOG("Loading MC4 files...");
	snprintf(fullPath, sizeof(fullPath), "%smc4/", userPath);
	read_mc4_games(fullPath, list);

	if (!list_count(list))
	{
		list_free(list);
		return NULL;
	}

	check_game_appdb(list);
	return list;
}

/*
 * Function:		ReadOnlineList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Downloads the entire gamelist file into a game_entry array
 * Arguments:
 *	gmc:			Set as the number of games read
 * Return:			Pointer to array of game_entry, null if failed
 */
static void _ReadOnlineListEx(const char* urlPath, const char* fext, uint16_t flag, list_t *list)
{
	game_entry_t *item;
	char path[256];
	char fname[64];
	struct stat stats;

	snprintf(path, sizeof(path), CHEATSMGR_LOCAL_CACHE "%s_games.txt", fext);
	snprintf(fname, sizeof(fname), "%s.txt", fext);

	if (stat(path, &stats) != SUCCESS && !http_download(urlPath, fname, path, 0))
		return;

	// re-download if file is +1 day old
	if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL) || stats.st_size == 0)
		if (!http_download(urlPath, fname, path, 0))
			return;

	long fsize;
	char *data = readTextFile(path, &fsize);
	
	char *ptr = data;
	char *end = data + fsize;

	while (ptr < end && *ptr)
	{
		char *tmp, *content = ptr;

		while (ptr < end && *ptr != '\n' && *ptr != '\r')
		{
			ptr++;
		}
		*ptr++ = 0;

		if ((tmp = strchr(content, '=')) != NULL)
		{
			*tmp++ = 0;
			item = createGameEntry(flag | CHEAT_FLAG_ONLINE, tmp);
			asprintf(&item->path, "%s%s/%s", urlPath, fext, content);

			if (item->flags & CHEAT_FLAG_MC4)
				*strrchr(item->path, '.') = 0;

			tmp = strchr(content, '_');
			*tmp++ = 0;
			item->title_id = strdup(content);

			*(tmp + 5) = 0;
			item->version = strdup(tmp);

			LOG("+ [%s %s] %s", item->title_id, item->version, item->name);
			list_append(list, item);
		}

		if (ptr < end && *ptr == '\r')
		{
			ptr++;
		}
		if (ptr < end && *ptr == '\n')
		{
			ptr++;
		}
	}

	if (data) free(data);
}

list_t * ReadOnlineList(const char* urlPath)
{
	list_t *list = list_alloc();

	// PS4 JSON games
	_ReadOnlineListEx(urlPath, "json", CHEAT_FLAG_PS4 | CHEAT_FLAG_JSON, list);

	// PS4 SHN games
	_ReadOnlineListEx(urlPath, "shn", CHEAT_FLAG_PS4 | CHEAT_FLAG_SHN, list);

	// PS4 MC4 games
	_ReadOnlineListEx(urlPath, "mc4", CHEAT_FLAG_PS4 | CHEAT_FLAG_MC4, list);

	if (!list_count(list))
	{
		list_free(list);
		return NULL;
	}

	check_game_appdb(list);
	return list;
}
