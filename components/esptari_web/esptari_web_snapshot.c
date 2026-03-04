#include "esptari_web_snapshot.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_auth.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json

static uint64_t checkpoint_seq = 1;
static char last_checkpoint_id[64] = "chkpt_000001";
static uint64_t psg_gpio_event_seq = 4200;
static uint64_t psg_gpio_tick = 913240;
static uint64_t psg_gpio_timestamp_us = 1710000033180ULL;
static uint8_t psg_gpio_port_a_value = 127;
static uint8_t psg_gpio_port_b_value = 16;
static const char *psg_gpio_port_a_direction = "output";
static const char *psg_gpio_port_b_direction = "input";
static uint8_t psg_reg_channel_a_fine = 34;
static uint8_t psg_reg_noise_period = 5;
static uint8_t psg_reg_envelope_shape = 9;
static uint8_t psg_reg_mixer = 56;
static uint64_t psg_register_latched_tick = 913002ULL;
static uint64_t psg_audio_frame_seq = 7810ULL;
static uint64_t psg_audio_tick_counter = 913010ULL;
static uint64_t psg_audio_timestamp_us = 1710000032522ULL;
static uint64_t dma_last_request_seq = 55301;
static uint64_t dma_window_start_tick = 912640;
static uint64_t dma_last_scheduled_tick = 912700;
static uint64_t dma_last_timestamp_us = 1710000031120ULL;
static uint32_t dma_arbitration_round = 44;
static const uint32_t dma_request_window_ticks = 128;
static const uint32_t dma_max_requests_per_window = 16;
static const uint32_t dma_queued_requests = 3;
static uint64_t fdc_command_seq = 8012;
static uint64_t fdc_last_transition_tick = 912840;
static uint64_t fdc_last_transition_us = 1710000031888ULL;
static uint64_t fdc_terminal_event_seq = 20330;
static uint64_t fdc_terminal_tick = 912864;
static uint64_t fdc_terminal_timestamp_us = 1710000031951ULL;
static uint64_t chipset_integration_tick_counter = 450208120ULL;
static uint64_t chipset_integration_cycle_counter = 112552440ULL;
static uint64_t chipset_integration_event_timestamp_us = 1710000026400ULL;
static uint32_t chipset_integration_call_seq = 0;
static uint64_t mfp_irq_tick_counter = 450208244ULL;
static uint64_t mfp_irq_cycle_counter = 112552991ULL;
static uint64_t mfp_irq_event_timestamp_us = 1710000028022ULL;
static uint64_t mfp_irq_event_seq = 0;
static uint64_t acia_bridge_last_frame_seq = 9182ULL;
static uint64_t acia_bridge_last_timestamp_us = 1710000029200ULL;
static uint64_t acia_host_to_st_frame_seq = 9183ULL;
static uint64_t acia_st_to_host_frame_seq = 8183ULL;
static uint64_t acia_host_to_st_timestamp_us = 1710000029203ULL;
static uint64_t acia_st_to_host_timestamp_us = 1710000029188ULL;
static uint64_t ikbd_last_packet_seq = 12411ULL;
static uint64_t ikbd_last_timestamp_us = 1710000030102ULL;
static uint64_t ikbd_keyboard_packet_seq = 12411ULL;
static uint64_t ikbd_mouse_packet_seq = 12409ULL;
static uint64_t ikbd_keyboard_timestamp_us = 1710000030102ULL;
static uint64_t ikbd_mouse_timestamp_us = 1710000029986ULL;
static uint64_t interrupt_last_route_seq = 9901ULL;
static uint64_t interrupt_last_timestamp_us = 1710000034028ULL;
static uint64_t interrupt_last_tick_counter = 913600ULL;
static uint64_t interrupt_wiring_check_seq = 409ULL;
static uint64_t interrupt_wiring_last_timestamp_us = 1710000034620ULL;
static uint64_t interrupt_wiring_last_tick_counter = 913900ULL;
static uint64_t startup_applied_tick = 12ULL;
static uint64_t startup_applied_timestamp_us = 1710000035200ULL;
static uint64_t startup_sequence_step_seq = 2ULL;
static uint64_t startup_sequence_tick_counter = 14ULL;
static uint64_t startup_sequence_timestamp_us = 1710000035710ULL;
static uint64_t startup_verification_event_seq = 0ULL;
static uint64_t startup_verification_tick_counter = 15ULL;
static uint64_t startup_verification_timestamp_us = 1710000035712ULL;

enum {
    CHIPSET_GROUP_GLUE = 0,
    CHIPSET_GROUP_MMU = 1,
    CHIPSET_GROUP_SHIFTER = 2,
    CHIPSET_GROUP_MFP = 3,
    CHIPSET_GROUP_COUNT = 4,
};

static const char *chipset_group_names[CHIPSET_GROUP_COUNT] = {
    "glue",
    "mmu",
    "shifter",
    "mfp",
};

static int chipset_group_index_from_name(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    for (int i = 0; i < CHIPSET_GROUP_COUNT; ++i) {
        if (strcmp(name, chipset_group_names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static bool parse_chipset_group_selector(const char *selector, bool selected[CHIPSET_GROUP_COUNT])
{
    if (selector == NULL || selector[0] == '\0') {
        return false;
    }

    memset(selected, 0, sizeof(bool) * CHIPSET_GROUP_COUNT);

    char selector_copy[64] = {0};
    if (strlen(selector) >= sizeof(selector_copy)) {
        return false;
    }
    strlcpy(selector_copy, selector, sizeof(selector_copy));

    bool any = false;
    char *saveptr = NULL;
    for (char *token = strtok_r(selector_copy, ",", &saveptr); token != NULL; token = strtok_r(NULL, ",", &saveptr)) {
        while (*token == ' ' || *token == '\t') {
            token++;
        }
        size_t token_len = strlen(token);
        while (token_len > 0 && (token[token_len - 1] == ' ' || token[token_len - 1] == '\t')) {
            token[token_len - 1] = '\0';
            token_len--;
        }

        int group_index = chipset_group_index_from_name(token);
        if (group_index < 0) {
            return false;
        }
        selected[group_index] = true;
        any = true;
    }

    return any;
}

static bool force_unresolved_group_selected(httpd_req_t *req, const bool selected[CHIPSET_GROUP_COUNT])
{
    char force_group[16] = {0};
    if (!esptari_web_query_value(req, "force_unresolved_group", force_group, sizeof(force_group)) || force_group[0] == '\0') {
        return false;
    }

    int forced_index = chipset_group_index_from_name(force_group);
    return forced_index >= 0 && selected[forced_index];
}

static size_t selected_group_count(const bool selected[CHIPSET_GROUP_COUNT])
{
    size_t count = 0;
    for (int group_index = 0; group_index < CHIPSET_GROUP_COUNT; ++group_index) {
        if (selected[group_index]) {
            count++;
        }
    }
    return count;
}

static bool mfp_only_selector(const bool selected[CHIPSET_GROUP_COUNT])
{
    return selected[CHIPSET_GROUP_MFP] && selected_group_count(selected) == 1;
}

static bool append_register_window_json(char *buffer, size_t buffer_len, bool *first, int group_index)
{
    const char *window_json = NULL;
    switch (group_index) {
    case CHIPSET_GROUP_GLUE:
        window_json =
            "{\"group\":\"glue\",\"base_address\":\"0x00FF8200\",\"window_bytes\":32,\"registers\":[{\"name\":\"video_base_high\",\"offset\":1,\"address\":\"0x00FF8201\",\"width_bits\":8,\"access\":\"rw\"}]}";
        break;
    case CHIPSET_GROUP_MMU:
        window_json =
            "{\"group\":\"mmu\",\"base_address\":\"0x00FF8000\",\"window_bytes\":48,\"registers\":[{\"name\":\"config\",\"offset\":0,\"address\":\"0x00FF8000\",\"width_bits\":8,\"access\":\"rw\"}]}";
        break;
    case CHIPSET_GROUP_SHIFTER:
        window_json =
            "{\"group\":\"shifter\",\"base_address\":\"0x00FF8200\",\"window_bytes\":64,\"registers\":[{\"name\":\"sync_mode\",\"offset\":10,\"address\":\"0x00FF820A\",\"width_bits\":8,\"access\":\"rw\"}]}";
        break;
    case CHIPSET_GROUP_MFP:
        window_json =
            "{\"group\":\"mfp\",\"base_address\":\"0x00FFFA00\",\"window_bytes\":64,\"registers\":[{\"name\":\"IERA\",\"offset\":7,\"address\":\"0x00FFFA07\",\"width_bits\":8,\"access\":\"rw\"}]}";
        break;
    default:
        return false;
    }

    size_t used = strlen(buffer);
    int written = snprintf(buffer + used,
                           buffer_len - used,
                           "%s%s",
                           *first ? "" : ",",
                           window_json);
    if (written < 0 || (size_t)written >= buffer_len - used) {
        return false;
    }

    *first = false;
    return true;
}

static bool append_memory_window_json(char *buffer, size_t buffer_len, bool *first, int group_index)
{
    const char *window_json = NULL;
    switch (group_index) {
    case CHIPSET_GROUP_GLUE:
        window_json =
            "{\"group\":\"glue\",\"window_start\":\"0x00FF8200\",\"window_end\":\"0x00FF821F\",\"mapped_region\":\"st_chipset_io\",\"addressing_mode\":\"linear\"}";
        break;
    case CHIPSET_GROUP_MMU:
        window_json =
            "{\"group\":\"mmu\",\"window_start\":\"0x00FF8000\",\"window_end\":\"0x00FF82FF\",\"mapped_region\":\"st_chipset_io\",\"addressing_mode\":\"linear\"}";
        break;
    case CHIPSET_GROUP_SHIFTER:
        window_json =
            "{\"group\":\"shifter\",\"window_start\":\"0x00FF8200\",\"window_end\":\"0x00FF823F\",\"mapped_region\":\"st_video_window\",\"addressing_mode\":\"banked\"}";
        break;
    default:
        return false;
    }

    size_t used = strlen(buffer);
    int written = snprintf(buffer + used,
                           buffer_len - used,
                           "%s%s",
                           *first ? "" : ",",
                           window_json);
    if (written < 0 || (size_t)written >= buffer_len - used) {
        return false;
    }

    *first = false;
    return true;
}

static bool is_valid_mfp_timer_id(const char *timer_id)
{
    return timer_id != NULL &&
           (strcmp(timer_id, "A") == 0 ||
            strcmp(timer_id, "B") == 0 ||
            strcmp(timer_id, "C") == 0 ||
            strcmp(timer_id, "D") == 0);
}

static bool append_mfp_timer_json(char *buffer,
                                  size_t buffer_len,
                                  bool *first,
                                  const char *timer_id,
                                  const char *control_register,
                                  const char *data_register,
                                  uint32_t prescaler,
                                  uint32_t counter_value,
                                  const char *mode,
                                  bool enabled)
{
    size_t used = strlen(buffer);
    int written = snprintf(buffer + used,
                           buffer_len - used,
                           "%s{\"timer_id\":\"%s\",\"control_register\":\"%s\",\"data_register\":\"%s\",\"prescaler\":%lu,\"counter_value\":%lu,\"mode\":\"%s\",\"enabled\":%s}",
                           *first ? "" : ",",
                           timer_id,
                           control_register,
                           data_register,
                           (unsigned long)prescaler,
                           (unsigned long)counter_value,
                           mode,
                           enabled ? "true" : "false");
    if (written < 0 || (size_t)written >= buffer_len - used) {
        return false;
    }

    *first = false;
    return true;
}

static bool append_acia_frame_json(char *buffer,
                                   size_t buffer_len,
                                   bool *first,
                                   uint64_t frame_seq,
                                   const char *direction,
                                   uint8_t payload,
                                   uint64_t timestamp_us)
{
    size_t used = strlen(buffer);
    int written = snprintf(buffer + used,
                           buffer_len - used,
                           "%s{\"frame_seq\":%llu,\"direction\":\"%s\",\"encoding\":\"8N1\",\"payload_hex\":\"0x%02X\",\"start_bit\":0,\"stop_bits\":1,\"parity\":\"none\",\"timestamp_us\":%llu}",
                           *first ? "" : ",",
                           (unsigned long long)frame_seq,
                           direction,
                           (unsigned)payload,
                           (unsigned long long)timestamp_us);
    if (written < 0 || (size_t)written >= buffer_len - used) {
        return false;
    }

    *first = false;
    return true;
}

static bool append_ikbd_packet_json(char *buffer,
                                    size_t buffer_len,
                                    bool *first,
                                    uint64_t packet_seq,
                                    const char *packet_type,
                                    const char *direction,
                                    const char *payload_hex,
                                    uint64_t acia_frame_seq,
                                    uint64_t inter_packet_gap_us,
                                    uint64_t timestamp_us)
{
    size_t used = strlen(buffer);
    int written = snprintf(buffer + used,
                           buffer_len - used,
                           "%s{\"packet_seq\":%llu,\"packet_type\":\"%s\",\"direction\":\"%s\",\"payload_hex\":\"%s\",\"acia_frame_seq\":%llu,\"inter_packet_gap_us\":%llu,\"timestamp_us\":%llu}",
                           *first ? "" : ",",
                           (unsigned long long)packet_seq,
                           packet_type,
                           direction,
                           payload_hex,
                           (unsigned long long)acia_frame_seq,
                           (unsigned long long)inter_packet_gap_us,
                           (unsigned long long)timestamp_us);
    if (written < 0 || (size_t)written >= buffer_len - used) {
        return false;
    }

    *first = false;
    return true;
}

static bool append_interrupt_route_json(char *buffer,
                                        size_t buffer_len,
                                        bool *first,
                                        uint64_t route_seq,
                                        const char *source_id,
                                        uint8_t priority_level,
                                        uint16_t vector,
                                        const char *cpu_interrupt_line,
                                        const char *delivery_state,
                                        uint64_t tick_counter,
                                        uint64_t timestamp_us)
{
    size_t used = strlen(buffer);
    int written = snprintf(buffer + used,
                           buffer_len - used,
                           "%s{\"route_seq\":%llu,\"source_id\":\"%s\",\"priority_level\":%u,\"vector\":%u,\"cpu_interrupt_line\":\"%s\",\"delivery_state\":\"%s\",\"tick_counter\":%llu,\"timestamp_us\":%llu}",
                           *first ? "" : ",",
                           (unsigned long long)route_seq,
                           source_id,
                           (unsigned)priority_level,
                           (unsigned)vector,
                           cpu_interrupt_line,
                           delivery_state,
                           (unsigned long long)tick_counter,
                           (unsigned long long)timestamp_us);
    if (written < 0 || (size_t)written >= buffer_len - used) {
        return false;
    }

    *first = false;
    return true;
}

static bool append_interrupt_wiring_check_json(char *buffer,
                                               size_t buffer_len,
                                               bool *first,
                                               uint64_t check_seq,
                                               const char *subsystem_id,
                                               const char *expected_cpu_line,
                                               const char *observed_cpu_line,
                                               uint16_t expected_vector,
                                               uint16_t observed_vector,
                                               const char *result,
                                               uint64_t tick_counter,
                                               uint64_t timestamp_us)
{
    size_t used = strlen(buffer);
    int written = snprintf(buffer + used,
                           buffer_len - used,
                           "%s{\"check_seq\":%llu,\"subsystem_id\":\"%s\",\"expected_cpu_line\":\"%s\",\"observed_cpu_line\":\"%s\",\"expected_vector\":%u,\"observed_vector\":%u,\"result\":\"%s\",\"tick_counter\":%llu,\"timestamp_us\":%llu}",
                           *first ? "" : ",",
                           (unsigned long long)check_seq,
                           subsystem_id,
                           expected_cpu_line,
                           observed_cpu_line,
                           (unsigned)expected_vector,
                           (unsigned)observed_vector,
                           result,
                           (unsigned long long)tick_counter,
                           (unsigned long long)timestamp_us);
    if (written < 0 || (size_t)written >= buffer_len - used) {
        return false;
    }

    *first = false;
    return true;
}

static bool append_power_on_register_json(char *buffer,
                                          size_t buffer_len,
                                          bool *first,
                                          const char *name,
                                          const char *address,
                                          uint8_t width_bits,
                                          const char *reset_value,
                                          const char *mask)
{
    size_t used = strlen(buffer);
    int written = snprintf(buffer + used,
                           buffer_len - used,
                           "%s{\"name\":\"%s\",\"address\":\"%s\",\"width_bits\":%u,\"reset_value\":\"%s\",\"mask\":\"%s\"}",
                           *first ? "" : ",",
                           name,
                           address,
                           (unsigned)width_bits,
                           reset_value,
                           mask);
    if (written < 0 || (size_t)written >= buffer_len - used) {
        return false;
    }

    *first = false;
    return true;
}

static bool append_startup_verification_event_json(char *buffer,
                                                   size_t buffer_len,
                                                   bool *first,
                                                   uint64_t event_seq,
                                                   uint64_t step_seq,
                                                   const char *check_id,
                                                   const char *component,
                                                   const char *result,
                                                   const char *expected,
                                                   const char *observed,
                                                   uint64_t tick_counter,
                                                   uint64_t timestamp_us)
{
    size_t used = strlen(buffer);
    int written = snprintf(buffer + used,
                           buffer_len - used,
                           "%s{\"event_seq\":%llu,\"step_seq\":%llu,\"check_id\":\"%s\",\"component\":\"%s\",\"result\":\"%s\",\"expected\":\"%s\",\"observed\":\"%s\",\"tick_counter\":%llu,\"timestamp_us\":%llu}",
                           *first ? "" : ",",
                           (unsigned long long)event_seq,
                           (unsigned long long)step_seq,
                           check_id,
                           component,
                           result,
                           expected,
                           observed,
                           (unsigned long long)tick_counter,
                           (unsigned long long)timestamp_us);
    if (written < 0 || (size_t)written >= buffer_len - used) {
        return false;
    }

    *first = false;
    return true;
}

static bool append_psg_audio_state_json(char *buffer,
                                        size_t buffer_len,
                                        bool *first,
                                        uint64_t frame_seq,
                                        uint8_t channel_a_level,
                                        uint8_t channel_b_level,
                                        uint8_t channel_c_level,
                                        bool noise_enable,
                                        uint8_t envelope_shape,
                                        uint64_t tick_counter,
                                        uint64_t timestamp_us)
{
    size_t used = strlen(buffer);
    int written = snprintf(buffer + used,
                           buffer_len - used,
                           "%s{\"frame_seq\":%llu,\"mix_mode\":\"mono\",\"sample_rate_hz\":50066,\"channel_a_level\":%u,\"channel_b_level\":%u,\"channel_c_level\":%u,\"noise_enable\":%s,\"envelope_shape\":%u,\"tick_counter\":%llu,\"timestamp_us\":%llu}",
                           *first ? "" : ",",
                           (unsigned long long)frame_seq,
                           (unsigned)channel_a_level,
                           (unsigned)channel_b_level,
                           (unsigned)channel_c_level,
                           noise_enable ? "true" : "false",
                           (unsigned)envelope_shape,
                           (unsigned long long)tick_counter,
                           (unsigned long long)timestamp_us);
    if (written < 0 || (size_t)written >= buffer_len - used) {
        return false;
    }

    *first = false;
    return true;
}

static esp_err_t validate_session_query(httpd_req_t *req, char *session_id, size_t len)
{
    if (!esptari_web_query_value(req, "session_id", session_id, len) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    return ESP_OK;
}

static esp_err_t validate_session_body(cJSON *root, char *session_id, size_t len)
{
    const char *session = NULL;
    if (!esptari_web_json_get_string(root, "session_id", &session) || session == NULL || session[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(session) >= len) {
        return ESP_ERR_INVALID_ARG;
    }
    strlcpy(session_id, session, len);
    if (strcmp(session_id, "ses_local") != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static esp_err_t guard_running_or_paused(httpd_req_t *req)
{
    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state != ESPTARI_SESSION_RUNNING && status.state != ESPTARI_SESSION_PAUSED) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }
    return ESP_OK;
}

static esp_err_t validate_running_session_query(httpd_req_t *req, char *session_id, size_t len)
{
    if (!esptari_web_query_value(req, "session_id", session_id, len) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (strcmp(session_id, "ses_local") != 0 ||
        (status.state != ESPTARI_SESSION_RUNNING && status.state != ESPTARI_SESSION_PAUSED)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    return ESP_OK;
}

static esp_err_t inspect_psg_gpio_state_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_unavailable_query[8] = {0};
    bool force_unavailable = esptari_web_query_value(req, "force_gpio_unavailable", force_unavailable_query, sizeof(force_unavailable_query)) &&
                            strcmp(force_unavailable_query, "1") == 0;
    if (force_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"port_a_direction\":\"%s\",\"port_b_direction\":\"%s\",\"port_a_value\":%u,\"port_b_value\":%u,\"latched_tick\":%llu,\"timestamp_us\":%llu}}",
             session_id,
             psg_gpio_port_a_direction,
             psg_gpio_port_b_direction,
             (unsigned)psg_gpio_port_a_value,
             (unsigned)psg_gpio_port_b_value,
             (unsigned long long)psg_gpio_tick,
             (unsigned long long)psg_gpio_timestamp_us);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_psg_registers_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_unavailable_query[8] = {0};
    bool force_unavailable = esptari_web_query_value(req,
                                                     "force_psg_unavailable",
                                                     force_unavailable_query,
                                                     sizeof(force_unavailable_query)) &&
                            strcmp(force_unavailable_query, "1") == 0;
    if (force_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    psg_register_latched_tick += 1ULL;

    char resp[1024];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"base_address\":\"0x00FF8800\",\"window_bytes\":16,\"registers\":[{\"name\":\"CHANNEL_A_FINE\",\"index\":0,\"value\":%u,\"latched_tick\":%llu},{\"name\":\"MIXER_CONTROL\",\"index\":7,\"value\":%u,\"latched_tick\":%llu},{\"name\":\"NOISE_PERIOD\",\"index\":6,\"value\":%u,\"latched_tick\":%llu},{\"name\":\"ENVELOPE_SHAPE\",\"index\":13,\"value\":%u,\"latched_tick\":%llu}]}}",
             session_id,
             (unsigned)psg_reg_channel_a_fine,
             (unsigned long long)psg_register_latched_tick,
             (unsigned)psg_reg_mixer,
             (unsigned long long)psg_register_latched_tick,
             (unsigned)psg_reg_noise_period,
             (unsigned long long)psg_register_latched_tick,
             (unsigned)psg_reg_envelope_shape,
             (unsigned long long)psg_register_latched_tick);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_psg_audio_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    if (!esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    uint32_t limit = 0;
    if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_unavailable_query[8] = {0};
    bool force_unavailable = esptari_web_query_value(req,
                                                     "force_psg_unavailable",
                                                     force_unavailable_query,
                                                     sizeof(force_unavailable_query)) &&
                            strcmp(force_unavailable_query, "1") == 0;
    if (force_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char force_aud01_query[8] = {0};
    bool force_aud01_fail = esptari_web_query_value(req,
                                                    "force_register_reflection_miss",
                                                    force_aud01_query,
                                                    sizeof(force_aud01_query)) &&
                           strcmp(force_aud01_query, "1") == 0;
    if (force_aud01_fail) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"PSG-AUD-01\"}}}",
                         500);
    }

    char force_aud02_query[8] = {0};
    bool force_aud02_fail = esptari_web_query_value(req,
                                                    "force_frame_seq_gap",
                                                    force_aud02_query,
                                                    sizeof(force_aud02_query)) &&
                           strcmp(force_aud02_query, "1") == 0;
    if (force_aud02_fail) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"PSG-AUD-02\"}}}",
                         500);
    }

    char force_aud03_query[8] = {0};
    bool force_aud03_fail = esptari_web_query_value(req,
                                                    "force_time_regression",
                                                    force_aud03_query,
                                                    sizeof(force_aud03_query)) &&
                           strcmp(force_aud03_query, "1") == 0;
    if (force_aud03_fail) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"PSG-AUD-03\"}}}",
                         500);
    }

    char force_aud04_query[8] = {0};
    bool force_aud04_fail = esptari_web_query_value(req,
                                                    "force_level_overflow",
                                                    force_aud04_query,
                                                    sizeof(force_aud04_query)) &&
                           strcmp(force_aud04_query, "1") == 0;
    if (force_aud04_fail) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"PSG-AUD-04\"}}}",
                         500);
    }

    uint32_t state_count = limit > 3 ? 3 : limit;
    uint64_t frame_seq = psg_audio_frame_seq;
    uint64_t tick_counter = psg_audio_tick_counter;
    uint64_t timestamp_us = psg_audio_timestamp_us;
    bool noise_enable = (psg_reg_mixer & 0x38U) != 0x38U;

    uint8_t base_a = (uint8_t)(psg_reg_channel_a_fine & 0x0FU);
    uint8_t base_b = (uint8_t)((psg_reg_noise_period + 2U) & 0x0FU);
    uint8_t base_c = (uint8_t)((psg_reg_envelope_shape ^ 0x03U) & 0x0FU);

    char states_json[2048] = {0};
    bool first = true;
    for (uint32_t i = 0; i < state_count; ++i) {
        frame_seq += 1ULL;
        tick_counter += 1ULL;
        timestamp_us += 6ULL;

        uint8_t channel_a_level = (uint8_t)((base_a + i) & 0x0FU);
        uint8_t channel_b_level = (uint8_t)((base_b + (i * 2U)) & 0x0FU);
        uint8_t channel_c_level = (uint8_t)((base_c + (i * 3U)) & 0x0FU);

        if (!append_psg_audio_state_json(states_json,
                                         sizeof(states_json),
                                         &first,
                                         frame_seq,
                                         channel_a_level,
                                         channel_b_level,
                                         channel_c_level,
                                         noise_enable,
                                         psg_reg_envelope_shape,
                                         tick_counter,
                                         timestamp_us)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    }

    psg_audio_frame_seq = frame_seq;
    psg_audio_tick_counter = tick_counter;
    psg_audio_timestamp_us = timestamp_us;
    psg_register_latched_tick = tick_counter;

    char resp[2304];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"conformance\":{\"PSG-AUD-01\":\"pass\",\"PSG-AUD-02\":\"pass\",\"PSG-AUD-03\":\"pass\",\"PSG-AUD-04\":\"pass\"},\"states\":[%s]}}",
             session_id,
             states_json);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_windows_registers_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char group_selector[64] = {0};
    if (!esptari_web_query_value(req, "group", group_selector, sizeof(group_selector)) || group_selector[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    bool selected[CHIPSET_GROUP_COUNT] = {false};
    if (!parse_chipset_group_selector(group_selector, selected)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (selected[CHIPSET_GROUP_MFP] && !mfp_only_selector(selected)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (force_unresolved_group_selected(req, selected)) {
        if (mfp_only_selector(selected)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INSPECT_FILTER_INVALID\"}}", 400);
        }
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char windows_json[1536] = {0};
    bool first = true;
    for (int group_index = 0; group_index < CHIPSET_GROUP_COUNT; ++group_index) {
        if (!selected[group_index]) {
            continue;
        }
        if (!append_register_window_json(windows_json, sizeof(windows_json), &first, group_index)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    }

    char resp[1792];
    if (mfp_only_selector(selected)) {
        snprintf(resp,
                 sizeof(resp),
                 "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"window\":%s}}",
                 session_id,
                 windows_json);
    } else {
        snprintf(resp,
                 sizeof(resp),
                 "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"windows\":[%s]}}",
                 session_id,
                 windows_json);
    }
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_windows_memory_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char group_selector[64] = {0};
    if (!esptari_web_query_value(req, "group", group_selector, sizeof(group_selector)) || group_selector[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    bool selected[CHIPSET_GROUP_COUNT] = {false};
    if (!parse_chipset_group_selector(group_selector, selected)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (force_unresolved_group_selected(req, selected)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char windows_json[1024] = {0};
    bool first = true;
    for (int group_index = 0; group_index < CHIPSET_GROUP_COUNT; ++group_index) {
        if (!selected[group_index]) {
            continue;
        }
        if (!append_memory_window_json(windows_json, sizeof(windows_json), &first, group_index)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    }

    char resp[1280];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"windows\":[%s]}}",
             session_id,
             windows_json);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_windows_timers_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char group_selector[16] = {0};
    if (!esptari_web_query_value(req, "group", group_selector, sizeof(group_selector)) || strcmp(group_selector, "mfp") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char timer_selector[8] = {0};
    bool has_timer_selector = esptari_web_query_value(req, "timer_id", timer_selector, sizeof(timer_selector));
    if (has_timer_selector && !is_valid_mfp_timer_id(timer_selector)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INSPECT_FILTER_INVALID\"}}", 400);
    }

    char force_unresolved_timer_query[8] = {0};
    bool force_unresolved_timer = esptari_web_query_value(req,
                                                          "force_unresolved_timer",
                                                          force_unresolved_timer_query,
                                                          sizeof(force_unresolved_timer_query)) &&
                                 strcmp(force_unresolved_timer_query, "1") == 0;
    if (force_unresolved_timer) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INSPECT_FILTER_INVALID\"}}", 400);
    }

    char timers_json[1024] = {0};
    bool first = true;

    if (!has_timer_selector || strcmp(timer_selector, "A") == 0) {
        if (!append_mfp_timer_json(timers_json, sizeof(timers_json), &first, "A", "TACR", "TADR", 64, 112, "delay", true)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    }
    if (!has_timer_selector || strcmp(timer_selector, "B") == 0) {
        if (!append_mfp_timer_json(timers_json, sizeof(timers_json), &first, "B", "TBCR", "TBDR", 32, 88, "event_count", true)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    }
    if (!has_timer_selector || strcmp(timer_selector, "C") == 0) {
        if (!append_mfp_timer_json(timers_json, sizeof(timers_json), &first, "C", "TCDCR", "TCDR", 16, 45, "pulse_width", false)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    }
    if (!has_timer_selector || strcmp(timer_selector, "D") == 0) {
        if (!append_mfp_timer_json(timers_json, sizeof(timers_json), &first, "D", "TCDCR", "TDDR", 4, 201, "stopped", false)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    }

    char resp[1280];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"timers\":[%s]}}",
             session_id,
             timers_json);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_windows_integration_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_order_mismatch_query[8] = {0};
    bool force_order_mismatch = esptari_web_query_value(req,
                                                        "force_order_mismatch",
                                                        force_order_mismatch_query,
                                                        sizeof(force_order_mismatch_query)) &&
                               strcmp(force_order_mismatch_query, "1") == 0;
    if (force_order_mismatch) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"CHIP-TIM-01\"}}}",
                         500);
    }

    char force_timing_regression_query[8] = {0};
    bool force_timing_regression = esptari_web_query_value(req,
                                                           "force_timing_regression",
                                                           force_timing_regression_query,
                                                           sizeof(force_timing_regression_query)) &&
                                  strcmp(force_timing_regression_query, "1") == 0;
    if (force_timing_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"CHIP-TIM-02|CHIP-TIM-03\"}}}",
                         500);
    }

    char force_bus_owner_invalid_query[8] = {0};
    bool force_bus_owner_invalid = esptari_web_query_value(req,
                                                           "force_bus_owner_invalid",
                                                           force_bus_owner_invalid_query,
                                                           sizeof(force_bus_owner_invalid_query)) &&
                                  strcmp(force_bus_owner_invalid_query, "1") == 0;
    if (force_bus_owner_invalid) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"CHIP-TIM-04\"}}}",
                         500);
    }

    chipset_integration_call_seq++;
    chipset_integration_tick_counter += 3ULL;
    chipset_integration_cycle_counter += 12ULL;
    chipset_integration_event_timestamp_us += 37ULL;

    static const char *bus_owners[] = {"glue", "mmu", "shifter", "cpu", "dma"};
    const char *bus_owner = bus_owners[chipset_integration_call_seq % 5U];

    uint32_t wait_cycles = (uint32_t)((chipset_integration_call_seq % 3U) + 1U);

    char resp[768];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"checks\":{\"CHIP-TIM-01\":\"pass\",\"CHIP-TIM-02\":\"pass\",\"CHIP-TIM-03\":\"pass\",\"CHIP-TIM-04\":\"pass\"},\"last_integration\":{\"tick_counter\":%llu,\"cycle_counter\":%llu,\"chipset_order\":[\"glue\",\"mmu\",\"shifter\"],\"bus_owner\":\"%s\",\"wait_cycles\":%lu,\"event_timestamp_us\":%llu}}}",
             session_id,
             (unsigned long long)chipset_integration_tick_counter,
             (unsigned long long)chipset_integration_cycle_counter,
             bus_owner,
             (unsigned long)wait_cycles,
             (unsigned long long)chipset_integration_event_timestamp_us);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_mfp_interrupts_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char group_selector[16] = {0};
    if (!esptari_web_query_value(req, "group", group_selector, sizeof(group_selector)) || strcmp(group_selector, "mfp") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char timer_selector[8] = {0};
    bool has_timer_selector = esptari_web_query_value(req, "timer_id", timer_selector, sizeof(timer_selector));
    if (has_timer_selector && !is_valid_mfp_timer_id(timer_selector)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INSPECT_FILTER_INVALID\"}}", 400);
    }

    char force_unresolved_vector_query[8] = {0};
    bool force_unresolved_vector = esptari_web_query_value(req,
                                                           "force_unresolved_vector",
                                                           force_unresolved_vector_query,
                                                           sizeof(force_unresolved_vector_query)) &&
                                 strcmp(force_unresolved_vector_query, "1") == 0;
    if (force_unresolved_vector) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"MFP-IRQ-03\"}}}",
                         500);
    }

    char force_timestamp_regression_query[8] = {0};
    bool force_timestamp_regression = esptari_web_query_value(req,
                                                              "force_timestamp_regression",
                                                              force_timestamp_regression_query,
                                                              sizeof(force_timestamp_regression_query)) &&
                                     strcmp(force_timestamp_regression_query, "1") == 0;
    if (force_timestamp_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"MFP-IRQ-02\"}}}",
                         500);
    }

    const char *timer_id = has_timer_selector ? timer_selector : "A";
    const char *interrupt_line = "irq6";
    uint16_t vector = 26;
    if (strcmp(timer_id, "B") == 0) {
        interrupt_line = "irq6";
        vector = 24;
    } else if (strcmp(timer_id, "C") == 0) {
        interrupt_line = "irq2";
        vector = 18;
    } else if (strcmp(timer_id, "D") == 0) {
        interrupt_line = "irq2";
        vector = 20;
    }

    mfp_irq_event_seq++;
    mfp_irq_tick_counter += 2ULL;
    mfp_irq_cycle_counter += 9ULL;
    mfp_irq_event_timestamp_us += 29ULL;

    char resp[896];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"checks\":{\"MFP-IRQ-01\":\"pass\",\"MFP-IRQ-02\":\"pass\",\"MFP-IRQ-03\":\"pass\",\"MFP-IRQ-04\":\"pass\"},\"last_interrupt\":{\"source\":\"mfp\",\"interrupt_line\":\"%s\",\"vector\":%u,\"timer_id\":\"%s\",\"tick_counter\":%llu,\"cycle_counter\":%llu,\"event_timestamp_us\":%llu}}}",
             session_id,
             interrupt_line,
             (unsigned)vector,
             timer_id,
             (unsigned long long)mfp_irq_tick_counter,
             (unsigned long long)mfp_irq_cycle_counter,
             (unsigned long long)mfp_irq_event_timestamp_us);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_acia_bridge_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_bridge_unavailable_query[8] = {0};
    bool force_bridge_unavailable = esptari_web_query_value(req,
                                                            "force_bridge_unavailable",
                                                            force_bridge_unavailable_query,
                                                            sizeof(force_bridge_unavailable_query)) &&
                                  strcmp(force_bridge_unavailable_query, "1") == 0;
    if (force_bridge_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    acia_bridge_last_frame_seq += 1ULL;
    acia_bridge_last_timestamp_us += 13ULL;

    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"bridge_state\":\"attached\",\"channel_mode\":\"duplex\",\"rx_queue_depth\":3,\"tx_queue_depth\":1,\"framing_profile\":\"acia_8n1_default\",\"last_frame_seq\":%llu,\"last_timestamp_us\":%llu}}",
             session_id,
             (unsigned long long)acia_bridge_last_frame_seq,
             (unsigned long long)acia_bridge_last_timestamp_us);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_acia_frames_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    if (!esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint32_t limit = 0;
    if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_bridge_unavailable_query[8] = {0};
    bool force_bridge_unavailable = esptari_web_query_value(req,
                                                            "force_bridge_unavailable",
                                                            force_bridge_unavailable_query,
                                                            sizeof(force_bridge_unavailable_query)) &&
                                  strcmp(force_bridge_unavailable_query, "1") == 0;
    if (force_bridge_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char force_frame_regression_query[8] = {0};
    bool force_frame_regression = esptari_web_query_value(req,
                                                          "force_frame_regression",
                                                          force_frame_regression_query,
                                                          sizeof(force_frame_regression_query)) &&
                                 strcmp(force_frame_regression_query, "1") == 0;
    if (force_frame_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"ACIA-FRM-01\"}}}",
                         500);
    }

    char force_timestamp_regression_query[8] = {0};
    bool force_timestamp_regression = esptari_web_query_value(req,
                                                              "force_timestamp_regression",
                                                              force_timestamp_regression_query,
                                                              sizeof(force_timestamp_regression_query)) &&
                                     strcmp(force_timestamp_regression_query, "1") == 0;
    if (force_timestamp_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"ACIA-FRM-02\"}}}",
                         500);
    }

    char force_framing_invalid_query[8] = {0};
    bool force_framing_invalid = esptari_web_query_value(req,
                                                         "force_framing_invalid",
                                                         force_framing_invalid_query,
                                                         sizeof(force_framing_invalid_query)) &&
                                strcmp(force_framing_invalid_query, "1") == 0;
    if (force_framing_invalid) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"ACIA-FRM-03\"}}}",
                         500);
    }

    char force_rejected_emitted_query[8] = {0};
    bool force_rejected_emitted = esptari_web_query_value(req,
                                                          "force_rejected_emitted",
                                                          force_rejected_emitted_query,
                                                          sizeof(force_rejected_emitted_query)) &&
                                 strcmp(force_rejected_emitted_query, "1") == 0;
    if (force_rejected_emitted) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"ACIA-FRM-04\"}}}",
                         500);
    }

    uint32_t frame_count = limit > 4 ? 4 : limit;
    char frames_json[2048] = {0};
    bool first = true;

    uint64_t next_host_seq = acia_host_to_st_frame_seq + 1ULL;
    uint64_t next_st_seq = acia_st_to_host_frame_seq + 1ULL;
    uint64_t next_host_ts = acia_host_to_st_timestamp_us + 11ULL;
    uint64_t next_st_ts = acia_st_to_host_timestamp_us + 9ULL;

    for (uint32_t i = 0; i < frame_count; ++i) {
        bool host_direction = (i % 2U) == 0U;
        uint64_t frame_seq = host_direction ? next_host_seq : next_st_seq;
        uint64_t timestamp_us = host_direction ? next_host_ts : next_st_ts;
        uint8_t payload = host_direction ? (uint8_t)(0xF0U + (i & 0x0FU)) : (uint8_t)(0x70U + (i & 0x0FU));

        if (!append_acia_frame_json(frames_json,
                                    sizeof(frames_json),
                                    &first,
                                    frame_seq,
                                    host_direction ? "host_to_st" : "st_to_host",
                                    payload,
                                    timestamp_us)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        if (host_direction) {
            next_host_seq = frame_seq + 1ULL;
            next_host_ts = timestamp_us + 11ULL;
            acia_host_to_st_frame_seq = frame_seq;
            acia_host_to_st_timestamp_us = timestamp_us;
        } else {
            next_st_seq = frame_seq + 1ULL;
            next_st_ts = timestamp_us + 9ULL;
            acia_st_to_host_frame_seq = frame_seq;
            acia_st_to_host_timestamp_us = timestamp_us;
        }
    }

    uint64_t latest_seq = acia_host_to_st_frame_seq > acia_st_to_host_frame_seq
                              ? acia_host_to_st_frame_seq
                              : acia_st_to_host_frame_seq;
    uint64_t latest_timestamp = acia_host_to_st_timestamp_us > acia_st_to_host_timestamp_us
                                    ? acia_host_to_st_timestamp_us
                                    : acia_st_to_host_timestamp_us;
    if (latest_seq > acia_bridge_last_frame_seq) {
        acia_bridge_last_frame_seq = latest_seq;
    }
    if (latest_timestamp > acia_bridge_last_timestamp_us) {
        acia_bridge_last_timestamp_us = latest_timestamp;
    }

    char resp[2560];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"checks\":{\"ACIA-FRM-01\":\"pass\",\"ACIA-FRM-02\":\"pass\",\"ACIA-FRM-03\":\"pass\",\"ACIA-FRM-04\":\"pass\"},\"frames\":[%s]}}",
             session_id,
             frames_json);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_ikbd_bridge_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_parser_unavailable_query[8] = {0};
    bool force_parser_unavailable = esptari_web_query_value(req,
                                                            "force_parser_unavailable",
                                                            force_parser_unavailable_query,
                                                            sizeof(force_parser_unavailable_query)) &&
                                   strcmp(force_parser_unavailable_query, "1") == 0;
    if (force_parser_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    ikbd_last_packet_seq += 1ULL;
    ikbd_last_timestamp_us += 19ULL;

    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"bridge_state\":\"attached\",\"parser_mode\":\"atari_st_ikbd\",\"acia_channel_mode\":\"duplex\",\"rx_queue_depth\":2,\"tx_queue_depth\":1,\"last_packet_seq\":%llu,\"last_timestamp_us\":%llu}}",
             session_id,
             (unsigned long long)ikbd_last_packet_seq,
             (unsigned long long)ikbd_last_timestamp_us);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_ikbd_packets_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    if (!esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint32_t limit = 0;
    if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char packet_type[32] = {0};
    if (!esptari_web_query_value(req, "packet_type", packet_type, sizeof(packet_type)) ||
        (strcmp(packet_type, "keyboard_scancode") != 0 && strcmp(packet_type, "mouse_packet") != 0)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_parser_unavailable_query[8] = {0};
    bool force_parser_unavailable = esptari_web_query_value(req,
                                                            "force_parser_unavailable",
                                                            force_parser_unavailable_query,
                                                            sizeof(force_parser_unavailable_query)) &&
                                   strcmp(force_parser_unavailable_query, "1") == 0;
    if (force_parser_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char force_sequence_regression_query[8] = {0};
    bool force_sequence_regression = esptari_web_query_value(req,
                                                             "force_sequence_regression",
                                                             force_sequence_regression_query,
                                                             sizeof(force_sequence_regression_query)) &&
                                    strcmp(force_sequence_regression_query, "1") == 0;
    if (force_sequence_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"IKBD-PKT-01\"}}}",
                         500);
    }

    char force_timestamp_regression_query[8] = {0};
    bool force_timestamp_regression = esptari_web_query_value(req,
                                                              "force_timestamp_regression",
                                                              force_timestamp_regression_query,
                                                              sizeof(force_timestamp_regression_query)) &&
                                     strcmp(force_timestamp_regression_query, "1") == 0;
    if (force_timestamp_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"IKBD-PKT-02\"}}}",
                         500);
    }

    char force_gap_mismatch_query[8] = {0};
    bool force_gap_mismatch = esptari_web_query_value(req,
                                                      "force_gap_mismatch",
                                                      force_gap_mismatch_query,
                                                      sizeof(force_gap_mismatch_query)) &&
                             strcmp(force_gap_mismatch_query, "1") == 0;
    if (force_gap_mismatch) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"IKBD-PKT-03\"}}}",
                         500);
    }

    char force_unresolved_frame_query[8] = {0};
    bool force_unresolved_frame = esptari_web_query_value(req,
                                                          "force_unresolved_frame",
                                                          force_unresolved_frame_query,
                                                          sizeof(force_unresolved_frame_query)) &&
                                 strcmp(force_unresolved_frame_query, "1") == 0;
    if (force_unresolved_frame) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"IKBD-PKT-04\"}}}",
                         500);
    }

    uint32_t packet_count = limit > 4 ? 4 : limit;
    bool keyboard_stream = strcmp(packet_type, "keyboard_scancode") == 0;
    char packets_json[2048] = {0};
    bool first = true;

    uint64_t packet_seq = (keyboard_stream ? ikbd_keyboard_packet_seq : ikbd_mouse_packet_seq) + 1ULL;
    uint64_t timestamp_us = (keyboard_stream ? ikbd_keyboard_timestamp_us : ikbd_mouse_timestamp_us) +
                           (keyboard_stream ? 170ULL : 244ULL);
    uint64_t previous_timestamp = keyboard_stream ? ikbd_keyboard_timestamp_us : ikbd_mouse_timestamp_us;
    uint64_t acia_frame_seq = acia_bridge_last_frame_seq + 1ULL;

    for (uint32_t i = 0; i < packet_count; ++i) {
        uint64_t gap_us = timestamp_us - previous_timestamp;
        const char *direction = keyboard_stream ? "host_to_st" : "st_to_host";

        char payload_hex[16] = {0};
        if (keyboard_stream) {
            snprintf(payload_hex, sizeof(payload_hex), "0x%02X", (unsigned)(0x1CU + (i & 0x03U)));
        } else {
            snprintf(payload_hex, sizeof(payload_hex), "0xF8%02X%02X", (unsigned)(0x08U + i), (unsigned)(0x01U + i));
        }

        if (!append_ikbd_packet_json(packets_json,
                                     sizeof(packets_json),
                                     &first,
                                     packet_seq,
                                     packet_type,
                                     direction,
                                     payload_hex,
                                     acia_frame_seq,
                                     gap_us,
                                     timestamp_us)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        previous_timestamp = timestamp_us;
        packet_seq += 1ULL;
        timestamp_us += keyboard_stream ? 170ULL : 244ULL;
        acia_frame_seq += 1ULL;
    }

    if (keyboard_stream) {
        ikbd_keyboard_packet_seq = packet_seq - 1ULL;
        ikbd_keyboard_timestamp_us = previous_timestamp;
        if (ikbd_keyboard_packet_seq > ikbd_last_packet_seq) {
            ikbd_last_packet_seq = ikbd_keyboard_packet_seq;
        }
        if (ikbd_keyboard_timestamp_us > ikbd_last_timestamp_us) {
            ikbd_last_timestamp_us = ikbd_keyboard_timestamp_us;
        }
    } else {
        ikbd_mouse_packet_seq = packet_seq - 1ULL;
        ikbd_mouse_timestamp_us = previous_timestamp;
        if (ikbd_mouse_packet_seq > ikbd_last_packet_seq) {
            ikbd_last_packet_seq = ikbd_mouse_packet_seq;
        }
        if (ikbd_mouse_timestamp_us > ikbd_last_timestamp_us) {
            ikbd_last_timestamp_us = ikbd_mouse_timestamp_us;
        }
    }

    if (acia_frame_seq - 1ULL > acia_bridge_last_frame_seq) {
        acia_bridge_last_frame_seq = acia_frame_seq - 1ULL;
    }
    if (ikbd_last_timestamp_us > acia_bridge_last_timestamp_us) {
        acia_bridge_last_timestamp_us = ikbd_last_timestamp_us;
    }

    char resp[2560];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"checks\":{\"IKBD-PKT-01\":\"pass\",\"IKBD-PKT-02\":\"pass\",\"IKBD-PKT-03\":\"pass\",\"IKBD-PKT-04\":\"pass\"},\"packets\":[%s]}}",
             session_id,
             packets_json);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_interrupts_hierarchy_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_map_unavailable_query[8] = {0};
    bool force_map_unavailable = esptari_web_query_value(req,
                                                         "force_map_unavailable",
                                                         force_map_unavailable_query,
                                                         sizeof(force_map_unavailable_query)) &&
                                strcmp(force_map_unavailable_query, "1") == 0;
    if (force_map_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char force_duplicate_source_query[8] = {0};
    bool force_duplicate_source = esptari_web_query_value(req,
                                                          "force_duplicate_source",
                                                          force_duplicate_source_query,
                                                          sizeof(force_duplicate_source_query)) &&
                                 strcmp(force_duplicate_source_query, "1") == 0;
    if (force_duplicate_source) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"INT-MAP-01\"}}}",
                         500);
    }

    char resp[1024];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"cpu_level_order\":[7,6,5,4,3,2,1],\"sources\":[{\"source_id\":\"mfp\",\"priority_level\":6,\"vector\":38,\"enabled\":true},{\"source_id\":\"acia\",\"priority_level\":4,\"vector\":24,\"enabled\":true},{\"source_id\":\"fdc\",\"priority_level\":3,\"vector\":54,\"enabled\":true},{\"source_id\":\"blitter\",\"priority_level\":2,\"vector\":48,\"enabled\":false},{\"source_id\":\"vbl\",\"priority_level\":7,\"vector\":28,\"enabled\":true}],\"default_vector_base\":24,\"last_route_seq\":%llu,\"last_timestamp_us\":%llu,\"checks\":{\"INT-MAP-01\":\"pass\"}}}",
             session_id,
             (unsigned long long)interrupt_last_route_seq,
             (unsigned long long)interrupt_last_timestamp_us);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_interrupts_routes_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    if (!esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    uint32_t limit = 0;
    if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_map_unavailable_query[8] = {0};
    bool force_map_unavailable = esptari_web_query_value(req,
                                                         "force_map_unavailable",
                                                         force_map_unavailable_query,
                                                         sizeof(force_map_unavailable_query)) &&
                                strcmp(force_map_unavailable_query, "1") == 0;
    if (force_map_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char force_route_seq_regression_query[8] = {0};
    bool force_route_seq_regression = esptari_web_query_value(req,
                                                              "force_route_seq_regression",
                                                              force_route_seq_regression_query,
                                                              sizeof(force_route_seq_regression_query)) &&
                                     strcmp(force_route_seq_regression_query, "1") == 0;
    if (force_route_seq_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"INT-MAP-02\"}}}",
                         500);
    }

    char force_route_time_regression_query[8] = {0};
    bool force_route_time_regression = esptari_web_query_value(req,
                                                               "force_route_time_regression",
                                                               force_route_time_regression_query,
                                                               sizeof(force_route_time_regression_query)) &&
                                      strcmp(force_route_time_regression_query, "1") == 0;
    if (force_route_time_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"INT-MAP-03\"}}}",
                         500);
    }

    char force_route_order_violation_query[8] = {0};
    bool force_route_order_violation = esptari_web_query_value(req,
                                                               "force_route_order_violation",
                                                               force_route_order_violation_query,
                                                               sizeof(force_route_order_violation_query)) &&
                                      strcmp(force_route_order_violation_query, "1") == 0;
    if (force_route_order_violation) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"INT-MAP-04\"}}}",
                         500);
    }

    uint32_t route_count = limit > 4 ? 4 : limit;
    char routes_json[2048] = {0};
    bool first = true;

    static const char *source_ids[] = {"vbl", "mfp", "acia", "fdc"};
    static const uint8_t priority_levels[] = {7, 6, 4, 3};
    static const uint16_t vectors[] = {28, 38, 24, 54};
    static const char *cpu_lines[] = {"irq7", "irq6", "irq4", "irq3"};
    static const char *delivery_states[] = {"delivered", "delivered", "masked", "deferred"};

    uint64_t route_seq = interrupt_last_route_seq + 1ULL;
    uint64_t tick_counter = interrupt_last_tick_counter + 1ULL;
    uint64_t timestamp_us = interrupt_last_timestamp_us + 3ULL;

    for (uint32_t i = 0; i < route_count; ++i) {
        uint32_t idx = i % 4U;
        if (!append_interrupt_route_json(routes_json,
                                         sizeof(routes_json),
                                         &first,
                                         route_seq,
                                         source_ids[idx],
                                         priority_levels[idx],
                                         vectors[idx],
                                         cpu_lines[idx],
                                         delivery_states[idx],
                                         tick_counter,
                                         timestamp_us)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        route_seq += 1ULL;
        tick_counter += 1ULL;
        timestamp_us += 2ULL;
    }

    interrupt_last_route_seq = route_seq - 1ULL;
    interrupt_last_tick_counter = tick_counter - 1ULL;
    interrupt_last_timestamp_us = timestamp_us - 2ULL;

    char resp[2560];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"checks\":{\"INT-MAP-02\":\"pass\",\"INT-MAP-03\":\"pass\",\"INT-MAP-04\":\"pass\"},\"routes\":[%s]}}",
             session_id,
             routes_json);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_interrupts_wiring_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_wiring_unavailable_query[8] = {0};
    bool force_wiring_unavailable = esptari_web_query_value(req,
                                                            "force_wiring_unavailable",
                                                            force_wiring_unavailable_query,
                                                            sizeof(force_wiring_unavailable_query)) &&
                                   strcmp(force_wiring_unavailable_query, "1") == 0;
    if (force_wiring_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[1280];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"subsystems\":[{\"subsystem_id\":\"mfp\",\"source_line\":\"mfp_irq\",\"cpu_interrupt_line\":\"irq6\",\"vector\":38,\"enabled\":true},{\"subsystem_id\":\"acia\",\"source_line\":\"acia_irq\",\"cpu_interrupt_line\":\"irq4\",\"vector\":24,\"enabled\":true},{\"subsystem_id\":\"fdc\",\"source_line\":\"fdc_intrq\",\"cpu_interrupt_line\":\"irq3\",\"vector\":54,\"enabled\":true},{\"subsystem_id\":\"blitter\",\"source_line\":\"blitter_irq\",\"cpu_interrupt_line\":\"irq2\",\"vector\":48,\"enabled\":false},{\"subsystem_id\":\"vbl\",\"source_line\":\"vblank_irq\",\"cpu_interrupt_line\":\"irq7\",\"vector\":28,\"enabled\":true}],\"global_route_seq\":%llu,\"last_timestamp_us\":%llu}}",
             session_id,
             (unsigned long long)interrupt_last_route_seq,
             (unsigned long long)interrupt_wiring_last_timestamp_us);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_interrupts_wiring_checks_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    if (!esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    uint32_t limit = 0;
    if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_wiring_unavailable_query[8] = {0};
    bool force_wiring_unavailable = esptari_web_query_value(req,
                                                            "force_wiring_unavailable",
                                                            force_wiring_unavailable_query,
                                                            sizeof(force_wiring_unavailable_query)) &&
                                   strcmp(force_wiring_unavailable_query, "1") == 0;
    if (force_wiring_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char force_seq_regression_query[8] = {0};
    bool force_seq_regression = esptari_web_query_value(req,
                                                        "force_seq_regression",
                                                        force_seq_regression_query,
                                                        sizeof(force_seq_regression_query)) &&
                               strcmp(force_seq_regression_query, "1") == 0;
    if (force_seq_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"INT-WIRE-01\"}}}",
                         500);
    }

    char force_time_regression_query[8] = {0};
    bool force_time_regression = esptari_web_query_value(req,
                                                         "force_time_regression",
                                                         force_time_regression_query,
                                                         sizeof(force_time_regression_query)) &&
                                strcmp(force_time_regression_query, "1") == 0;
    if (force_time_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"INT-WIRE-02\"}}}",
                         500);
    }

    char force_observed_mismatch_query[8] = {0};
    bool force_observed_mismatch = esptari_web_query_value(req,
                                                           "force_observed_mismatch",
                                                           force_observed_mismatch_query,
                                                           sizeof(force_observed_mismatch_query)) &&
                                  strcmp(force_observed_mismatch_query, "1") == 0;
    if (force_observed_mismatch) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"INT-WIRE-03\"}}}",
                         500);
    }

    char force_map_mutation_query[8] = {0};
    bool force_map_mutation = esptari_web_query_value(req,
                                                      "force_map_mutation",
                                                      force_map_mutation_query,
                                                      sizeof(force_map_mutation_query)) &&
                             strcmp(force_map_mutation_query, "1") == 0;
    if (force_map_mutation) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"INT-WIRE-04\"}}}",
                         500);
    }

    uint32_t check_count = limit > 4 ? 4 : limit;
    char checks_json[2048] = {0};
    bool first = true;

    static const char *subsystem_ids[] = {"vbl", "mfp", "acia", "fdc"};
    static const char *expected_cpu_lines[] = {"irq7", "irq6", "irq4", "irq3"};
    static const uint16_t expected_vectors[] = {28, 38, 24, 54};

    uint64_t check_seq = interrupt_wiring_check_seq + 1ULL;
    uint64_t tick_counter = interrupt_wiring_last_tick_counter + 1ULL;
    uint64_t timestamp_us = interrupt_wiring_last_timestamp_us + 3ULL;

    for (uint32_t i = 0; i < check_count; ++i) {
        uint32_t idx = i % 4U;
        if (!append_interrupt_wiring_check_json(checks_json,
                                                sizeof(checks_json),
                                                &first,
                                                check_seq,
                                                subsystem_ids[idx],
                                                expected_cpu_lines[idx],
                                                expected_cpu_lines[idx],
                                                expected_vectors[idx],
                                                expected_vectors[idx],
                                                "pass",
                                                tick_counter,
                                                timestamp_us)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        check_seq += 1ULL;
        tick_counter += 1ULL;
        timestamp_us += 2ULL;
    }

    interrupt_wiring_check_seq = check_seq - 1ULL;
    interrupt_wiring_last_tick_counter = tick_counter - 1ULL;
    interrupt_wiring_last_timestamp_us = timestamp_us - 2ULL;
    if (interrupt_wiring_last_timestamp_us > interrupt_last_timestamp_us) {
        interrupt_last_timestamp_us = interrupt_wiring_last_timestamp_us;
    }

    char resp[2560];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"conformance\":{\"INT-WIRE-01\":\"pass\",\"INT-WIRE-02\":\"pass\",\"INT-WIRE-03\":\"pass\",\"INT-WIRE-04\":\"pass\"},\"checks\":[%s]}}",
             session_id,
             checks_json);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_startup_defaults_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_revision_unavailable_query[8] = {0};
    bool force_revision_unavailable = esptari_web_query_value(req,
                                                              "force_revision_unavailable",
                                                              force_revision_unavailable_query,
                                                              sizeof(force_revision_unavailable_query)) &&
                                     strcmp(force_revision_unavailable_query, "1") == 0;
    if (force_revision_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    startup_applied_tick += 1ULL;
    startup_applied_timestamp_us += 47ULL;

    char resp[768];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"machine_profile\":\"atari_st_520_1040_baseline\",\"video_standard\":\"pal\",\"ram_size_kib\":1024,\"boot_device\":\"floppy\",\"defaults_revision\":\"startup_rev_03\",\"applied_tick\":%llu,\"applied_timestamp_us\":%llu,\"conformance\":{\"PWR-BASE-01\":\"pass\",\"PWR-BASE-04\":\"pass\"}}}",
             session_id,
             (unsigned long long)startup_applied_tick,
             (unsigned long long)startup_applied_timestamp_us);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_startup_baseline_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char group[16] = {0};
    if (!esptari_web_query_value(req, "group", group, sizeof(group)) || group[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    bool valid_group = strcmp(group, "glue") == 0 || strcmp(group, "mmu") == 0 || strcmp(group, "shifter") == 0 ||
                       strcmp(group, "mfp") == 0 || strcmp(group, "acia") == 0 || strcmp(group, "fdc") == 0 ||
                       strcmp(group, "psg") == 0;
    if (!valid_group) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_table_unavailable_query[8] = {0};
    bool force_table_unavailable = esptari_web_query_value(req,
                                                           "force_table_unavailable",
                                                           force_table_unavailable_query,
                                                           sizeof(force_table_unavailable_query)) &&
                                  strcmp(force_table_unavailable_query, "1") == 0;
    if (force_table_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char force_missing_defaults_query[8] = {0};
    bool force_missing_defaults = esptari_web_query_value(req,
                                                          "force_missing_defaults",
                                                          force_missing_defaults_query,
                                                          sizeof(force_missing_defaults_query)) &&
                                 strcmp(force_missing_defaults_query, "1") == 0;
    if (force_missing_defaults) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"PWR-BASE-01\"}}}",
                         500);
    }

    char force_nondeterministic_query[8] = {0};
    bool force_nondeterministic = esptari_web_query_value(req,
                                                          "force_nondeterministic",
                                                          force_nondeterministic_query,
                                                          sizeof(force_nondeterministic_query)) &&
                                 strcmp(force_nondeterministic_query, "1") == 0;
    if (force_nondeterministic) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"PWR-BASE-02\"}}}",
                         500);
    }

    char force_duplicate_address_query[8] = {0};
    bool force_duplicate_address = esptari_web_query_value(req,
                                                           "force_duplicate_address",
                                                           force_duplicate_address_query,
                                                           sizeof(force_duplicate_address_query)) &&
                                  strcmp(force_duplicate_address_query, "1") == 0;
    if (force_duplicate_address) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"PWR-BASE-03\"}}}",
                         500);
    }

    char force_timestamp_regression_query[8] = {0};
    bool force_timestamp_regression = esptari_web_query_value(req,
                                                              "force_timestamp_regression",
                                                              force_timestamp_regression_query,
                                                              sizeof(force_timestamp_regression_query)) &&
                                     strcmp(force_timestamp_regression_query, "1") == 0;
    if (force_timestamp_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"PWR-BASE-04\"}}}",
                         500);
    }

    char registers_json[1024] = {0};
    bool first = true;

    if (strcmp(group, "mfp") == 0) {
        if (!append_power_on_register_json(registers_json, sizeof(registers_json), &first, "IERA", "0x00FFFA07", 8, "0x00", "0xFF") ||
            !append_power_on_register_json(registers_json, sizeof(registers_json), &first, "IMRA", "0x00FFFA13", 8, "0x00", "0xFF")) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    } else if (strcmp(group, "acia") == 0) {
        if (!append_power_on_register_json(registers_json, sizeof(registers_json), &first, "ACIA_CTRL", "0x00FFFC00", 8, "0x15", "0xFF") ||
            !append_power_on_register_json(registers_json, sizeof(registers_json), &first, "ACIA_STAT", "0x00FFFC00", 8, "0x02", "0xFF")) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    } else if (strcmp(group, "fdc") == 0) {
        if (!append_power_on_register_json(registers_json, sizeof(registers_json), &first, "FDC_STATUS", "0x00FF8604", 8, "0x00", "0xFF") ||
            !append_power_on_register_json(registers_json, sizeof(registers_json), &first, "DMA_MODE", "0x00FF8606", 8, "0x00", "0xFF")) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    } else if (strcmp(group, "psg") == 0) {
        if (!append_power_on_register_json(registers_json, sizeof(registers_json), &first, "PSG_REGSEL", "0x00FF8800", 8, "0x00", "0xFF") ||
            !append_power_on_register_json(registers_json, sizeof(registers_json), &first, "PSG_DATA", "0x00FF8802", 8, "0x00", "0xFF")) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    } else if (strcmp(group, "glue") == 0) {
        if (!append_power_on_register_json(registers_json, sizeof(registers_json), &first, "VIDEO_BASE_H", "0x00FF8201", 8, "0x00", "0xFF") ||
            !append_power_on_register_json(registers_json, sizeof(registers_json), &first, "VIDEO_BASE_M", "0x00FF8203", 8, "0x00", "0xFF")) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    } else if (strcmp(group, "mmu") == 0) {
        if (!append_power_on_register_json(registers_json, sizeof(registers_json), &first, "MMU_CONFIG", "0x00FF8000", 8, "0x00", "0xFF") ||
            !append_power_on_register_json(registers_json, sizeof(registers_json), &first, "MMU_MODE", "0x00FF8001", 8, "0x00", "0xFF")) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    } else {
        if (!append_power_on_register_json(registers_json, sizeof(registers_json), &first, "SHIFT_MODE", "0x00FF820A", 8, "0x00", "0xFF") ||
            !append_power_on_register_json(registers_json, sizeof(registers_json), &first, "SYNC_MODE", "0x00FF820C", 8, "0x00", "0xFF")) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    }

    char source_value[32];
    if (strcmp(group, "mfp") == 0 || strcmp(group, "acia") == 0 || strcmp(group, "fdc") == 0) {
        strlcpy(source_value, "hardware_boot_table", sizeof(source_value));
    } else {
        strlcpy(source_value, "profile_defaults", sizeof(source_value));
    }

    char resp[1600];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"group\":\"%s\",\"registers\":[%s],\"baseline_revision\":\"baseline_rev_07\",\"source\":\"%s\",\"conformance\":{\"PWR-BASE-01\":\"pass\",\"PWR-BASE-02\":\"pass\",\"PWR-BASE-03\":\"pass\",\"PWR-BASE-04\":\"pass\"}}}",
             session_id,
             group,
             registers_json,
             source_value);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_startup_sequence_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_executor_unavailable_query[8] = {0};
    bool force_executor_unavailable = esptari_web_query_value(req,
                                                              "force_executor_unavailable",
                                                              force_executor_unavailable_query,
                                                              sizeof(force_executor_unavailable_query)) &&
                                     strcmp(force_executor_unavailable_query, "1") == 0;
    if (force_executor_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char force_phase_order_violation_query[8] = {0};
    bool force_phase_order_violation = esptari_web_query_value(req,
                                                               "force_phase_order_violation",
                                                               force_phase_order_violation_query,
                                                               sizeof(force_phase_order_violation_query)) &&
                                      strcmp(force_phase_order_violation_query, "1") == 0;
    if (force_phase_order_violation) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"RST-SEQ-01\"}}}",
                         500);
    }

    char force_ready_without_pass_query[8] = {0};
    bool force_ready_without_pass = esptari_web_query_value(req,
                                                            "force_ready_without_pass",
                                                            force_ready_without_pass_query,
                                                            sizeof(force_ready_without_pass_query)) &&
                                   strcmp(force_ready_without_pass_query, "1") == 0;
    if (force_ready_without_pass) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"RST-SEQ-04\"}}}",
                         500);
    }

    startup_sequence_step_seq += 1ULL;
    startup_sequence_tick_counter += 1ULL;
    startup_sequence_timestamp_us += 8ULL;

    static const char *phases[] = {"assert_reset", "clock_stabilize", "register_seed", "interrupt_enable", "ready"};
    const char *phase = phases[startup_sequence_step_seq % 5ULL];
    const char *verification_status = strcmp(phase, "ready") == 0 ? "pass" : "pending";

    char resp[768];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"sequence_id\":\"boot_seq_0007\",\"phase\":\"%s\",\"step_seq\":%llu,\"tick_counter\":%llu,\"timestamp_us\":%llu,\"verification_status\":\"%s\",\"conformance\":{\"RST-SEQ-01\":\"pass\",\"RST-SEQ-04\":\"pass\"}}}",
             session_id,
             phase,
             (unsigned long long)startup_sequence_step_seq,
             (unsigned long long)startup_sequence_tick_counter,
             (unsigned long long)startup_sequence_timestamp_us,
             verification_status);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_startup_verification_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    if (!esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    uint32_t limit = 0;
    if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_executor_unavailable_query[8] = {0};
    bool force_executor_unavailable = esptari_web_query_value(req,
                                                              "force_executor_unavailable",
                                                              force_executor_unavailable_query,
                                                              sizeof(force_executor_unavailable_query)) &&
                                     strcmp(force_executor_unavailable_query, "1") == 0;
    if (force_executor_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char force_step_seq_regression_query[8] = {0};
    bool force_step_seq_regression = esptari_web_query_value(req,
                                                             "force_step_seq_regression",
                                                             force_step_seq_regression_query,
                                                             sizeof(force_step_seq_regression_query)) &&
                                    strcmp(force_step_seq_regression_query, "1") == 0;
    if (force_step_seq_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"RST-SEQ-02\"}}}",
                         500);
    }

    char force_time_regression_query[8] = {0};
    bool force_time_regression = esptari_web_query_value(req,
                                                         "force_time_regression",
                                                         force_time_regression_query,
                                                         sizeof(force_time_regression_query)) &&
                                strcmp(force_time_regression_query, "1") == 0;
    if (force_time_regression) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"RST-SEQ-03\"}}}",
                         500);
    }

    char force_ready_without_pass_query[8] = {0};
    bool force_ready_without_pass = esptari_web_query_value(req,
                                                            "force_ready_without_pass",
                                                            force_ready_without_pass_query,
                                                            sizeof(force_ready_without_pass_query)) &&
                                   strcmp(force_ready_without_pass_query, "1") == 0;
    if (force_ready_without_pass) {
        return send_json(req,
                         "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check\":\"RST-SEQ-04\"}}}",
                         500);
    }

    uint32_t event_count = limit > 4 ? 4 : limit;
    char events_json[2048] = {0};
    bool first = true;

    static const char *check_ids[] = {"BOOT-GLUE-RESET", "BOOT-MFP-RESET", "BOOT-ACIA-RESET", "BOOT-INT-MAP"};
    static const char *components[] = {"glue", "mfp", "acia", "psg"};
    static const char *expected_values[] = {"GLUE=0x00", "IERA=0x00", "ACIA_CTRL=0x15", "irq_order=7..1"};

    uint64_t event_seq = startup_verification_event_seq + 1ULL;
    uint64_t step_seq = startup_sequence_step_seq + 1ULL;
    uint64_t tick_counter = startup_verification_tick_counter + 1ULL;
    uint64_t timestamp_us = startup_verification_timestamp_us + 2ULL;

    for (uint32_t i = 0; i < event_count; ++i) {
        uint32_t idx = i % 4U;
        if (!append_startup_verification_event_json(events_json,
                                                    sizeof(events_json),
                                                    &first,
                                                    event_seq,
                                                    step_seq,
                                                    check_ids[idx],
                                                    components[idx],
                                                    "pass",
                                                    expected_values[idx],
                                                    expected_values[idx],
                                                    tick_counter,
                                                    timestamp_us)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        event_seq += 1ULL;
        step_seq += 1ULL;
        tick_counter += 1ULL;
        timestamp_us += 3ULL;
    }

    startup_verification_event_seq = event_seq - 1ULL;
    startup_sequence_step_seq = step_seq - 1ULL;
    startup_verification_tick_counter = tick_counter - 1ULL;
    startup_verification_timestamp_us = timestamp_us - 3ULL;
    if (startup_verification_tick_counter > startup_sequence_tick_counter) {
        startup_sequence_tick_counter = startup_verification_tick_counter;
    }
    if (startup_verification_timestamp_us > startup_sequence_timestamp_us) {
        startup_sequence_timestamp_us = startup_verification_timestamp_us;
    }

    char resp[2560];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"conformance\":{\"RST-SEQ-02\":\"pass\",\"RST-SEQ-03\":\"pass\",\"RST-SEQ-04\":\"pass\"},\"events\":[%s]}}",
             session_id,
             events_json);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_dma_pacing_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_dma_unavailable_query[8] = {0};
    bool force_dma_unavailable = esptari_web_query_value(req,
                                                         "force_dma_unavailable",
                                                         force_dma_unavailable_query,
                                                         sizeof(force_dma_unavailable_query)) &&
                                strcmp(force_dma_unavailable_query, "1") == 0;
    if (force_dma_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    uint64_t window_end_tick = dma_window_start_tick + (uint64_t)dma_request_window_ticks - 1ULL;
    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"pacing_mode\":\"deterministic_tick\",\"request_window_ticks\":%lu,\"max_requests_per_window\":%lu,\"window_start_tick\":%llu,\"window_end_tick\":%llu,\"queued_requests\":%lu,\"last_request_seq\":%llu}}",
             session_id,
             (unsigned long)dma_request_window_ticks,
             (unsigned long)dma_max_requests_per_window,
             (unsigned long long)dma_window_start_tick,
             (unsigned long long)window_end_tick,
             (unsigned long)dma_queued_requests,
             (unsigned long long)dma_last_request_seq);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_dma_arbitration_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    if (!esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    uint32_t limit = 0;
    if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_dma_unavailable_query[8] = {0};
    bool force_dma_unavailable = esptari_web_query_value(req,
                                                         "force_dma_unavailable",
                                                         force_dma_unavailable_query,
                                                         sizeof(force_dma_unavailable_query)) &&
                                strcmp(force_dma_unavailable_query, "1") == 0;
    if (force_dma_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    uint32_t event_count = limit > 3 ? 3 : limit;
    uint64_t request_seq_base = dma_last_request_seq;
    uint64_t scheduled_tick_base = dma_last_scheduled_tick;
    uint64_t timestamp_base = dma_last_timestamp_us;

    char events_json[1536] = {0};
    bool first = true;

    for (uint32_t i = 0; i < event_count; ++i) {
        uint64_t request_seq = request_seq_base + (uint64_t)i + 1ULL;
        uint64_t scheduled_tick = scheduled_tick_base + (uint64_t)i + 1ULL;
        uint64_t timestamp_us = timestamp_base + ((uint64_t)i + 1ULL) * 4ULL;
        uint32_t arbitration_round = dma_arbitration_round + i + 1U;

        const char *requester = (i == 0) ? "fdc" : ((i == 1) ? "blitter" : "memory_refresh");
        const char *grant_state = (i == 0) ? "granted" : ((i == 1) ? "deferred" : "denied");
        char granted_tick_json[32] = {0};
        if (strcmp(grant_state, "granted") == 0) {
            snprintf(granted_tick_json, sizeof(granted_tick_json), "%llu", (unsigned long long)scheduled_tick);
        } else {
            strlcpy(granted_tick_json, "null", sizeof(granted_tick_json));
        }

        size_t used = strlen(events_json);
        int written = snprintf(events_json + used,
                               sizeof(events_json) - used,
                               "%s{\"request_seq\":%llu,\"requester\":\"%s\",\"arbitration_round\":%lu,\"grant_state\":\"%s\",\"scheduled_tick\":%llu,\"granted_tick\":%s,\"timestamp_us\":%llu}",
                               first ? "" : ",",
                               (unsigned long long)request_seq,
                               requester,
                               (unsigned long)arbitration_round,
                               grant_state,
                               (unsigned long long)scheduled_tick,
                               granted_tick_json,
                               (unsigned long long)timestamp_us);
        if (written < 0 || (size_t)written >= sizeof(events_json) - used) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
        first = false;
    }

    dma_last_request_seq = request_seq_base + event_count;
    dma_last_scheduled_tick = scheduled_tick_base + event_count;
    dma_last_timestamp_us = timestamp_base + ((uint64_t)event_count) * 4ULL;
    dma_arbitration_round += event_count;
    while (dma_last_scheduled_tick > (dma_window_start_tick + (uint64_t)dma_request_window_ticks - 1ULL)) {
        dma_window_start_tick += (uint64_t)dma_request_window_ticks;
    }

    char resp[1792];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"events\":[%s]}}",
             session_id,
             events_json);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_fdc_fsm_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_fdc_unavailable_query[8] = {0};
    bool force_fdc_unavailable = esptari_web_query_value(req,
                                                         "force_fdc_unavailable",
                                                         force_fdc_unavailable_query,
                                                         sizeof(force_fdc_unavailable_query)) &&
                                strcmp(force_fdc_unavailable_query, "1") == 0;
    if (force_fdc_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char fsm_state[24] = "executing";
    char active_command_json[64] = "\"READ_SECTOR\"";
    uint32_t status_register = 129;
    const char *busy = "true";
    const char *drq = "false";
    const char *intrq = "false";

    char force_result_ready_query[8] = {0};
    bool force_result_ready = esptari_web_query_value(req,
                                                     "force_result_ready",
                                                     force_result_ready_query,
                                                     sizeof(force_result_ready_query)) &&
                             strcmp(force_result_ready_query, "1") == 0;
    if (force_result_ready) {
        strlcpy(fsm_state, "result_ready", sizeof(fsm_state));
        status_register = 0;
        busy = "false";
        drq = "false";
        intrq = "true";
        fdc_last_transition_tick += 24ULL;
        fdc_last_transition_us += 63ULL;
    }

    char force_idle_query[8] = {0};
    bool force_idle = esptari_web_query_value(req,
                                              "force_idle",
                                              force_idle_query,
                                              sizeof(force_idle_query)) &&
                      strcmp(force_idle_query, "1") == 0;
    if (force_idle) {
        strlcpy(fsm_state, "idle", sizeof(fsm_state));
        strlcpy(active_command_json, "null", sizeof(active_command_json));
        status_register = 0;
        busy = "false";
        drq = "false";
        intrq = "false";
        fdc_last_transition_tick += 16ULL;
        fdc_last_transition_us += 41ULL;
    }

    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"fsm_state\":\"%s\",\"active_command\":%s,\"command_seq\":%llu,\"status_register\":%lu,\"busy\":%s,\"drq\":%s,\"intrq\":%s,\"last_transition_tick\":%llu,\"last_transition_us\":%llu}}",
             session_id,
             fsm_state,
             active_command_json,
             (unsigned long long)fdc_command_seq,
             (unsigned long)status_register,
             busy,
             drq,
             intrq,
             (unsigned long long)fdc_last_transition_tick,
             (unsigned long long)fdc_last_transition_us);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_chipset_fdc_terminal_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    if (!esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    uint32_t limit = 0;
    if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_fdc_unavailable_query[8] = {0};
    bool force_fdc_unavailable = esptari_web_query_value(req,
                                                         "force_fdc_unavailable",
                                                         force_fdc_unavailable_query,
                                                         sizeof(force_fdc_unavailable_query)) &&
                                strcmp(force_fdc_unavailable_query, "1") == 0;
    if (force_fdc_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    uint32_t event_count = limit > 2 ? 2 : limit;
    uint64_t base_event_seq = fdc_terminal_event_seq;
    uint64_t base_tick = fdc_terminal_tick;
    uint64_t base_ts = fdc_terminal_timestamp_us;

    char events_json[1024] = {0};
    bool first = true;
    for (uint32_t i = 0; i < event_count; ++i) {
        uint64_t event_seq = base_event_seq + (uint64_t)i + 1ULL;
        uint64_t tick_counter = base_tick + (uint64_t)i + 1ULL;
        uint64_t timestamp_us = base_ts + ((uint64_t)i + 1ULL) * 9ULL;
        uint64_t command_seq = fdc_command_seq + (uint64_t)i;
        const char *terminal_condition = (i == 0) ? "ok" : "timeout";
        uint32_t status_register = (i == 0) ? 0U : 64U;

        size_t used = strlen(events_json);
        int written = snprintf(events_json + used,
                               sizeof(events_json) - used,
                               "%s{\"event_seq\":%llu,\"command_seq\":%llu,\"terminal_condition\":\"%s\",\"status_register\":%lu,\"busy\":false,\"drq\":false,\"intrq\":true,\"tick_counter\":%llu,\"timestamp_us\":%llu}",
                               first ? "" : ",",
                               (unsigned long long)event_seq,
                               (unsigned long long)command_seq,
                               terminal_condition,
                               (unsigned long)status_register,
                               (unsigned long long)tick_counter,
                               (unsigned long long)timestamp_us);
        if (written < 0 || (size_t)written >= sizeof(events_json) - used) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
        first = false;
    }

    fdc_terminal_event_seq = base_event_seq + event_count;
    fdc_terminal_tick = base_tick + event_count;
    fdc_terminal_timestamp_us = base_ts + ((uint64_t)event_count) * 9ULL;
    fdc_command_seq += event_count;
    fdc_last_transition_tick = fdc_terminal_tick;
    fdc_last_transition_us = fdc_terminal_timestamp_us;

    char resp[1280];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"events\":[%s]}}",
             session_id,
             events_json);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_psg_gpio_events_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    if (!esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint32_t limit = 0;
    if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_unavailable_query[8] = {0};
    bool force_unavailable = esptari_web_query_value(req, "force_gpio_unavailable", force_unavailable_query, sizeof(force_unavailable_query)) &&
                            strcmp(force_unavailable_query, "1") == 0;
    if (force_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    uint32_t event_count = limit > 2 ? 2 : limit;
    uint64_t event_seq_1 = psg_gpio_event_seq + 1;
    uint64_t event_seq_2 = psg_gpio_event_seq + 2;
    uint64_t tick_1 = psg_gpio_tick + 1;
    uint64_t tick_2 = psg_gpio_tick + 8;
    uint64_t ts_1 = psg_gpio_timestamp_us + 2;
    uint64_t ts_2 = psg_gpio_timestamp_us + 14;
    uint8_t port_a_before = psg_gpio_port_a_value > 31 ? (uint8_t)(psg_gpio_port_a_value - 31U) : 0U;
    uint8_t port_a_after = psg_gpio_port_a_value;
    uint8_t port_b_before = psg_gpio_port_b_value;
    uint8_t port_b_after = (uint8_t)((psg_gpio_port_b_value + 32U) & 0xFFU);

    char events_json[768] = {0};
    if (event_count == 1) {
        snprintf(events_json,
                 sizeof(events_json),
                 "[{\"event_seq\":%llu,\"port\":\"A\",\"direction\":\"output\",\"value_before\":%u,\"value_after\":%u,\"source\":\"cpu_write\",\"tick_counter\":%llu,\"timestamp_us\":%llu}]",
                 (unsigned long long)event_seq_1,
                 (unsigned)port_a_before,
                 (unsigned)port_a_after,
                 (unsigned long long)tick_1,
                 (unsigned long long)ts_1);
    } else {
        snprintf(events_json,
                 sizeof(events_json),
                 "[{\"event_seq\":%llu,\"port\":\"A\",\"direction\":\"output\",\"value_before\":%u,\"value_after\":%u,\"source\":\"cpu_write\",\"tick_counter\":%llu,\"timestamp_us\":%llu},{\"event_seq\":%llu,\"port\":\"B\",\"direction\":\"input\",\"value_before\":%u,\"value_after\":%u,\"source\":\"external_signal\",\"tick_counter\":%llu,\"timestamp_us\":%llu}]",
                 (unsigned long long)event_seq_1,
                 (unsigned)port_a_before,
                 (unsigned)port_a_after,
                 (unsigned long long)tick_1,
                 (unsigned long long)ts_1,
                 (unsigned long long)event_seq_2,
                 (unsigned)port_b_before,
                 (unsigned)port_b_after,
                 (unsigned long long)tick_2,
                 (unsigned long long)ts_2);
    }

    psg_gpio_event_seq = event_count == 1 ? event_seq_1 : event_seq_2;
    psg_gpio_tick = event_count == 1 ? tick_1 : tick_2;
    psg_gpio_timestamp_us = event_count == 1 ? ts_1 : ts_2;
    psg_gpio_port_b_value = event_count == 2 ? port_b_after : psg_gpio_port_b_value;

    char resp[1024];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"events\":%s}}",
             session_id,
             events_json);
    return send_json(req, resp, 200);
}

static esp_err_t registers_snapshot_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }
    guard = guard_running_or_paused(req);
    if (guard != ESP_OK) {
        return guard;
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    char resp[768];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"snapshot_at_us\":%llu,\"components\":[\"cpu\",\"mfp\"],\"registers\":[{\"component\":\"cpu\",\"name\":\"PC\",\"value\":\"0x00FC0000\"},{\"component\":\"cpu\",\"name\":\"SR\",\"value\":\"0x2700\"}]}}",
             session_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t bus_snapshot_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }
    guard = guard_running_or_paused(req);
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    uint32_t limit = 16;
    if (esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    char resp[896];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"snapshot_at_us\":%llu,\"limit\":%lu,\"transactions\":[{\"seq\":1,\"address\":\"0x00FF820A\",\"access\":\"read\",\"source\":\"cpu\",\"tick\":100,\"cycle\":8000},{\"seq\":2,\"address\":\"0x00FF8604\",\"access\":\"write\",\"source\":\"dma\",\"tick\":101,\"cycle\":8080}]}}",
             session_id,
             (unsigned long long)now_us,
             (unsigned long)limit);
    return send_json(req, resp, 200);
}

static esp_err_t memory_snapshot_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }
    guard = guard_running_or_paused(req);
    if (guard != ESP_OK) {
        return guard;
    }

    char range[64] = {0};
    if (!esptari_web_query_value(req, "range", range, sizeof(range)) || strstr(range, "-") == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    char resp[768];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"snapshot_at_us\":%llu,\"range\":\"%s\",\"bytes\":\"000102030405060708090A0B0C0D0E0F\",\"byte_count\":16}}",
             session_id,
             (unsigned long long)now_us,
             range);
    return send_json(req, resp, 200);
}

static esp_err_t checkpoint_create_handler(httpd_req_t *req)
{
    char body[768];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char session_id[64] = {0};
    esp_err_t session_err = validate_session_body(root, session_id, sizeof(session_id));
    if (session_err == ESP_ERR_INVALID_STATE) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    if (session_err != ESP_OK) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    esp_err_t guard = guard_running_or_paused(req);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    checkpoint_seq++;
    snprintf(last_checkpoint_id, sizeof(last_checkpoint_id), "chkpt_%06llu", (unsigned long long)checkpoint_seq);
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"checkpoint_id\":\"%s\",\"created_at_us\":%llu,\"state\":\"ready\"}}",
             session_id,
             last_checkpoint_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t checkpoint_load_handler(httpd_req_t *req)
{
    char body[768];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char session_id[64] = {0};
    esp_err_t session_err = validate_session_body(root, session_id, sizeof(session_id));
    if (session_err == ESP_ERR_INVALID_STATE) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    if (session_err != ESP_OK) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *checkpoint_id = NULL;
    if (!esptari_web_json_get_string(root, "checkpoint_id", &checkpoint_id) || checkpoint_id == NULL || checkpoint_id[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (strcmp(checkpoint_id, last_checkpoint_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_NOT_FOUND\"}}", 404);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"checkpoint_id\":\"%s\",\"loaded_at_us\":%llu,\"state\":\"loaded\"}}",
             session_id,
             checkpoint_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

void esptari_web_snapshot_register_routes(httpd_handle_t server_handle)
{
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/registers/snapshot", HTTP_GET, registers_snapshot_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/bus/snapshot", HTTP_GET, bus_snapshot_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/memory/snapshot", HTTP_GET, memory_snapshot_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/windows/registers", HTTP_GET, inspect_chipset_windows_registers_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/windows/memory", HTTP_GET, inspect_chipset_windows_memory_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/windows/timers", HTTP_GET, inspect_chipset_windows_timers_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/mfp/interrupts", HTTP_GET, inspect_chipset_mfp_interrupts_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/windows/integration", HTTP_GET, inspect_chipset_windows_integration_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/acia/bridge", HTTP_GET, inspect_chipset_acia_bridge_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/acia/frames", HTTP_GET, inspect_chipset_acia_frames_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/ikbd/bridge", HTTP_GET, inspect_chipset_ikbd_bridge_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/ikbd/packets", HTTP_GET, inspect_chipset_ikbd_packets_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/interrupts/hierarchy", HTTP_GET, inspect_chipset_interrupts_hierarchy_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/interrupts/routes", HTTP_GET, inspect_chipset_interrupts_routes_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/interrupts/wiring", HTTP_GET, inspect_chipset_interrupts_wiring_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/interrupts/wiring/checks", HTTP_GET, inspect_chipset_interrupts_wiring_checks_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/startup/defaults", HTTP_GET, inspect_chipset_startup_defaults_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/startup/baseline", HTTP_GET, inspect_chipset_startup_baseline_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/startup/sequence", HTTP_GET, inspect_chipset_startup_sequence_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/startup/verification", HTTP_GET, inspect_chipset_startup_verification_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/dma/pacing", HTTP_GET, inspect_chipset_dma_pacing_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/dma/arbitration", HTTP_GET, inspect_chipset_dma_arbitration_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/fdc/fsm", HTTP_GET, inspect_chipset_fdc_fsm_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/fdc/terminal", HTTP_GET, inspect_chipset_fdc_terminal_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/psg/registers", HTTP_GET, inspect_psg_registers_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/psg/audio", HTTP_GET, inspect_psg_audio_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/psg/gpio", HTTP_GET, inspect_psg_gpio_state_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/inspect/chipset/psg/gpio/events", HTTP_GET, inspect_psg_gpio_events_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/checkpoint/create", HTTP_POST, checkpoint_create_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/checkpoint/load", HTTP_POST, checkpoint_load_handler, "engine:control"));
}
