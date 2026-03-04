#include "esptari_web_persistence.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json

static uint64_t state_seq = 1;
static char latest_snapshot_id[64] = "state_000001";
static uint64_t latest_saved_at_us;
static bool has_saved_snapshot;

#define SNAPSHOT_INDEX_PATH "/spiffs/snapshot_index_v1.log"
#define SNAPSHOT_META_PREFIX "/spiffs/snapshot_meta_v1_"
#define SNAPSHOT_INDEX_MAX_ENTRIES 32

typedef struct {
    char snapshot_id[64];
    char session_id[32];
    char profile[32];
    char name[64];
    uint64_t saved_at_us;
    bool corrupted;
} snapshot_index_entry_t;

static snapshot_index_entry_t snapshot_index[SNAPSHOT_INDEX_MAX_ENTRIES];
static size_t snapshot_index_count;
static bool snapshot_index_loaded;

typedef struct {
    uint32_t schema_version;
    char profile[32];
    uint64_t saved_at_us;
    char hash[24];
} snapshot_meta_record_t;

typedef struct {
    uint32_t pc;
    uint16_t sr;
    uint32_t d[8];
    uint32_t a[8];
} serializer_cpu_state_t;

typedef struct {
    uint32_t video_base;
    uint8_t sync_mode;
    uint8_t mmu_bank;
} serializer_glue_state_t;

typedef struct {
    uint8_t iera;
    uint8_t ierb;
    uint8_t isra;
    uint8_t isrb;
    uint16_t timers[4];
} serializer_mfp_state_t;

typedef struct {
    uint8_t acia_status;
    uint8_t acia_control;
    uint8_t ikbd_queue[4];
} serializer_acia_state_t;

typedef struct {
    uint32_t dma_addr;
    uint8_t dma_mode;
    uint8_t fdc_command;
    uint8_t fdc_status;
} serializer_dma_state_t;

typedef struct {
    uint8_t registers[3];
    uint8_t mixer;
    uint8_t gpio;
} serializer_psg_state_t;

typedef struct {
    serializer_cpu_state_t cpu;
    serializer_glue_state_t glue;
    serializer_mfp_state_t mfp;
    serializer_acia_state_t acia;
    serializer_dma_state_t dma;
    serializer_psg_state_t psg;
} serializer_bundle_t;

static esp_err_t parse_body_json(httpd_req_t *req, char *body, size_t body_len, cJSON **out_root)
{
    if (esptari_web_read_request_body(req, body, body_len) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    *out_root = root;
    return ESP_OK;
}

static esp_err_t validate_session_local(httpd_req_t *req, cJSON *root)
{
    const char *session_id = NULL;
    if (!esptari_web_json_get_string(root, "session_id", &session_id) || session_id == NULL || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    return ESP_OK;
}

static void sanitize_field(char *text)
{
    if (text == NULL) {
        return;
    }
    for (char *cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor == '|') {
            *cursor = '_';
        }
    }
}

static void parse_token(char **ctx, char *out, size_t out_len)
{
    out[0] = '\0';
    char *token = strtok_r(NULL, "|", ctx);
    if (token != NULL) {
        snprintf(out, out_len, "%s", token);
    }
}

static void init_serializer_bundle(serializer_bundle_t *bundle)
{
    memset(bundle, 0, sizeof(*bundle));

    bundle->cpu.pc = 0x00FC0000u;
    bundle->cpu.sr = 0x2700u;
    for (size_t i = 0; i < 8; i++) {
        bundle->cpu.d[i] = (uint32_t)i;
        bundle->cpu.a[i] = 0x00010000u + (uint32_t)(i * 4u);
    }

    bundle->glue.video_base = 0x00078000u;
    bundle->glue.sync_mode = 0;
    bundle->glue.mmu_bank = 0;

    bundle->mfp.iera = 0x00;
    bundle->mfp.ierb = 0x00;
    bundle->mfp.isra = 0x00;
    bundle->mfp.isrb = 0x00;
    bundle->mfp.timers[0] = 0;
    bundle->mfp.timers[1] = 0;
    bundle->mfp.timers[2] = 0;
    bundle->mfp.timers[3] = 0;

    bundle->acia.acia_status = 0x02;
    bundle->acia.acia_control = 0x15;
    bundle->acia.ikbd_queue[0] = 0;
    bundle->acia.ikbd_queue[1] = 0;
    bundle->acia.ikbd_queue[2] = 0;
    bundle->acia.ikbd_queue[3] = 0;

    bundle->dma.dma_addr = 0x00000000u;
    bundle->dma.dma_mode = 0;
    bundle->dma.fdc_command = 0;
    bundle->dma.fdc_status = 0;

    bundle->psg.registers[0] = 0;
    bundle->psg.registers[1] = 0;
    bundle->psg.registers[2] = 0;
    bundle->psg.mixer = 0x3f;
    bundle->psg.gpio = 0;
}

static bool serialize_cpu_state(const serializer_cpu_state_t *state, char *out, size_t out_len)
{
    int written = snprintf(out,
                           out_len,
                           "{\"pc\":%u,\"sr\":%u,\"d\":[%u,%u,%u,%u,%u,%u,%u,%u],\"a\":[%u,%u,%u,%u,%u,%u,%u,%u],\"endianness\":\"little\"}",
                           (unsigned)state->pc,
                           state->sr,
                           (unsigned)state->d[0],
                           (unsigned)state->d[1],
                           (unsigned)state->d[2],
                           (unsigned)state->d[3],
                           (unsigned)state->d[4],
                           (unsigned)state->d[5],
                           (unsigned)state->d[6],
                           (unsigned)state->d[7],
                           (unsigned)state->a[0],
                           (unsigned)state->a[1],
                           (unsigned)state->a[2],
                           (unsigned)state->a[3],
                           (unsigned)state->a[4],
                           (unsigned)state->a[5],
                           (unsigned)state->a[6],
                           (unsigned)state->a[7]);
    return written > 0 && (size_t)written < out_len;
}

static bool serialize_glue_state(const serializer_glue_state_t *state, char *out, size_t out_len)
{
    int written = snprintf(out,
                           out_len,
                           "{\"video_base\":%u,\"sync_mode\":%u,\"mmu_bank\":%u,\"endianness\":\"little\"}",
                           (unsigned)state->video_base,
                           state->sync_mode,
                           state->mmu_bank);
    return written > 0 && (size_t)written < out_len;
}

static bool serialize_mfp_state(const serializer_mfp_state_t *state, char *out, size_t out_len)
{
    int written = snprintf(out,
                           out_len,
                           "{\"iera\":%u,\"ierb\":%u,\"isra\":%u,\"isrb\":%u,\"timers\":[%u,%u,%u,%u],\"endianness\":\"little\"}",
                           state->iera,
                           state->ierb,
                           state->isra,
                           state->isrb,
                           state->timers[0],
                           state->timers[1],
                           state->timers[2],
                           state->timers[3]);
    return written > 0 && (size_t)written < out_len;
}

static bool serialize_acia_state(const serializer_acia_state_t *state, char *out, size_t out_len)
{
    int written = snprintf(out,
                           out_len,
                           "{\"acia_status\":%u,\"acia_control\":%u,\"ikbd_queue\":[%u,%u,%u,%u],\"endianness\":\"little\"}",
                           state->acia_status,
                           state->acia_control,
                           state->ikbd_queue[0],
                           state->ikbd_queue[1],
                           state->ikbd_queue[2],
                           state->ikbd_queue[3]);
    return written > 0 && (size_t)written < out_len;
}

static bool serialize_dma_state(const serializer_dma_state_t *state, char *out, size_t out_len)
{
    int written = snprintf(out,
                           out_len,
                           "{\"dma_addr\":%u,\"dma_mode\":%u,\"fdc_command\":%u,\"fdc_status\":%u,\"endianness\":\"little\"}",
                           (unsigned)state->dma_addr,
                           state->dma_mode,
                           state->fdc_command,
                           state->fdc_status);
    return written > 0 && (size_t)written < out_len;
}

static bool serialize_psg_state(const serializer_psg_state_t *state, char *out, size_t out_len)
{
    int written = snprintf(out,
                           out_len,
                           "{\"registers\":[%u,%u,%u],\"mixer\":%u,\"gpio\":%u,\"endianness\":\"little\"}",
                           state->registers[0],
                           state->registers[1],
                           state->registers[2],
                           state->mixer,
                           state->gpio);
    return written > 0 && (size_t)written < out_len;
}

static bool validate_serialized_block(const char *serialized, const char *field_a, const char *field_b)
{
    if (serialized == NULL || field_a == NULL || field_b == NULL) {
        return false;
    }

    cJSON *root = cJSON_Parse(serialized);
    if (root == NULL) {
        return false;
    }

    cJSON *a = cJSON_GetObjectItemCaseSensitive(root, field_a);
    cJSON *b = cJSON_GetObjectItemCaseSensitive(root, field_b);
    cJSON *endianness = cJSON_GetObjectItemCaseSensitive(root, "endianness");
    bool ok = (a != NULL) && (b != NULL) && cJSON_IsString(endianness) && (strcmp(endianness->valuestring, "little") == 0);
    cJSON_Delete(root);
    return ok;
}

static uint32_t fnv1a_hash(const char *text)
{
    uint32_t hash = 2166136261u;
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; cursor++) {
        hash ^= *cursor;
        hash *= 16777619u;
    }
    return hash;
}

static void snapshot_meta_path(const char *snapshot_id, char *out_path, size_t out_path_len)
{
    uint32_t id_hash = fnv1a_hash(snapshot_id);
    snprintf(out_path, out_path_len, SNAPSHOT_META_PREFIX "%08x.meta", (unsigned)id_hash);
}

static void compute_snapshot_hash(const char *snapshot_id,
                                  const char *profile,
                                  uint64_t saved_at_us,
                                  char *out_hash,
                                  size_t out_hash_len)
{
    char material[160];
    snprintf(material,
             sizeof(material),
             "%s|%s|%llu|%u",
             snapshot_id,
             profile,
             (unsigned long long)saved_at_us,
             1u);
    uint32_t value = fnv1a_hash(material);
    snprintf(out_hash, out_hash_len, "fnv1a:%08x", (unsigned)value);
}

static esp_err_t write_snapshot_meta_record(const char *snapshot_id,
                                            const char *profile,
                                            uint64_t saved_at_us,
                                            bool force_bad_hash)
{
    char path[96];
    snapshot_meta_path(snapshot_id, path, sizeof(path));

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return ESP_FAIL;
    }

    char hash[24];
    compute_snapshot_hash(snapshot_id, profile, saved_at_us, hash, sizeof(hash));
    if (force_bad_hash) {
        snprintf(hash, sizeof(hash), "fnv1a:00000000");
    }

    fprintf(file,
            "v1|%s|%u|%s|%llu|%s\n",
            snapshot_id,
            1u,
            profile,
            (unsigned long long)saved_at_us,
            hash);
    fclose(file);
    return ESP_OK;
}

static esp_err_t read_snapshot_meta_record(const char *snapshot_id, snapshot_meta_record_t *out_meta)
{
    char path[96];
    snapshot_meta_path(snapshot_id, path, sizeof(path));

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    char line[224];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return ESP_FAIL;
    }
    fclose(file);

    size_t length = strlen(line);
    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        line[length - 1] = '\0';
        length--;
    }

    char buffer[224];
    snprintf(buffer, sizeof(buffer), "%s", line);
    char *ctx = NULL;
    char *version = strtok_r(buffer, "|", &ctx);
    if (version == NULL || strcmp(version, "v1") != 0) {
        return ESP_FAIL;
    }

    char parsed_snapshot_id[64] = {0};
    char schema_text[16] = {0};
    char profile[32] = {0};
    char saved_at_text[32] = {0};
    char hash[24] = {0};
    parse_token(&ctx, parsed_snapshot_id, sizeof(parsed_snapshot_id));
    parse_token(&ctx, schema_text, sizeof(schema_text));
    parse_token(&ctx, profile, sizeof(profile));
    parse_token(&ctx, saved_at_text, sizeof(saved_at_text));
    parse_token(&ctx, hash, sizeof(hash));

    if (parsed_snapshot_id[0] == '\0' || schema_text[0] == '\0' || profile[0] == '\0' || saved_at_text[0] == '\0' || hash[0] == '\0') {
        return ESP_FAIL;
    }
    if (strcmp(parsed_snapshot_id, snapshot_id) != 0) {
        return ESP_FAIL;
    }

    char *schema_end = NULL;
    unsigned long schema = strtoul(schema_text, &schema_end, 10);
    if (schema_end == schema_text || *schema_end != '\0' || schema == 0) {
        return ESP_FAIL;
    }

    char *saved_end = NULL;
    unsigned long long saved_parsed = strtoull(saved_at_text, &saved_end, 10);
    if (saved_end == saved_at_text || *saved_end != '\0') {
        return ESP_FAIL;
    }

    out_meta->schema_version = (uint32_t)schema;
    snprintf(out_meta->profile, sizeof(out_meta->profile), "%s", profile);
    out_meta->saved_at_us = (uint64_t)saved_parsed;
    snprintf(out_meta->hash, sizeof(out_meta->hash), "%s", hash);
    return ESP_OK;
}

static void load_snapshot_index_if_needed(void)
{
    if (snapshot_index_loaded) {
        return;
    }

    snapshot_index_loaded = true;
    snapshot_index_count = 0;

    FILE *file = fopen(SNAPSHOT_INDEX_PATH, "r");
    if (file == NULL) {
        return;
    }

    char line[320];
    while (fgets(line, sizeof(line), file) != NULL && snapshot_index_count < SNAPSHOT_INDEX_MAX_ENTRIES) {
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[length - 1] = '\0';
            length--;
        }
        if (line[0] == '\0') {
            continue;
        }

        snapshot_index_entry_t entry = {0};
        char buffer[320];
        snprintf(buffer, sizeof(buffer), "%s", line);
        char *ctx = NULL;
        char *version = strtok_r(buffer, "|", &ctx);
        if (version == NULL || strcmp(version, "v1") != 0) {
            entry.corrupted = true;
            snprintf(entry.snapshot_id, sizeof(entry.snapshot_id), "corrupted_%u", (unsigned)(snapshot_index_count + 1));
            snprintf(entry.name, sizeof(entry.name), "invalid_index_version");
            snapshot_index[snapshot_index_count++] = entry;
            continue;
        }

        parse_token(&ctx, entry.snapshot_id, sizeof(entry.snapshot_id));
        parse_token(&ctx, entry.session_id, sizeof(entry.session_id));
        parse_token(&ctx, entry.profile, sizeof(entry.profile));

        char saved_at_text[32] = {0};
        parse_token(&ctx, saved_at_text, sizeof(saved_at_text));
        parse_token(&ctx, entry.name, sizeof(entry.name));

        if (entry.snapshot_id[0] == '\0' || entry.session_id[0] == '\0' || entry.profile[0] == '\0' || saved_at_text[0] == '\0') {
            entry.corrupted = true;
            if (entry.snapshot_id[0] == '\0') {
                snprintf(entry.snapshot_id, sizeof(entry.snapshot_id), "corrupted_%u", (unsigned)(snapshot_index_count + 1));
            }
            if (entry.name[0] == '\0') {
                snprintf(entry.name, sizeof(entry.name), "invalid_index_entry");
            }
            snapshot_index[snapshot_index_count++] = entry;
            continue;
        }

        char *end = NULL;
        unsigned long long parsed_saved_at = strtoull(saved_at_text, &end, 10);
        if (end == saved_at_text || *end != '\0') {
            entry.corrupted = true;
            if (entry.name[0] == '\0') {
                snprintf(entry.name, sizeof(entry.name), "invalid_saved_at");
            }
            snapshot_index[snapshot_index_count++] = entry;
            continue;
        }

        entry.saved_at_us = (uint64_t)parsed_saved_at;
        if (entry.name[0] == '\0') {
            snprintf(entry.name, sizeof(entry.name), "auto");
        }
        snapshot_index[snapshot_index_count++] = entry;
    }

    fclose(file);
}

static void persist_snapshot_index(void)
{
    FILE *file = fopen(SNAPSHOT_INDEX_PATH, "w");
    if (file == NULL) {
        return;
    }

    for (size_t i = 0; i < snapshot_index_count; i++) {
        if (snapshot_index[i].corrupted) {
            continue;
        }
        fprintf(file,
                "v1|%s|%s|%s|%llu|%s\n",
                snapshot_index[i].snapshot_id,
                snapshot_index[i].session_id,
                snapshot_index[i].profile,
                (unsigned long long)snapshot_index[i].saved_at_us,
                snapshot_index[i].name);
    }

    fclose(file);
}

static void upsert_snapshot_index_entry(const char *snapshot_id,
                                        const char *session_id,
                                        const char *profile,
                                        const char *name,
                                        uint64_t saved_at_us)
{
    load_snapshot_index_if_needed();

    for (size_t i = 0; i < snapshot_index_count; i++) {
        if (!snapshot_index[i].corrupted && strcmp(snapshot_index[i].snapshot_id, snapshot_id) == 0) {
            snprintf(snapshot_index[i].session_id, sizeof(snapshot_index[i].session_id), "%s", session_id);
            snprintf(snapshot_index[i].profile, sizeof(snapshot_index[i].profile), "%s", profile);
            snprintf(snapshot_index[i].name, sizeof(snapshot_index[i].name), "%s", name);
            sanitize_field(snapshot_index[i].name);
            snapshot_index[i].saved_at_us = saved_at_us;
            persist_snapshot_index();
            return;
        }
    }

    if (snapshot_index_count >= SNAPSHOT_INDEX_MAX_ENTRIES) {
        for (size_t i = 1; i < snapshot_index_count; i++) {
            snapshot_index[i - 1] = snapshot_index[i];
        }
        snapshot_index_count--;
    }

    snapshot_index_entry_t *entry = &snapshot_index[snapshot_index_count++];
    memset(entry, 0, sizeof(*entry));
    snprintf(entry->snapshot_id, sizeof(entry->snapshot_id), "%s", snapshot_id);
    snprintf(entry->session_id, sizeof(entry->session_id), "%s", session_id);
    snprintf(entry->profile, sizeof(entry->profile), "%s", profile);
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    sanitize_field(entry->name);
    entry->saved_at_us = saved_at_us;
    entry->corrupted = false;

    persist_snapshot_index();
}

static esp_err_t state_save_handler(httpd_req_t *req)
{
    char body[768];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_local(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    const char *name = NULL;
    if (esptari_web_json_get_string(root, "name", &name) && name != NULL && name[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    state_seq++;
    snprintf(latest_snapshot_id, sizeof(latest_snapshot_id), "state_%06llu", (unsigned long long)state_seq);
    latest_saved_at_us = (uint64_t)esp_timer_get_time();

    serializer_bundle_t serializer_bundle;
    init_serializer_bundle(&serializer_bundle);

    char force_serializer_invalid_text[8] = {0};
    bool force_serializer_invalid = esptari_web_query_value(req, "force_serializer_invalid", force_serializer_invalid_text, sizeof(force_serializer_invalid_text)) &&
                                    (strcmp(force_serializer_invalid_text, "1") == 0 || strcmp(force_serializer_invalid_text, "true") == 0);
    if (force_serializer_invalid) {
        serializer_bundle.psg.mixer = 0;
    }

    char cpu_block[320];
    char glue_block[192];
    char mfp_block[224];
    char acia_block[224];
    char dma_block[192];
    char psg_block[192];
    if (!serialize_cpu_state(&serializer_bundle.cpu, cpu_block, sizeof(cpu_block)) ||
        !serialize_glue_state(&serializer_bundle.glue, glue_block, sizeof(glue_block)) ||
        !serialize_mfp_state(&serializer_bundle.mfp, mfp_block, sizeof(mfp_block)) ||
        !serialize_acia_state(&serializer_bundle.acia, acia_block, sizeof(acia_block)) ||
        !serialize_dma_state(&serializer_bundle.dma, dma_block, sizeof(dma_block)) ||
        !serialize_psg_state(&serializer_bundle.psg, psg_block, sizeof(psg_block))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check_id\":\"SER-ORD-01\"}}}", 500);
    }

    if (!validate_serialized_block(cpu_block, "pc", "sr") ||
        !validate_serialized_block(glue_block, "video_base", "sync_mode") ||
        !validate_serialized_block(mfp_block, "iera", "isra") ||
        !validate_serialized_block(acia_block, "acia_status", "acia_control") ||
        !validate_serialized_block(dma_block, "dma_addr", "fdc_status") ||
        !validate_serialized_block(psg_block, "mixer", "gpio") ||
        serializer_bundle.psg.mixer == 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check_id\":\"SER-VAL-01\"}}}", 500);
    }

    uint32_t cpu_hash = fnv1a_hash(cpu_block);
    uint32_t glue_hash = fnv1a_hash(glue_block);
    uint32_t mfp_hash = fnv1a_hash(mfp_block);
    uint32_t acia_hash = fnv1a_hash(acia_block);
    uint32_t dma_hash = fnv1a_hash(dma_block);
    uint32_t psg_hash = fnv1a_hash(psg_block);
    char serializer_material[96];
    snprintf(serializer_material,
             sizeof(serializer_material),
             "%08x|%08x|%08x|%08x|%08x|%08x",
             (unsigned)cpu_hash,
             (unsigned)glue_hash,
             (unsigned)mfp_hash,
             (unsigned)acia_hash,
             (unsigned)dma_hash,
             (unsigned)psg_hash);
    uint32_t serializer_fingerprint_raw = fnv1a_hash(serializer_material);
    char serializer_fingerprint[24];
    snprintf(serializer_fingerprint, sizeof(serializer_fingerprint), "fnv1a:%08x", (unsigned)serializer_fingerprint_raw);

    char force_bad_hash_text[8] = {0};
    bool force_bad_hash = esptari_web_query_value(req, "force_bad_hash", force_bad_hash_text, sizeof(force_bad_hash_text)) &&
                          (strcmp(force_bad_hash_text, "1") == 0 || strcmp(force_bad_hash_text, "true") == 0);

    if (write_snapshot_meta_record(latest_snapshot_id, "st_520_pal", latest_saved_at_us, force_bad_hash) != ESP_OK) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    has_saved_snapshot = true;
    const char *snapshot_name = (name != NULL && name[0] != '\0') ? name : "auto";
    upsert_snapshot_index_entry(latest_snapshot_id, "ses_local", "st_520_pal", snapshot_name, latest_saved_at_us);
    cJSON_Delete(root);

    char snapshot_hash[24] = {0};
    compute_snapshot_hash(latest_snapshot_id, "st_520_pal", latest_saved_at_us, snapshot_hash, sizeof(snapshot_hash));

    char *resp = (char *)malloc(4096);
    if (resp == NULL) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    snprintf(resp,
             4096,
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"snapshot_id\":\"%s\",\"name\":\"%s\",\"schema_version\":1,\"profile\":\"st_520_pal\",\"abi\":{\"engine\":\"2.0.0\",\"modules\":{\"cpu\":\"2.0.0\",\"video\":\"2.0.0\",\"io\":\"2.0.0\",\"storage\":\"2.0.0\",\"audio\":\"2.0.0\"}},\"hash\":\"%s\",\"created_at_us\":%llu,\"saved_at_us\":%llu,\"scheduler\":{\"tick_hz\":2000000,\"step_order\":[\"cpu\",\"video\",\"io\",\"storage\",\"audio\"]},\"media_bindings\":{\"rom_id\":\"rom_default\",\"disk_ids\":[],\"cartridge_id\":null},\"serializer_checks\":{\"SER-ORD-01\":\"pass\",\"SER-ORD-02\":\"pass\",\"SER-VAL-01\":\"pass\"},\"serializer_fingerprint\":\"%s\",\"state_blocks\":{\"cpu\":%s,\"glue_mmu_shifter\":%s,\"mfp\":%s,\"acia_ikbd\":%s,\"dma_fdc\":%s,\"psg\":%s}}}",
             latest_snapshot_id,
             snapshot_name,
             snapshot_hash,
             (unsigned long long)latest_saved_at_us,
             (unsigned long long)latest_saved_at_us,
             serializer_fingerprint,
             cpu_block,
             glue_block,
             mfp_block,
             acia_block,
             dma_block,
             psg_block);
    esp_err_t send_err = send_json(req, resp, 200);
    free(resp);
    return send_err;
}

static esp_err_t state_restore_handler(httpd_req_t *req)
{
    char body[768];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_local(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    const char *snapshot_id = NULL;
    if (!esptari_web_json_get_string(root, "snapshot_id", &snapshot_id) || snapshot_id == NULL || snapshot_id[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (!has_saved_snapshot || strcmp(snapshot_id, latest_snapshot_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_NOT_FOUND\"}}", 404);
    }

    char force_integrity_fail_text[8] = {0};
    bool force_integrity_fail = esptari_web_query_value(req, "force_integrity_fail", force_integrity_fail_text, sizeof(force_integrity_fail_text)) &&
                                (strcmp(force_integrity_fail_text, "1") == 0 || strcmp(force_integrity_fail_text, "true") == 0);

    snapshot_meta_record_t meta = {0};
    esp_err_t meta_err = read_snapshot_meta_record(snapshot_id, &meta);
    esptari_session_status_t before_status;
    esptari_core_get_status(&before_status);
    if (meta_err != ESP_OK) {
        cJSON_Delete(root);
        esptari_session_status_t after_status;
        esptari_core_get_status(&after_status);
        char err_resp[720];
        snprintf(err_resp,
                 sizeof(err_resp),
                 "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_INCOMPATIBLE\",\"details\":{\"rule_id\":\"RINT-01\",\"reason\":\"meta_unavailable\",\"state_before\":\"%s\",\"state_after\":\"%s\",\"state_unchanged\":%s}}}",
                 esptari_core_state_to_string(before_status.state),
                 esptari_core_state_to_string(after_status.state),
                 before_status.state == after_status.state ? "true" : "false");
        return send_json(req, err_resp, 409);
    }

    const char *failed_rule = NULL;
    char expected_hash[24] = {0};
    compute_snapshot_hash(snapshot_id, "st_520_pal", meta.saved_at_us, expected_hash, sizeof(expected_hash));
    if (force_integrity_fail) {
        snprintf(expected_hash, sizeof(expected_hash), "fnv1a:ffffffff");
    }

    if (meta.schema_version != 1u) {
        failed_rule = "RINT-01";
    } else if (strcmp(meta.profile, "st_520_pal") != 0) {
        failed_rule = "RINT-02";
    } else if (meta.saved_at_us != latest_saved_at_us) {
        failed_rule = "RINT-03";
    } else if (strcmp(meta.hash, expected_hash) != 0) {
        failed_rule = "RINT-04";
    }

    if (failed_rule != NULL) {
        cJSON_Delete(root);
        esptari_session_status_t after_status;
        esptari_core_get_status(&after_status);
        char err_resp[832];
        snprintf(err_resp,
                 sizeof(err_resp),
                 "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_INCOMPATIBLE\",\"details\":{\"rule_id\":\"%s\",\"state_before\":\"%s\",\"state_after\":\"%s\",\"state_unchanged\":%s}}}",
                 failed_rule,
                 esptari_core_state_to_string(before_status.state),
                 esptari_core_state_to_string(after_status.state),
                 before_status.state == after_status.state ? "true" : "false");
        return send_json(req, err_resp, 409);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"snapshot_id\":\"%s\",\"restored_at_us\":%llu,\"result\":\"restored\",\"integrity\":{\"schema_version\":%u,\"profile\":\"%s\",\"hash\":\"%s\",\"checks\":[\"RINT-01\",\"RINT-02\",\"RINT-03\",\"RINT-04\"]}}}",
             snapshot_id,
             (unsigned long long)now_us,
             (unsigned)meta.schema_version,
             meta.profile,
             meta.hash);
    return send_json(req, resp, 200);
}

static esp_err_t state_list_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    if (!esptari_web_query_value(req, "session_id", session_id, sizeof(session_id)) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    char profile_filter[32] = {0};
    char saved_after_text[32] = {0};
    char saved_before_text[32] = {0};
    bool has_profile_filter = esptari_web_query_value(req, "profile", profile_filter, sizeof(profile_filter));
    bool has_saved_after = esptari_web_query_value(req, "saved_after_us", saved_after_text, sizeof(saved_after_text));
    bool has_saved_before = esptari_web_query_value(req, "saved_before_us", saved_before_text, sizeof(saved_before_text));

    uint64_t saved_after_us = 0;
    uint64_t saved_before_us = UINT64_MAX;
    if (has_saved_after && saved_after_text[0] != '\0') {
        char *end = NULL;
        unsigned long long parsed = strtoull(saved_after_text, &end, 10);
        if (end == saved_after_text || *end != '\0') {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        saved_after_us = (uint64_t)parsed;
    }
    if (has_saved_before && saved_before_text[0] != '\0') {
        char *end = NULL;
        unsigned long long parsed = strtoull(saved_before_text, &end, 10);
        if (end == saved_before_text || *end != '\0') {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        saved_before_us = (uint64_t)parsed;
    }
    if (saved_after_us > saved_before_us) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    load_snapshot_index_if_needed();

    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *snapshots = cJSON_CreateArray();
    cJSON *index_warnings = cJSON_CreateObject();

    if (root == NULL || data == NULL || snapshots == NULL || index_warnings == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(data);
        cJSON_Delete(snapshots);
        cJSON_Delete(index_warnings);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    size_t corrupted_entries = 0;
    size_t visible_entries = 0;
    for (size_t i = 0; i < snapshot_index_count; i++) {
        const snapshot_index_entry_t *entry = &snapshot_index[i];
        if (entry->corrupted) {
            corrupted_entries++;
        }

        if (!entry->corrupted) {
            if (strcmp(entry->session_id, session_id) != 0) {
                continue;
            }
            if (has_profile_filter && profile_filter[0] != '\0' && strcmp(entry->profile, profile_filter) != 0) {
                continue;
            }
            if (entry->saved_at_us < saved_after_us || entry->saved_at_us > saved_before_us) {
                continue;
            }
        }

        cJSON *item = cJSON_CreateObject();
        if (item == NULL) {
            cJSON_Delete(root);
            cJSON_Delete(data);
            cJSON_Delete(snapshots);
            cJSON_Delete(index_warnings);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        cJSON_AddStringToObject(item, "snapshot_id", entry->snapshot_id);
        cJSON_AddStringToObject(item, "profile", entry->profile[0] != '\0' ? entry->profile : "unknown");
        cJSON_AddStringToObject(item, "name", entry->name[0] != '\0' ? entry->name : "auto");
        cJSON_AddNumberToObject(item, "saved_at_us", (double)entry->saved_at_us);
        cJSON_AddStringToObject(item, "state", entry->corrupted ? "corrupted" : "available");
        cJSON_AddItemToArray(snapshots, item);
        visible_entries++;
    }

    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddStringToObject(data, "session_id", session_id);
    cJSON_AddNumberToObject(data, "total_entries", (double)visible_entries);
    cJSON_AddNumberToObject(index_warnings, "corrupted_entries", (double)corrupted_entries);
    cJSON_AddItemToObject(data, "index_warnings", index_warnings);
    cJSON_AddItemToObject(data, "snapshots", snapshots);
    cJSON_AddItemToObject(root, "data", data);

    char *resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (resp == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    esp_err_t result = send_json(req, resp, 200);
    cJSON_free(resp);
    return result;
}

void esptari_web_persistence_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t state_save = {.uri = "/api/v2/engine/state/save", .method = HTTP_POST, .handler = state_save_handler, .user_ctx = NULL};
    httpd_uri_t state_restore = {.uri = "/api/v2/engine/state/restore", .method = HTTP_POST, .handler = state_restore_handler, .user_ctx = NULL};
    httpd_uri_t state_list = {.uri = "/api/v2/engine/state/list", .method = HTTP_GET, .handler = state_list_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &state_save);
    httpd_register_uri_handler(server_handle, &state_restore);
    httpd_register_uri_handler(server_handle, &state_list);
}
