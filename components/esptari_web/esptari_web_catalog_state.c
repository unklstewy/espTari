#include "esptari_web_catalog_state.h"

#include <stdio.h>
#include <string.h>

static uint64_t catalog_download_seq;
static uint64_t catalog_probe_seq;
static uint64_t catalog_scan_seq;
static char catalog_last_scan_id[48];
static char catalog_prev_scan_id[48];

static const catalog_entry_t rom_catalog_entries[] = {
    {"rom.atari.st.01", "/sdcard/roms/st/TOS104.ROM", "", "", "local_only", "2026-03-01T15:22:01Z", 0},
    {"rom.atari.st.02", "", "http://catalog.example/roms/TOS206.ROM", "sha256:rom0206", "online", "2026-03-01T15:22:05Z", 0},
};

static const catalog_entry_t floppy_catalog_entries[] = {
    {"disk.automation.a_093", "/sdcard/disks/st/AUTOMATION/A_093.ST", "http://ataristdb.sidecartridge.com/AUTOMATION/A_093.ST", "sha256:abcd", "online", "2026-03-01T15:22:01Z", 0},
    {"disk.demos.dead_entry", "", "http://ataristdb.sidecartridge.com/DEMOS/DEAD.ST", "", "dead", "2026-03-01T15:22:11Z", 3},
};

static const catalog_entry_t tos_catalog_entries[] = {
    {"tos.eu.1.04", "/sdcard/tos/TOS104.IMG", "", "", "local_only", "2026-03-01T15:22:21Z", 0},
    {"tos.eu.2.06", "", "http://catalog.example/tos/TOS206.IMG", "sha256:tos0206", "offline", "2026-03-01T15:22:31Z", 1},
};

static catalog_entry_runtime_t rom_catalog_runtime[] = {
    {.state_override = false, .local_present = true, .dead_marked = false, .last_dead_retry_result = "none", .download_fail_count = 0, .first_missing_at_us = 0},
    {.state_override = false, .local_present = false, .dead_marked = false, .last_dead_retry_result = "none", .download_fail_count = 0, .first_missing_at_us = 1710002100000ULL},
};

static catalog_entry_runtime_t floppy_catalog_runtime[] = {
    {.state_override = false, .local_present = true, .dead_marked = false, .last_dead_retry_result = "none", .download_fail_count = 0, .first_missing_at_us = 0},
    {.state_override = true, .availability_state = "dead", .local_present = false, .dead_marked = true, .dead_source = "probe_threshold", .last_dead_reason = "probe failure threshold reached", .last_dead_marked_at_us = 1710002200000ULL, .last_dead_retry_result = "none", .download_fail_count = 3, .first_missing_at_us = 1710002200000ULL},
};

static catalog_entry_runtime_t tos_catalog_runtime[] = {
    {.state_override = false, .local_present = true, .dead_marked = false, .last_dead_retry_result = "none", .download_fail_count = 0, .first_missing_at_us = 0},
    {.state_override = false, .local_present = false, .dead_marked = false, .last_dead_retry_result = "none", .download_fail_count = 1, .first_missing_at_us = 1710002300000ULL},
};

static const catalog_def_t catalog_defs[] = {
    {"roms", "/sdcard/config/engine_v2/rom_catalog.json", rom_catalog_entries, sizeof(rom_catalog_entries) / sizeof(rom_catalog_entries[0])},
    {"floppies", "/sdcard/config/engine_v2/disk_catalog.json", floppy_catalog_entries, sizeof(floppy_catalog_entries) / sizeof(floppy_catalog_entries[0])},
    {"tos", "/sdcard/config/engine_v2/tos_catalog.json", tos_catalog_entries, sizeof(tos_catalog_entries) / sizeof(tos_catalog_entries[0])},
};

size_t esptari_web_catalog_count(void)
{
    return sizeof(catalog_defs) / sizeof(catalog_defs[0]);
}

const catalog_def_t *esptari_web_catalog_at(size_t index)
{
    if (index >= esptari_web_catalog_count()) {
        return NULL;
    }
    return &catalog_defs[index];
}

const catalog_def_t *esptari_web_catalog_find(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < esptari_web_catalog_count(); i++) {
        if (strcmp(catalog_defs[i].name, name) == 0) {
            return &catalog_defs[i];
        }
    }
    return NULL;
}

catalog_entry_runtime_t *esptari_web_catalog_runtime_at(const catalog_def_t *def, size_t index)
{
    if (def == NULL) {
        return NULL;
    }
    if (strcmp(def->name, "roms") == 0 && index < (sizeof(rom_catalog_runtime) / sizeof(rom_catalog_runtime[0]))) {
        return &rom_catalog_runtime[index];
    }
    if (strcmp(def->name, "floppies") == 0 && index < (sizeof(floppy_catalog_runtime) / sizeof(floppy_catalog_runtime[0]))) {
        return &floppy_catalog_runtime[index];
    }
    if (strcmp(def->name, "tos") == 0 && index < (sizeof(tos_catalog_runtime) / sizeof(tos_catalog_runtime[0]))) {
        return &tos_catalog_runtime[index];
    }
    return NULL;
}

const char *esptari_web_catalog_entry_state(const catalog_def_t *def, size_t index)
{
    const catalog_entry_t *entry = &def->entries[index];
    catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, index);
    if (runtime != NULL && runtime->state_override && runtime->availability_state[0] != '\0') {
        return runtime->availability_state;
    }
    return entry->availability_state;
}

bool esptari_web_catalog_entry_local_present(const catalog_def_t *def, size_t index)
{
    catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, index);
    if (runtime != NULL) {
        return runtime->local_present;
    }
    const catalog_entry_t *entry = &def->entries[index];
    return !(entry == NULL || entry->local_path == NULL || entry->local_path[0] == '\0');
}

const char *esptari_web_catalog_entry_local_path_projected(const catalog_def_t *def, size_t index)
{
    if (!esptari_web_catalog_entry_local_present(def, index)) {
        return "";
    }
    return def->entries[index].local_path;
}

uint32_t esptari_web_catalog_entry_download_fail_count(const catalog_def_t *def, size_t index)
{
    const catalog_entry_t *entry = &def->entries[index];
    catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, index);
    if (runtime != NULL) {
        return runtime->download_fail_count;
    }
    return entry->download_fail_count;
}

int esptari_web_catalog_find_entry_index(const catalog_def_t *def, const char *entry_id)
{
    if (def == NULL || entry_id == NULL) {
        return -1;
    }
    for (size_t i = 0; i < def->entry_count; i++) {
        if (strcmp(def->entries[i].id, entry_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

uint64_t esptari_web_catalog_next_download_seq(void)
{
    catalog_download_seq++;
    return catalog_download_seq;
}

uint64_t esptari_web_catalog_next_probe_seq(void)
{
    catalog_probe_seq++;
    return catalog_probe_seq;
}

uint64_t esptari_web_catalog_next_scan_seq(void)
{
    catalog_scan_seq++;
    return catalog_scan_seq;
}

void esptari_web_catalog_record_scan_id(uint64_t scan_seq)
{
    if (catalog_last_scan_id[0] != '\0') {
        snprintf(catalog_prev_scan_id, sizeof(catalog_prev_scan_id), "%s", catalog_last_scan_id);
    }
    snprintf(catalog_last_scan_id, sizeof(catalog_last_scan_id), "scan_%06llu", (unsigned long long)scan_seq);
}

const char *esptari_web_catalog_last_scan_id(void)
{
    return catalog_last_scan_id;
}

const char *esptari_web_catalog_prev_scan_id(void)
{
    return catalog_prev_scan_id;
}
