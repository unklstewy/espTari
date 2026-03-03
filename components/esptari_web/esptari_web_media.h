#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_http_server.h"

typedef struct {
	bool has_error;
	uint64_t event_seq;
	uint64_t event_timestamp_us;
	char media_id[64];
	char phase[16];
	char result[16];
	char request_id[48];
	char error_code[48];
	char error_message[128];
} esptari_web_media_attach_event_t;

typedef struct {
	bool has_disk_id;
	uint64_t event_seq;
	uint64_t event_timestamp_us;
	char drive[2];
	char state[16];
	char disk_id[64];
	char request_id[48];
} esptari_web_media_disk_state_event_t;

void esptari_web_media_register_routes(httpd_handle_t server_handle);
void esptari_web_media_get_last_rom_attach_events(const esptari_web_media_attach_event_t **out_events,
												  size_t *out_count);
void esptari_web_media_get_last_disk_state_events(const esptari_web_media_disk_state_event_t **out_events,
												  size_t *out_count);
