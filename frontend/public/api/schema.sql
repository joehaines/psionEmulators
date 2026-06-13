-- Psion emulator usage analytics schema.
--
-- Run once per environment, via phpMyAdmin or `mysql < schema.sql`.
-- Both tables are append-only from the client's point of view; the only
-- updates come from heartbeat UPSERTs into device_session_totals.

CREATE TABLE IF NOT EXISTS device_events (
  id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  client_id   CHAR(36)        NOT NULL,
  device_id   VARCHAR(64)     NOT NULL,
  event_type  VARCHAR(32)     NOT NULL,
  session_id  CHAR(36)        NULL,
  duration_ms INT UNSIGNED    NULL,
  created_at  TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_device_event   (device_id, event_type),
  KEY idx_created        (created_at),
  KEY idx_client_recent  (client_id, device_id, event_type, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Per-(client, session) running total. Heartbeats UPSERT here so a long
-- session contributes a single row holding the max duration seen, which
-- avoids double-counting when the leaderboard query sums across sessions.
CREATE TABLE IF NOT EXISTS device_session_totals (
  client_id   CHAR(36)     NOT NULL,
  session_id  CHAR(36)     NOT NULL,
  device_id   VARCHAR(64)  NOT NULL,
  duration_ms INT UNSIGNED NOT NULL DEFAULT 0,
  updated_at  TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (client_id, session_id),
  KEY idx_device (device_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- App-library events: one row per "try it on a device" launch or per
-- download. app_id is the slug from the generated apps manifest (NOT
-- validated against a server-side list -- the manifest evolves with
-- deploys -- so it's length/charset-constrained instead).
CREATE TABLE IF NOT EXISTS app_events (
  id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  client_id   CHAR(36)        NOT NULL,
  app_id      VARCHAR(96)     NOT NULL,
  event_type  VARCHAR(16)     NOT NULL,   -- 'app_try' | 'app_download'
  created_at  TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  KEY idx_app_event  (app_id, event_type),
  KEY idx_app_recent (client_id, app_id, event_type, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
