#include "esptari_input.h"

#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define INPUT_MAX_MAPPINGS 8

typedef struct {
	bool used;
	esptari_input_mapping_t mapping;
} mapping_slot_t;

static mapping_slot_t s_mappings[INPUT_MAX_MAPPINGS];
static SemaphoreHandle_t s_input_lock;
static char s_active_mapping_profile_id[64];
static uint64_t s_apply_cutover_tick;

static bool str_empty(const char *value)
{
	return value == NULL || value[0] == '\0';
}

static int find_mapping_index(const char *mapping_profile_id)
{
	for (int index = 0; index < INPUT_MAX_MAPPINGS; index++) {
		if (!s_mappings[index].used) {
			continue;
		}
		if (strcmp(s_mappings[index].mapping.mapping_profile_id, mapping_profile_id) == 0) {
			return index;
		}
	}
	return -1;
}

static int alloc_mapping_index(void)
{
	for (int index = 0; index < INPUT_MAX_MAPPINGS; index++) {
		if (!s_mappings[index].used) {
			return index;
		}
	}
	return -1;
}

void esptari_input_init(void)
{
	if (s_input_lock == NULL) {
		s_input_lock = xSemaphoreCreateMutex();
	}

	if (s_input_lock != NULL) {
		xSemaphoreTake(s_input_lock, portMAX_DELAY);
		memset(s_mappings, 0, sizeof(s_mappings));
		memset(s_active_mapping_profile_id, 0, sizeof(s_active_mapping_profile_id));
		s_apply_cutover_tick = 0;
		xSemaphoreGive(s_input_lock);
	}
}

esp_err_t esptari_input_mapping_create(const char *mapping_profile_id,
									   const char *machine,
									   const char *profile,
									   const char *entries_json,
									   esptari_input_mapping_t *out_mapping)
{
	if (s_input_lock == NULL || str_empty(mapping_profile_id) || str_empty(machine) || str_empty(profile)) {
		return ESP_ERR_INVALID_ARG;
	}

	xSemaphoreTake(s_input_lock, portMAX_DELAY);

	if (find_mapping_index(mapping_profile_id) >= 0) {
		xSemaphoreGive(s_input_lock);
		return ESP_ERR_INVALID_STATE;
	}

	int new_index = alloc_mapping_index();
	if (new_index < 0) {
		xSemaphoreGive(s_input_lock);
		return ESP_ERR_NO_MEM;
	}

	mapping_slot_t *slot = &s_mappings[new_index];
	slot->used = true;
	memset(&slot->mapping, 0, sizeof(slot->mapping));
	strlcpy(slot->mapping.mapping_profile_id, mapping_profile_id, sizeof(slot->mapping.mapping_profile_id));
	strlcpy(slot->mapping.machine, machine, sizeof(slot->mapping.machine));
	strlcpy(slot->mapping.profile, profile, sizeof(slot->mapping.profile));
	strlcpy(slot->mapping.entries_json,
			str_empty(entries_json) ? "[]" : entries_json,
			sizeof(slot->mapping.entries_json));
	slot->mapping.revision = 1;
	slot->mapping.updated_at_us = (uint64_t)esp_timer_get_time();

	if (out_mapping != NULL) {
		*out_mapping = slot->mapping;
	}

	xSemaphoreGive(s_input_lock);
	return ESP_OK;
}

int esptari_input_mapping_list(const char *machine,
							  esptari_input_mapping_summary_t *out_items,
							  int max_items)
{
	if (s_input_lock == NULL || out_items == NULL || max_items <= 0) {
		return 0;
	}

	xSemaphoreTake(s_input_lock, portMAX_DELAY);

	int count = 0;
	for (int index = 0; index < INPUT_MAX_MAPPINGS; index++) {
		if (!s_mappings[index].used) {
			continue;
		}
		if (!str_empty(machine) && strcmp(s_mappings[index].mapping.machine, machine) != 0) {
			continue;
		}
		if (count >= max_items) {
			break;
		}

		esptari_input_mapping_summary_t *item = &out_items[count++];
		memset(item, 0, sizeof(*item));
		strlcpy(item->mapping_profile_id,
				s_mappings[index].mapping.mapping_profile_id,
				sizeof(item->mapping_profile_id));
		strlcpy(item->machine, s_mappings[index].mapping.machine, sizeof(item->machine));
		strlcpy(item->profile, s_mappings[index].mapping.profile, sizeof(item->profile));
		item->revision = s_mappings[index].mapping.revision;
		item->updated_at_us = s_mappings[index].mapping.updated_at_us;
	}

	xSemaphoreGive(s_input_lock);
	return count;
}

esp_err_t esptari_input_mapping_get(const char *mapping_profile_id,
									esptari_input_mapping_t *out_mapping)
{
	if (s_input_lock == NULL || str_empty(mapping_profile_id) || out_mapping == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	xSemaphoreTake(s_input_lock, portMAX_DELAY);
	int index = find_mapping_index(mapping_profile_id);
	if (index < 0) {
		xSemaphoreGive(s_input_lock);
		return ESP_ERR_NOT_FOUND;
	}

	*out_mapping = s_mappings[index].mapping;
	xSemaphoreGive(s_input_lock);
	return ESP_OK;
}

esp_err_t esptari_input_mapping_patch(const char *mapping_profile_id,
									  const char *profile,
									  const char *entries_json,
									  bool *out_changed,
									  esptari_input_mapping_t *out_mapping)
{
	if (s_input_lock == NULL || str_empty(mapping_profile_id)) {
		return ESP_ERR_INVALID_ARG;
	}

	xSemaphoreTake(s_input_lock, portMAX_DELAY);
	int index = find_mapping_index(mapping_profile_id);
	if (index < 0) {
		xSemaphoreGive(s_input_lock);
		return ESP_ERR_NOT_FOUND;
	}

	bool changed = false;
	mapping_slot_t *slot = &s_mappings[index];

	if (!str_empty(profile) && strcmp(slot->mapping.profile, profile) != 0) {
		strlcpy(slot->mapping.profile, profile, sizeof(slot->mapping.profile));
		changed = true;
	}

	if (!str_empty(entries_json) && strcmp(slot->mapping.entries_json, entries_json) != 0) {
		strlcpy(slot->mapping.entries_json, entries_json, sizeof(slot->mapping.entries_json));
		changed = true;
	}

	if (changed) {
		slot->mapping.revision += 1;
		slot->mapping.updated_at_us = (uint64_t)esp_timer_get_time();
	}

	if (out_changed != NULL) {
		*out_changed = changed;
	}
	if (out_mapping != NULL) {
		*out_mapping = slot->mapping;
	}

	xSemaphoreGive(s_input_lock);
	return ESP_OK;
}

esp_err_t esptari_input_mapping_delete(const char *mapping_profile_id)
{
	if (s_input_lock == NULL || str_empty(mapping_profile_id)) {
		return ESP_ERR_INVALID_ARG;
	}

	xSemaphoreTake(s_input_lock, portMAX_DELAY);
	int index = find_mapping_index(mapping_profile_id);
	if (index < 0) {
		xSemaphoreGive(s_input_lock);
		return ESP_ERR_NOT_FOUND;
	}

	if (strcmp(s_active_mapping_profile_id, mapping_profile_id) == 0) {
		xSemaphoreGive(s_input_lock);
		return ESP_ERR_INVALID_STATE;
	}

	memset(&s_mappings[index], 0, sizeof(s_mappings[index]));
	xSemaphoreGive(s_input_lock);
	return ESP_OK;
}

esp_err_t esptari_input_mapping_apply(const char *mapping_profile_id,
									  bool has_expected_revision,
									  uint32_t expected_revision,
									  esptari_input_apply_result_t *out_result)
{
	if (s_input_lock == NULL || str_empty(mapping_profile_id) || out_result == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	xSemaphoreTake(s_input_lock, portMAX_DELAY);
	int index = find_mapping_index(mapping_profile_id);
	if (index < 0) {
		xSemaphoreGive(s_input_lock);
		return ESP_ERR_NOT_FOUND;
	}

	mapping_slot_t *slot = &s_mappings[index];
	if (has_expected_revision && slot->mapping.revision != expected_revision) {
		xSemaphoreGive(s_input_lock);
		return ESP_ERR_INVALID_STATE;
	}

	memset(out_result, 0, sizeof(*out_result));
	out_result->has_active = true;
	strlcpy(out_result->previous_mapping_profile_id,
			str_empty(s_active_mapping_profile_id) ? "" : s_active_mapping_profile_id,
			sizeof(out_result->previous_mapping_profile_id));
	strlcpy(out_result->active_mapping_profile_id,
			slot->mapping.mapping_profile_id,
			sizeof(out_result->active_mapping_profile_id));
	out_result->active_mapping_revision = slot->mapping.revision;

	if (strcmp(s_active_mapping_profile_id, slot->mapping.mapping_profile_id) == 0) {
		out_result->no_op = true;
	} else {
		out_result->no_op = false;
		strlcpy(s_active_mapping_profile_id,
				slot->mapping.mapping_profile_id,
				sizeof(s_active_mapping_profile_id));
		s_apply_cutover_tick += 1;
	}
	out_result->cutover_tick = s_apply_cutover_tick;

	xSemaphoreGive(s_input_lock);
	return ESP_OK;
}

esp_err_t esptari_input_mapping_get_active(esptari_input_mapping_t *out_mapping)
{
	if (s_input_lock == NULL || out_mapping == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	xSemaphoreTake(s_input_lock, portMAX_DELAY);
	if (str_empty(s_active_mapping_profile_id)) {
		xSemaphoreGive(s_input_lock);
		return ESP_ERR_NOT_FOUND;
	}

	int index = find_mapping_index(s_active_mapping_profile_id);
	if (index < 0) {
		xSemaphoreGive(s_input_lock);
		return ESP_ERR_NOT_FOUND;
	}

	*out_mapping = s_mappings[index].mapping;
	xSemaphoreGive(s_input_lock);
	return ESP_OK;
}
