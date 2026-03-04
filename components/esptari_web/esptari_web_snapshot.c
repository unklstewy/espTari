#include "esptari_web_snapshot.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
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
    httpd_uri_t registers_snapshot = {.uri = "/api/v2/inspect/registers/snapshot", .method = HTTP_GET, .handler = registers_snapshot_handler, .user_ctx = NULL};
    httpd_uri_t bus_snapshot = {.uri = "/api/v2/inspect/bus/snapshot", .method = HTTP_GET, .handler = bus_snapshot_handler, .user_ctx = NULL};
    httpd_uri_t memory_snapshot = {.uri = "/api/v2/inspect/memory/snapshot", .method = HTTP_GET, .handler = memory_snapshot_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_windows_registers = {.uri = "/api/v2/inspect/chipset/windows/registers", .method = HTTP_GET, .handler = inspect_chipset_windows_registers_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_windows_memory = {.uri = "/api/v2/inspect/chipset/windows/memory", .method = HTTP_GET, .handler = inspect_chipset_windows_memory_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_windows_timers = {.uri = "/api/v2/inspect/chipset/windows/timers", .method = HTTP_GET, .handler = inspect_chipset_windows_timers_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_mfp_interrupts = {.uri = "/api/v2/inspect/chipset/mfp/interrupts", .method = HTTP_GET, .handler = inspect_chipset_mfp_interrupts_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_windows_integration = {.uri = "/api/v2/inspect/chipset/windows/integration", .method = HTTP_GET, .handler = inspect_chipset_windows_integration_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_acia_bridge = {.uri = "/api/v2/inspect/chipset/acia/bridge", .method = HTTP_GET, .handler = inspect_chipset_acia_bridge_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_acia_frames = {.uri = "/api/v2/inspect/chipset/acia/frames", .method = HTTP_GET, .handler = inspect_chipset_acia_frames_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_dma_pacing = {.uri = "/api/v2/inspect/chipset/dma/pacing", .method = HTTP_GET, .handler = inspect_chipset_dma_pacing_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_dma_arbitration = {.uri = "/api/v2/inspect/chipset/dma/arbitration", .method = HTTP_GET, .handler = inspect_chipset_dma_arbitration_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_fdc_fsm = {.uri = "/api/v2/inspect/chipset/fdc/fsm", .method = HTTP_GET, .handler = inspect_chipset_fdc_fsm_handler, .user_ctx = NULL};
    httpd_uri_t inspect_chipset_fdc_terminal = {.uri = "/api/v2/inspect/chipset/fdc/terminal", .method = HTTP_GET, .handler = inspect_chipset_fdc_terminal_handler, .user_ctx = NULL};
    httpd_uri_t inspect_psg_gpio_state = {.uri = "/api/v2/inspect/chipset/psg/gpio", .method = HTTP_GET, .handler = inspect_psg_gpio_state_handler, .user_ctx = NULL};
    httpd_uri_t inspect_psg_gpio_events = {.uri = "/api/v2/inspect/chipset/psg/gpio/events", .method = HTTP_GET, .handler = inspect_psg_gpio_events_handler, .user_ctx = NULL};
    httpd_uri_t checkpoint_create = {.uri = "/api/v2/engine/checkpoint/create", .method = HTTP_POST, .handler = checkpoint_create_handler, .user_ctx = NULL};
    httpd_uri_t checkpoint_load = {.uri = "/api/v2/engine/checkpoint/load", .method = HTTP_POST, .handler = checkpoint_load_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &registers_snapshot);
    httpd_register_uri_handler(server_handle, &bus_snapshot);
    httpd_register_uri_handler(server_handle, &memory_snapshot);
    httpd_register_uri_handler(server_handle, &inspect_chipset_windows_registers);
    httpd_register_uri_handler(server_handle, &inspect_chipset_windows_memory);
    httpd_register_uri_handler(server_handle, &inspect_chipset_windows_timers);
    httpd_register_uri_handler(server_handle, &inspect_chipset_mfp_interrupts);
    httpd_register_uri_handler(server_handle, &inspect_chipset_windows_integration);
    httpd_register_uri_handler(server_handle, &inspect_chipset_acia_bridge);
    httpd_register_uri_handler(server_handle, &inspect_chipset_acia_frames);
    httpd_register_uri_handler(server_handle, &inspect_chipset_dma_pacing);
    httpd_register_uri_handler(server_handle, &inspect_chipset_dma_arbitration);
    httpd_register_uri_handler(server_handle, &inspect_chipset_fdc_fsm);
    httpd_register_uri_handler(server_handle, &inspect_chipset_fdc_terminal);
    httpd_register_uri_handler(server_handle, &inspect_psg_gpio_state);
    httpd_register_uri_handler(server_handle, &inspect_psg_gpio_events);
    httpd_register_uri_handler(server_handle, &checkpoint_create);
    httpd_register_uri_handler(server_handle, &checkpoint_load);
}
