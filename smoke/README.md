# Python Smoke Suite

This directory now supports descriptor-driven smoke tests.

## Descriptor format

Add files matching:

- `test_[test_description]_[PhaseOrSprintTaskId].test`

Descriptors are TOML files with:

- `[meta]`: `test_id`, `task_id`, `description`
- `[config]`: `base_url`, `auth_scope`, `timeout_s`, `continue_on_failure`
- `[[step]]`: `name`, `method`, `path`, `body_json`, and expectations (`expect_status`, `expect_error_code`, `expect_ok`, `expect_json`)

## Runner

- Script: `tools/smoke_suite/smoke_runner.py`
- DB evidence store: `TRACKING/smoke_results.db`

Run all descriptors:

```bash
python3 tools/smoke_suite/smoke_runner.py --all
```

Run one descriptor by filename, full path, or `meta.test_id`:

```bash
python3 tools/smoke_suite/smoke_runner.py --test test_input_mapping_T070.test
python3 tools/smoke_suite/smoke_runner.py --test smoke/descriptors/test_input_mapping_T070.test
python3 tools/smoke_suite/smoke_runner.py --test test_input_mapping_T070
```

List discovered tests:

```bash
python3 tools/smoke_suite/smoke_runner.py --list
```

Override base URL:

```bash
python3 tools/smoke_suite/smoke_runner.py --all --base-url http://192.168.1.196
```

## Evidence model

- No timestamped evidence filenames are required.
- Timestamps are embedded in run rows (`started_at`, `ended_at`) in SQLite.
- Step-level response evidence is stored in `smoke_test_step_results`.
