#include "esptari_web_catalog_sync.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "esptari_web_http_utils.h"

static const char *TAG = "esptari_web_catalog";

typedef struct {
    char id[40];
    char scope[64];
    char status[24];
    uint64_t created_ms;
} esptari_sync_job_t;

typedef struct {
    char id[40];
    char cron[48];
    char scope[64];
    bool enabled;
} esptari_sync_schedule_t;

static esptari_sync_job_t s_jobs[] = {
    {.id = "sync-0001", .scope = "roms", .status = "completed", .created_ms = 0},
    {.id = "sync-0002", .scope = "disks", .status = "running", .created_ms = 0},
};

static esptari_sync_schedule_t s_schedules[] = {
    {.id = "sched-0001", .cron = "0 */6 * * *", .scope = "roms", .enabled = true},
};

static uint64_t current_time_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static esp_err_t send_json_object(httpd_req_t *req, cJSON *root, int status)
{
    char *json = cJSON_PrintUnformatted(root);
    if (json == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = esptari_web_send_json(req, json, status);
    cJSON_free(json);
    return out;
}

static esp_err_t send_error(httpd_req_t *req, const char *code, int status)
{
    char resp[192];
    snprintf(resp, sizeof(resp), "{\"ok\":false,\"error\":{\"code\":\"%s\"}}", code);
    return esptari_web_send_json(req, resp, status);
}

static bool parse_json_request(httpd_req_t *req, cJSON **json)
{
    char body[512];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        send_error(req, "BAD_REQUEST", 400);
        return false;
    }

    *json = cJSON_Parse(body);
    if (*json == NULL) {
        send_error(req, "BAD_REQUEST", 400);
        return false;
    }

    return true;
}

static const char *wildcard_tail(const char *uri, const char *prefix)
{
    if (strncmp(uri, prefix, strlen(prefix)) == 0) {
        return uri + strlen(prefix);
    }
    return NULL;
}

static esp_err_t handle_catalog_sync_run(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jobId", "sync-0003");
    cJSON_AddStringToObject(root, "status", "queued");
    cJSON_AddNumberToObject(root, "queuedAtMs", (double)current_time_ms());

    send_json_object(req, root, 202);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_catalog_sync_jobs(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *jobs = cJSON_AddArrayToObject(root, "jobs");

    for (size_t i = 0; i < sizeof(s_jobs) / sizeof(s_jobs[0]); ++i) {
        cJSON *job = cJSON_CreateObject();
        cJSON_AddStringToObject(job, "id", s_jobs[i].id);
        cJSON_AddStringToObject(job, "scope", s_jobs[i].scope);
        cJSON_AddStringToObject(job, "status", s_jobs[i].status);
        cJSON_AddNumberToObject(job, "createdAtMs", (double)s_jobs[i].created_ms);
        cJSON_AddItemToArray(jobs, job);
    }

    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_catalog_sync_job_by_id(httpd_req_t *req)
{
    const char *job_id = wildcard_tail(req->uri, "/api/v2/catalog-sync/jobs/");
    if (job_id == NULL || job_id[0] == '\0') {
        send_error(req, "BAD_REQUEST", 400);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", job_id);
    cJSON_AddStringToObject(root, "status", "running");
    cJSON_AddNumberToObject(root, "progress", 0.55);

    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_catalog_sync_create_schedule(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *cron = cJSON_GetObjectItemCaseSensitive(json, "cron");
    const cJSON *scope = cJSON_GetObjectItemCaseSensitive(json, "scope");
    if (!cJSON_IsString(cron) || !cJSON_IsString(scope) || cron->valuestring == NULL || scope->valuestring == NULL) {
        cJSON_Delete(json);
        send_error(req, "BAD_REQUEST", 400);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", "sched-0002");
    cJSON_AddStringToObject(root, "cron", cron->valuestring);
    cJSON_AddStringToObject(root, "scope", scope->valuestring);
    cJSON_AddBoolToObject(root, "enabled", true);

    cJSON_Delete(json);
    send_json_object(req, root, 201);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_catalog_sync_list_schedules(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *schedules = cJSON_AddArrayToObject(root, "schedules");

    for (size_t i = 0; i < sizeof(s_schedules) / sizeof(s_schedules[0]); ++i) {
        cJSON *schedule = cJSON_CreateObject();
        cJSON_AddStringToObject(schedule, "id", s_schedules[i].id);
        cJSON_AddStringToObject(schedule, "cron", s_schedules[i].cron);
        cJSON_AddStringToObject(schedule, "scope", s_schedules[i].scope);
        cJSON_AddBoolToObject(schedule, "enabled", s_schedules[i].enabled);
        cJSON_AddItemToArray(schedules, schedule);
    }

    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_catalog_sync_delete_schedule(httpd_req_t *req)
{
    const char *schedule_id = wildcard_tail(req->uri, "/api/v2/catalog-sync/schedules/");
    if (schedule_id == NULL || schedule_id[0] == '\0') {
        send_error(req, "BAD_REQUEST", 400);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", schedule_id);
    cJSON_AddStringToObject(root, "status", "deleted");

    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_ebins_catalog(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_AddArrayToObject(root, "items");

    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "name", "cpu_core.ebin");
    cJSON_AddStringToObject(item, "version", "1.0.0");
    cJSON_AddBoolToObject(item, "loaded", true);
    cJSON_AddItemToArray(items, item);

    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_ebins_rescan(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "queued");
    cJSON_AddNumberToObject(root, "queuedAtMs", (double)current_time_ms());
    send_json_object(req, root, 202);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_ebins_validate(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", cJSON_IsString(name) ? name->valuestring : "unknown");
    cJSON_AddBoolToObject(root, "valid", true);
    cJSON_AddStringToObject(root, "status", "ok");

    cJSON_Delete(json);
    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_ebins_load(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", cJSON_IsString(name) ? name->valuestring : "unknown");
    cJSON_AddStringToObject(root, "status", "loaded");

    cJSON_Delete(json);
    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_ebins_unload(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", cJSON_IsString(name) ? name->valuestring : "unknown");
    cJSON_AddStringToObject(root, "status", "unloaded");

    cJSON_Delete(json);
    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

void esptari_web_catalog_sync_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t sync_jobs_run = {.uri = "/api/v2/catalog-sync/jobs/run", .method = HTTP_POST, .handler = handle_catalog_sync_run, .user_ctx = NULL};
    httpd_uri_t sync_jobs_list = {.uri = "/api/v2/catalog-sync/jobs", .method = HTTP_GET, .handler = handle_catalog_sync_jobs, .user_ctx = NULL};
    httpd_uri_t sync_job_get = {.uri = "/api/v2/catalog-sync/jobs/*", .method = HTTP_GET, .handler = handle_catalog_sync_job_by_id, .user_ctx = NULL};
    httpd_uri_t sync_schedule_create = {.uri = "/api/v2/catalog-sync/schedules", .method = HTTP_POST, .handler = handle_catalog_sync_create_schedule, .user_ctx = NULL};
    httpd_uri_t sync_schedule_list = {.uri = "/api/v2/catalog-sync/schedules", .method = HTTP_GET, .handler = handle_catalog_sync_list_schedules, .user_ctx = NULL};
    httpd_uri_t sync_schedule_delete = {.uri = "/api/v2/catalog-sync/schedules/*", .method = HTTP_DELETE, .handler = handle_catalog_sync_delete_schedule, .user_ctx = NULL};
    httpd_uri_t ebins_catalog = {.uri = "/api/v2/ebins/catalog", .method = HTTP_GET, .handler = handle_ebins_catalog, .user_ctx = NULL};
    httpd_uri_t ebins_rescan = {.uri = "/api/v2/ebins/rescan", .method = HTTP_POST, .handler = handle_ebins_rescan, .user_ctx = NULL};
    httpd_uri_t ebins_validate = {.uri = "/api/v2/ebins/validate", .method = HTTP_POST, .handler = handle_ebins_validate, .user_ctx = NULL};
    httpd_uri_t ebins_load = {.uri = "/api/v2/ebins/load", .method = HTTP_POST, .handler = handle_ebins_load, .user_ctx = NULL};
    httpd_uri_t ebins_unload = {.uri = "/api/v2/ebins/unload", .method = HTTP_POST, .handler = handle_ebins_unload, .user_ctx = NULL};

    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_jobs_run));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_jobs_list));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_job_get));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_schedule_create));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_schedule_list));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_schedule_delete));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &ebins_catalog));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &ebins_rescan));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &ebins_validate));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &ebins_load));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &ebins_unload));

    ESP_LOGI(TAG, "Registered catalog sync and ebin routes");
}
