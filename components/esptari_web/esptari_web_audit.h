#pragma once

void esptari_web_audit_log(const char *actor,
                           const char *action,
                           const char *target,
                           const char *status,
                           const char *reason);
