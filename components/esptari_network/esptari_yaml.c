/**
 * @file esptari_yaml.c
 * @brief Lightweight YAML subset parser for network configuration
 *
 * Line-by-line parser that tracks indentation depth to handle nested
 * key:value mappings. Supports the Netplan-style YAML format used by
 * espTari's network configuration.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "esptari_yaml.h"

static const char *TAG = "yaml";

/*---------------------------------------------------------------------
 * Internal helpers
 *-------------------------------------------------------------------*/

/** Safe string copy — always NUL terminates, no truncation warning. */
static void safe_copy(char *dst, const char *src, size_t dst_sz)
{
    if (dst_sz == 0) return;
    size_t len = strlen(src);
    if (len >= dst_sz) len = dst_sz - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/** Trim leading whitespace, return indent depth (in spaces). */
static int count_indent(const char *line)
{
    int n = 0;
    while (line[n] == ' ') n++;
    return n;
}

/** Trim trailing whitespace / newlines in-place. */
static void rtrim(char *s)
{
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\r' ||
                       s[len - 1] == '\n' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

/** Strip surrounding quotes from a value string in-place. */
static void strip_quotes(char *s)
{
    int len = strlen(s);
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') ||
                     (s[0] == '\'' && s[len - 1] == '\''))) {
        memmove(s, s + 1, len - 2);
        s[len - 2] = '\0';
    }
}

/** Parse a "key: value" line.  Returns false if line is blank/comment. */
static bool parse_kv(const char *line, char *key, int key_sz,
                     char *val, int val_sz)
{
    /* Skip indent */
    while (*line == ' ') line++;

    /* Skip blank lines and comments */
    if (*line == '\0' || *line == '#' || *line == '\n') return false;

    /* Check for list item "- xxx" */
    if (*line == '-') {
        /* Store key as "-" and value as the rest */
        strncpy(key, "-", key_sz);
        key[key_sz - 1] = '\0';
        line++;
        while (*line == ' ') line++;
        safe_copy(val, line, val_sz);
        rtrim(val);
        strip_quotes(val);
        return true;
    }

    /* Find colon separator */
    const char *colon = strchr(line, ':');
    if (!colon) return false;

    int klen = colon - line;
    if (klen <= 0 || klen >= key_sz) return false;
    memcpy(key, line, klen);
    key[klen] = '\0';
    rtrim(key);
    strip_quotes(key);

    /* Value is after colon */
    const char *vp = colon + 1;
    while (*vp == ' ') vp++;

    if (*vp == '\0' || *vp == '\n' || *vp == '#') {
        /* No value — this is a mapping key (value on next indented lines) */
        val[0] = '\0';
    } else {
        safe_copy(val, vp, val_sz);
        rtrim(val);
        strip_quotes(val);
    }

    return true;
}

/** Check if value is boolean "true" / "yes" */
static bool is_true(const char *v)
{
    return (strcasecmp(v, "true") == 0 || strcasecmp(v, "yes") == 0 ||
            strcmp(v, "1") == 0);
}

/*---------------------------------------------------------------------
 * Parser state machine
 *-------------------------------------------------------------------*/

/** Parser context — tracks which section/subsection we're in */
typedef enum {
    SEC_ROOT = 0,
    SEC_NETWORK,
    SEC_ETHERNETS,
    SEC_ETH0,
    SEC_WIFIS,
    SEC_WLAN0,
    SEC_WLAN0_AP,           /* inside access-points */
    SEC_WLAN0_AP_ENTRY,     /* specific AP entry */
    SEC_ROUTING,
    SEC_SERVICES,
    SEC_MDNS,
    SEC_MDNS_SERVICES,
} yaml_section_t;

esp_err_t esptari_yaml_parse(const char *yaml_str,
                              esptari_net_config_t *config)
{
    if (!yaml_str || !config) return ESP_ERR_INVALID_ARG;

    /* Defaults */
    memset(config, 0, sizeof(*config));
    config->wifi_enabled = true;
    config->eth_enabled = true;
    config->wifi_ip.dhcp = true;
    config->eth_ip.dhcp = true;
    config->wifi_priority = 10;
    config->eth_priority = 0;
    config->default_interface = ESPTARI_IF_ETH;
    config->failover_enabled = true;
    config->failover_timeout_ms = 5000;
    config->mdns_enabled = true;
    strncpy(config->hostname, "esptari", sizeof(config->hostname));

    yaml_section_t section = SEC_ROOT;
    int section_indent[12] = {0};  /* indent level per section depth */

    /* Work with a mutable copy for strtok */
    char *buf = strdup(yaml_str);
    if (!buf) return ESP_ERR_NO_MEM;

    char *saveptr = NULL;
    char *line = strtok_r(buf, "\n", &saveptr);

    char key[64], val[128];

    /* Track which AP entry we're filling */
    int current_ap = -1;

    while (line) {
        int indent = count_indent(line);
        char raw_line[256];
        strncpy(raw_line, line, sizeof(raw_line));
        raw_line[sizeof(raw_line) - 1] = '\0';
        rtrim(raw_line);

        if (!parse_kv(raw_line, key, sizeof(key), val, sizeof(val))) {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        /* Pop section stack when indent decreases — walk up the section
         * hierarchy until we find a section whose indent is strictly less
         * than the current line's indent.                                   */
        while (section != SEC_ROOT && indent <= section_indent[section]) {
            switch (section) {
                case SEC_WLAN0_AP_ENTRY: section = SEC_WLAN0_AP;  break;
                case SEC_WLAN0_AP:       section = SEC_WLAN0;     break;
                case SEC_WLAN0:          section = SEC_WIFIS;     break;
                case SEC_WIFIS:          section = SEC_NETWORK;   break;
                case SEC_ETH0:           section = SEC_ETHERNETS; break;
                case SEC_ETHERNETS:      section = SEC_NETWORK;   break;
                case SEC_MDNS_SERVICES:  section = SEC_MDNS;      break;
                case SEC_MDNS:           section = SEC_SERVICES;  break;
                case SEC_SERVICES:       section = SEC_NETWORK;   break;
                case SEC_ROUTING:        section = SEC_NETWORK;   break;
                case SEC_NETWORK:        section = SEC_ROOT;      break;
                default:                 section = SEC_ROOT;      break;
            }
        }

        /* Section entry detection */
        if (strcmp(key, "network") == 0 && val[0] == '\0') {
            section = SEC_NETWORK;
            section_indent[SEC_NETWORK] = indent;
        } else if (section == SEC_NETWORK && strcmp(key, "ethernets") == 0) {
            section = SEC_ETHERNETS;
            section_indent[SEC_ETHERNETS] = indent;
        } else if (section == SEC_ETHERNETS && strcmp(key, "eth0") == 0) {
            section = SEC_ETH0;
            section_indent[SEC_ETH0] = indent;
            config->eth_enabled = true;
        } else if (section == SEC_NETWORK && strcmp(key, "wifis") == 0) {
            section = SEC_WIFIS;
            section_indent[SEC_WIFIS] = indent;
        } else if (section == SEC_WIFIS && strcmp(key, "wlan0") == 0) {
            section = SEC_WLAN0;
            section_indent[SEC_WLAN0] = indent;
            config->wifi_enabled = true;
        } else if (section == SEC_WLAN0 && strcmp(key, "access-points") == 0) {
            section = SEC_WLAN0_AP;
            section_indent[SEC_WLAN0_AP] = indent;
        } else if (section == SEC_NETWORK && strcmp(key, "routing") == 0) {
            section = SEC_ROUTING;
            section_indent[SEC_ROUTING] = indent;
        } else if (section == SEC_NETWORK && strcmp(key, "services") == 0) {
            section = SEC_SERVICES;
            section_indent[SEC_SERVICES] = indent;
        } else if (section == SEC_SERVICES && strcmp(key, "mdns") == 0) {
            section = SEC_MDNS;
            section_indent[SEC_MDNS] = indent;
        }
        /* Handle values in each section */
        else if (section == SEC_NETWORK) {
            if (strcmp(key, "version") == 0) {
                /* ignore — always version 1 */
            } else if (strcmp(key, "renderer") == 0) {
                /* ignore */
            }
        } else if (section == SEC_ETH0) {
            if (strcmp(key, "dhcp4") == 0) {
                config->eth_ip.dhcp = is_true(val);
            } else if (strcmp(key, "optional") == 0) {
                /* optional=true means don't fail if interface is down */
            } else if (strcmp(key, "addresses") == 0) {
                /* handled as list items */
            } else if (strcmp(key, "-") == 0 && val[0]) {
                /* Static IP address entry "192.168.1.100/24" */
                char *slash = strchr(val, '/');
                if (slash) *slash = '\0';
                safe_copy(config->eth_ip.ip, val, sizeof(config->eth_ip.ip));
                config->eth_ip.dhcp = false;
            } else if (strcmp(key, "gateway4") == 0) {
                safe_copy(config->eth_ip.gateway, val, sizeof(config->eth_ip.gateway));
            }
        } else if (section == SEC_WLAN0) {
            if (strcmp(key, "dhcp4") == 0) {
                config->wifi_ip.dhcp = is_true(val);
            } else if (strcmp(key, "priority") == 0) {
                config->wifi_priority = atoi(val);
            }
        } else if (section == SEC_WLAN0_AP) {
            /* An AP entry line — key is the SSID (quoted), val is empty */
            if (val[0] == '\0' && strcmp(key, "-") != 0) {
                current_ap = config->wifi_ap_count;
                if (current_ap < ESPTARI_NET_MAX_AP_ENTRIES) {
                    safe_copy(config->wifi_aps[current_ap].ssid, key,
                              sizeof(config->wifi_aps[current_ap].ssid));
                    config->wifi_ap_count++;
                    section = SEC_WLAN0_AP_ENTRY;
                    section_indent[SEC_WLAN0_AP_ENTRY] = indent;
                }
            }
        } else if (section == SEC_WLAN0_AP_ENTRY) {
            if (strcmp(key, "password") == 0 && current_ap >= 0 &&
                current_ap < ESPTARI_NET_MAX_AP_ENTRIES) {
                safe_copy(config->wifi_aps[current_ap].password, val,
                          sizeof(config->wifi_aps[current_ap].password));
            }
        } else if (section == SEC_ROUTING) {
            if (strcmp(key, "default-interface") == 0) {
                if (strcmp(val, "eth0") == 0) {
                    config->default_interface = ESPTARI_IF_ETH;
                } else {
                    config->default_interface = ESPTARI_IF_WIFI;
                }
            } else if (strcmp(key, "failover") == 0) {
                config->failover_enabled = is_true(val);
            } else if (strcmp(key, "failover-timeout-ms") == 0) {
                config->failover_timeout_ms = atoi(val);
            }
        } else if (section == SEC_MDNS) {
            if (strcmp(key, "enabled") == 0) {
                config->mdns_enabled = is_true(val);
            } else if (strcmp(key, "hostname") == 0) {
                safe_copy(config->hostname, val, sizeof(config->hostname));
            }
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(buf);
    ESP_LOGI(TAG, "Parsed config: wifi=%d (%d APs), eth=%d, mdns=%d (%s)",
             config->wifi_enabled, config->wifi_ap_count,
             config->eth_enabled, config->mdns_enabled, config->hostname);
    return ESP_OK;
}

/*---------------------------------------------------------------------
 * Serializer
 *-------------------------------------------------------------------*/

int esptari_yaml_serialize(const esptari_net_config_t *config,
                            char *buf, int buf_sz)
{
    if (!config || !buf || buf_sz <= 0) return -1;

    int pos = 0;

#define APPEND(...) do { \
    int n = snprintf(buf + pos, buf_sz - pos, __VA_ARGS__); \
    if (n < 0 || pos + n >= buf_sz) return -1; \
    pos += n; \
} while(0)

    APPEND("# espTari Network Configuration\n");
    APPEND("network:\n");
    APPEND("  version: 1\n");
    APPEND("  renderer: esptari\n\n");

    /* Ethernet */
    if (config->eth_enabled) {
        APPEND("  ethernets:\n");
        APPEND("    eth0:\n");
        APPEND("      dhcp4: %s\n", config->eth_ip.dhcp ? "true" : "false");
        if (!config->eth_ip.dhcp && config->eth_ip.ip[0]) {
            APPEND("      addresses:\n");
            APPEND("        - %s/%s\n", config->eth_ip.ip,
                   config->eth_ip.netmask[0] ? config->eth_ip.netmask : "24");
            if (config->eth_ip.gateway[0])
                APPEND("      gateway4: %s\n", config->eth_ip.gateway);
        }
        APPEND("\n");
    }

    /* WiFi */
    if (config->wifi_enabled) {
        APPEND("  wifis:\n");
        APPEND("    wlan0:\n");
        APPEND("      dhcp4: %s\n", config->wifi_ip.dhcp ? "true" : "false");
        APPEND("      optional: true\n");
        if (config->wifi_ap_count > 0) {
            APPEND("      access-points:\n");
            for (int i = 0; i < config->wifi_ap_count; i++) {
                APPEND("        \"%s\":\n", config->wifi_aps[i].ssid);
                if (config->wifi_aps[i].password[0])
                    APPEND("          password: \"%s\"\n",
                           config->wifi_aps[i].password);
            }
        }
        APPEND("      priority: %d\n\n", config->wifi_priority);
    }

    /* Routing */
    APPEND("  routing:\n");
    APPEND("    default-interface: %s\n",
           config->default_interface == ESPTARI_IF_ETH ? "eth0" : "wlan0");
    APPEND("    failover: %s\n", config->failover_enabled ? "true" : "false");
    APPEND("    failover-timeout-ms: %d\n\n", config->failover_timeout_ms);

    /* mDNS */
    APPEND("  services:\n");
    APPEND("    mdns:\n");
    APPEND("      enabled: %s\n", config->mdns_enabled ? "true" : "false");
    APPEND("      hostname: %s\n", config->hostname);

#undef APPEND

    return pos;
}

/*---------------------------------------------------------------------
 * File I/O
 *-------------------------------------------------------------------*/

esp_err_t esptari_yaml_load_file(const char *path,
                                  esptari_net_config_t *config)
{
    if (!path || !config) return ESP_ERR_INVALID_ARG;

    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "Config file not found: %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 8192) {
        fclose(f);
        ESP_LOGE(TAG, "Config file invalid size: %ld", sz);
        return ESP_ERR_INVALID_SIZE;
    }

    char *buf = malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    buf[rd] = '\0';

    esp_err_t ret = esptari_yaml_parse(buf, config);
    free(buf);
    return ret;
}

esp_err_t esptari_yaml_save_file(const char *path,
                                  const esptari_net_config_t *config)
{
    if (!path || !config) return ESP_ERR_INVALID_ARG;

    /* Heap-allocate to avoid stack overflow (main task has limited stack) */
    const int buf_sz = 2048;
    char *buf = malloc(buf_sz);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate serialize buffer");
        return ESP_ERR_NO_MEM;
    }

    int len = esptari_yaml_serialize(config, buf, buf_sz);
    if (len < 0) {
        ESP_LOGE(TAG, "Failed to serialize config");
        free(buf);
        return ESP_FAIL;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        free(buf);
        return ESP_FAIL;
    }

    size_t written = fwrite(buf, 1, len, f);
    fclose(f);
    free(buf);

    if ((int)written != len) {
        ESP_LOGE(TAG, "Incomplete write: %d/%d", (int)written, len);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Config saved to %s (%d bytes)", path, len);
    return ESP_OK;
}
