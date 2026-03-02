#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
	char mapping_profile_id[64];
	char machine[32];
	char profile[32];
	uint32_t revision;
	uint64_t updated_at_us;
} esptari_input_mapping_summary_t;

typedef struct {
	char mapping_profile_id[64];
	char machine[32];
	char profile[32];
	char entries_json[512];
	uint32_t revision;
	uint64_t updated_at_us;
} esptari_input_mapping_t;

typedef struct {
	bool has_active;
	char previous_mapping_profile_id[64];
	char active_mapping_profile_id[64];
	uint32_t active_mapping_revision;
	uint64_t cutover_tick;
	bool no_op;
} esptari_input_apply_result_t;

void esptari_input_init(void);
esp_err_t esptari_input_mapping_create(const char *mapping_profile_id,
									   const char *machine,
									   const char *profile,
									   const char *entries_json,
									   esptari_input_mapping_t *out_mapping);
int esptari_input_mapping_list(const char *machine,
							   esptari_input_mapping_summary_t *out_items,
							   int max_items);
esp_err_t esptari_input_mapping_get(const char *mapping_profile_id,
									esptari_input_mapping_t *out_mapping);
esp_err_t esptari_input_mapping_patch(const char *mapping_profile_id,
									  const char *profile,
									  const char *entries_json,
									  bool *out_changed,
									  esptari_input_mapping_t *out_mapping);
esp_err_t esptari_input_mapping_delete(const char *mapping_profile_id);
esp_err_t esptari_input_mapping_apply(const char *mapping_profile_id,
									  bool has_expected_revision,
									  uint32_t expected_revision,
									  esptari_input_apply_result_t *out_result);
esp_err_t esptari_input_mapping_get_active(esptari_input_mapping_t *out_mapping);
