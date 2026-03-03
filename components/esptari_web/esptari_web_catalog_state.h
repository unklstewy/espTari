#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *id;
    const char *local_path;
    const char *hosted_url;
    const char *sha256_expected;
    const char *availability_state;
    const char *availability_checked_at;
    uint32_t download_fail_count;
} catalog_entry_t;

typedef struct {
    bool state_override;
    char availability_state[16];
    bool local_present;
    uint64_t last_indexed_scan_seq;
    uint64_t last_transition_to_missing_scan_seq;
    uint64_t last_transition_to_present_scan_seq;
    uint64_t indexed_file_size_bytes;
    uint64_t indexed_mtime_us;
    uint64_t last_indexed_at_us;
    uint32_t probe_attempts;
    uint32_t probe_fail_streak;
    uint64_t last_probe_at_us;
    bool last_probe_timed_out;
    bool dead_marked;
    char dead_source[24];
    char last_dead_reason[96];
    uint64_t last_dead_marked_at_us;
    uint32_t dead_retry_attempts;
    uint32_t dead_retry_successes;
    uint32_t dead_retry_failures;
    uint64_t last_dead_retry_at_us;
    char last_dead_retry_result[16];
    uint32_t download_fail_count;
    uint64_t first_missing_at_us;
} catalog_entry_runtime_t;

typedef struct {
    const char *name;
    const char *path;
    const catalog_entry_t *entries;
    size_t entry_count;
} catalog_def_t;

size_t esptari_web_catalog_count(void);
const catalog_def_t *esptari_web_catalog_at(size_t index);
const catalog_def_t *esptari_web_catalog_find(const char *name);

catalog_entry_runtime_t *esptari_web_catalog_runtime_at(const catalog_def_t *def, size_t index);
const char *esptari_web_catalog_entry_state(const catalog_def_t *def, size_t index);
bool esptari_web_catalog_entry_local_present(const catalog_def_t *def, size_t index);
const char *esptari_web_catalog_entry_local_path_projected(const catalog_def_t *def, size_t index);
uint32_t esptari_web_catalog_entry_download_fail_count(const catalog_def_t *def, size_t index);
int esptari_web_catalog_find_entry_index(const catalog_def_t *def, const char *entry_id);

uint64_t esptari_web_catalog_next_download_seq(void);
uint64_t esptari_web_catalog_next_probe_seq(void);
uint64_t esptari_web_catalog_next_scan_seq(void);
void esptari_web_catalog_record_scan_id(uint64_t scan_seq);
const char *esptari_web_catalog_last_scan_id(void);
const char *esptari_web_catalog_prev_scan_id(void);
