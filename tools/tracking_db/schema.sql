PRAGMA journal_mode=WAL;
PRAGMA foreign_keys=ON;

CREATE TABLE IF NOT EXISTS tasks (
  task_id TEXT PRIMARY KEY,
  epic TEXT,
  objective TEXT NOT NULL,
  priority TEXT,
  size TEXT,
  sprint TEXT,
  status TEXT,
  dependencies_raw TEXT,
  source_file TEXT NOT NULL,
  source_line INTEGER NOT NULL,
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS task_dependencies (
  task_id TEXT NOT NULL,
  depends_on TEXT NOT NULL,
  PRIMARY KEY (task_id, depends_on),
  FOREIGN KEY(task_id) REFERENCES tasks(task_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS acceptance_decisions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  decision_date TEXT,
  sprint TEXT,
  task_ref TEXT,
  decision TEXT,
  notes TEXT,
  evidence_link TEXT,
  source_file TEXT NOT NULL,
  source_line INTEGER NOT NULL,
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS acceptance_artifacts (
  decision_id INTEGER NOT NULL,
  artifact_path TEXT NOT NULL,
  PRIMARY KEY (decision_id, artifact_path),
  FOREIGN KEY(decision_id) REFERENCES acceptance_decisions(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS kanban_cards (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  column_name TEXT NOT NULL,
  card_text TEXT NOT NULL,
  task_id TEXT,
  source_file TEXT NOT NULL,
  source_line INTEGER NOT NULL,
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS task_events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  task_id TEXT NOT NULL,
  event_type TEXT NOT NULL,
  from_status TEXT,
  to_status TEXT,
  note TEXT,
  actor TEXT NOT NULL,
  created_at TEXT NOT NULL,
  FOREIGN KEY(task_id) REFERENCES tasks(task_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS progress_sessions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  started_at TEXT NOT NULL,
  ended_at TEXT,
  summary TEXT,
  actor TEXT NOT NULL
);

CREATE VIRTUAL TABLE IF NOT EXISTS acceptance_fts USING fts5(
  task_ref,
  notes,
  evidence_link,
  content='acceptance_decisions',
  content_rowid='id'
);

CREATE INDEX IF NOT EXISTS idx_tasks_status ON tasks(status);
CREATE INDEX IF NOT EXISTS idx_tasks_priority ON tasks(priority);
CREATE INDEX IF NOT EXISTS idx_acceptance_task_ref ON acceptance_decisions(task_ref);
CREATE INDEX IF NOT EXISTS idx_kanban_column ON kanban_cards(column_name);
CREATE INDEX IF NOT EXISTS idx_task_events_task_id ON task_events(task_id);
CREATE INDEX IF NOT EXISTS idx_task_events_created_at ON task_events(created_at);
