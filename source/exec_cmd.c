#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <orbis/SaveData.h>

#include "cheats.h"
#include "menu.h"
#include "common.h"
#include "util.h"


uint64_t patch_hash_calc(const game_entry_t* game, const code_entry_t* code)
{
	uint64_t output_hash = 0x1505;

	output_hash = djb2_hash(game->name, output_hash);
	output_hash = djb2_hash(code->name, output_hash);
	output_hash = djb2_hash(game->version, output_hash);
	output_hash = djb2_hash(game->path, output_hash);
	output_hash = djb2_hash(code->file, output_hash);

#ifdef DEBUG_ENABLE_LOG
	LOG("input: \"%s%s%s%s%s\"", game->name, code->name, game->version, game->path, code->file);
	LOG("output: 0x%016lx", output_hash);
#endif

	return output_hash;
}

static void togglePatch(const game_entry_t* game, const code_entry_t* code)
{
	char hash_path[256];
	uint8_t settings[2] = {0x30, 0x0A}; // "0\n"

	uint64_t hash = patch_hash_calc(game, code);
	snprintf(hash_path, sizeof(hash_path), GOLDCHEATS_PATCH_PATH "settings/0x%016lx.txt", hash);
	LOG("Toggle patch (%d): %s", code->activated, hash_path);

	settings[0] += code->activated;
	if (write_buffer(hash_path, settings, sizeof(settings)) < 0)
	{
		LOG("Failed to write patch settings file %s", hash_path);
		return;
	}

	show_message("Patch \"%s\" %s", code->name, code->activated ? "Enabled" : "Disabled");
}

static void updNetCheats(void)
{
	if (!http_download(GOLDCHEATS_URL, GOLDCHEATS_FILE, GOLDCHEATS_LOCAL_CACHE LOCAL_TEMP_ZIP, 1))
	{
		show_message("No internet connection to " GOLDCHEATS_URL GOLDCHEATS_FILE " or server not available!");
		return;
	}

	int ret = extract_zip_gh(GOLDCHEATS_LOCAL_CACHE LOCAL_TEMP_ZIP, GOLDCHEATS_DATA_PATH);
	if (ret > 0)
	{
		char *cheat_ver = readTextFile(GOLDCHEATS_DATA_PATH "misc/cheat_ver.txt", NULL);
		show_message("Successfully installed %d %s files\n%s", ret, "cheat", cheat_ver);
		free(cheat_ver);
	}
	else
	{
		show_message("No files extracted!");
	}

	unlink_secure(GOLDCHEATS_LOCAL_CACHE LOCAL_TEMP_ZIP);
}

static void updNetPatches(void)
{
	if (!http_download(GOLDPATCH_URL, GOLDPATCH_FILE, GOLDCHEATS_LOCAL_CACHE LOCAL_TEMP_ZIP, 1))
	{
		show_message("No internet connection to " GOLDPATCH_URL GOLDPATCH_FILE " or server not available!");
		return;
	}

	int ret = extract_zip_gh(GOLDCHEATS_LOCAL_CACHE LOCAL_TEMP_ZIP, GOLDCHEATS_PATCH_PATH);
	if (ret > 0)
	{
		char *patch_ver = readTextFile(GOLDCHEATS_PATCH_PATH "misc/patch_ver.txt", NULL);
		show_message("Successfully installed %d %s files\n%s", ret, "patch", patch_ver);
		free(patch_ver);
	}
	else
	{
		show_message("No files extracted!");
	}

	unlink_secure(GOLDCHEATS_LOCAL_CACHE LOCAL_TEMP_ZIP);
}

static void updLocalCheats(const char* upd_path)
{
	if (!extract_zip_gh(upd_path, GOLDCHEATS_DATA_PATH))
	{
		show_message("Cannot open file %s", upd_path);
		return;
	}

	char *cheat_ver = readTextFile(GOLDCHEATS_DATA_PATH "misc/cheat_ver.txt", NULL);
	show_message("Successfully installed offline cheat data from %s\n%s", upd_path, cheat_ver);
	free(cheat_ver);
}

static void updLocalPatches(const char* upd_path)
{
	if (!extract_zip_gh(upd_path, GOLDCHEATS_PATCH_PATH))
	{
		show_message("Cannot open file %s", upd_path);
		return;
	}

	char *patch_ver = readTextFile(GOLDCHEATS_PATCH_PATH "misc/patch_ver.txt", NULL);
	show_message("Successfully installed offline patch data from %s\n%s", upd_path, patch_ver);
	free(patch_ver);
}

static void backupCheats(const char* dst_path)
{
	char zip_path[256];
	struct tm t;

	// build file path
	t = *gmtime(&(time_t){time(NULL)});
	snprintf(zip_path, sizeof(zip_path), "%sGH-cheats_%d-%02d-%02d_%02d%02d%02d.zip", dst_path, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	if (!zip_directory(GOLDCHEATS_PATH "cheats", GOLDCHEATS_DATA_PATH, zip_path))
	{
		show_message("Failed to backup cheats to %s", zip_path);
		return;
	};

	show_message("Created cheats backup successfully:\n%s", zip_path);
}

static void backupPatches(const char* dst_path)
{
	char zip_path[256];
	struct tm t;

	// build file path
	t = *gmtime(&(time_t){time(NULL)});
	snprintf(zip_path, sizeof(zip_path), "%sGH-patches_%d-%02d-%02d_%02d%02d%02d.zip", dst_path, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	if (!zip_directory(GOLDCHEATS_PATH "patches", GOLDCHEATS_PATCH_PATH, zip_path))
	{
		show_message("Failed to backup patches to %s", zip_path);
		return;
	};

	show_message("Created patches backup successfully:\n%s", zip_path);
}

void execCodeCommand(code_entry_t* code, const char* codecmd)
{
	switch (codecmd[0])
	{
		case CMD_TOGGLE_PATCH:
			togglePatch(selected_entry, code);
			break;

		case CMD_UPD_INTERNET_CHEATS:
			updNetCheats();
			code->activated = 0;
			break;

		case CMD_UPD_INTERNET_PATCHES:
			updNetPatches();
			code->activated = 0;
			break;

		case CMD_UPD_LOCAL_CHEATS_USB:
			updLocalCheats(USB0_PATH GOLDCHEATS_LOCAL_FILE);
			code->activated = 0;
			break;

		case CMD_UPD_LOCAL_CHEATS_HDD:
			updLocalCheats(GOLDCHEATS_PATH GOLDCHEATS_LOCAL_FILE);
			code->activated = 0;
			break;

		case CMD_UPD_LOCAL_PATCHES_USB:
			updLocalPatches(USB0_PATH GOLDPATCH_FILE);
			code->activated = 0;
			break;

		case CMD_UPD_LOCAL_PATCHES_HDD:
			updLocalPatches(GOLDCHEATS_PATH GOLDPATCH_FILE);
			code->activated = 0;
			break;

		case CMD_BACKUP_CHEATS_HDD:
		case CMD_BACKUP_CHEATS_USB:
			backupCheats(codecmd[0] == CMD_BACKUP_CHEATS_USB ? USB0_PATH : GOLDCHEATS_PATH);
			code->activated = 0;
			break;

		case CMD_BACKUP_PATCHES_HDD:
		case CMD_BACKUP_PATCHES_USB:
			backupPatches(codecmd[0] == CMD_BACKUP_PATCHES_USB ? USB0_PATH : GOLDCHEATS_PATH);
			code->activated = 0;
			break;

		default:
			break;
	}

	return;
}
