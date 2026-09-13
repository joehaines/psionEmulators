<?php
// Tracking endpoint for per-device usage analytics.
//
// Accepts a single JSON POST body:
//   { clientId, deviceId, event, sessionId?, durationMs? }
// where:
//   clientId  ::= UUID v4 (client-generated, persisted in localStorage)
//   deviceId  ::= one of the device IDs in $ALLOWED_DEVICES below
//   event     ::= 'load' | 'session' | 'cf_attach' | 'speaker_on' | 'mic_on'
//               | 'printer_capture'
//   sessionId ::= UUID v4, required when event='session'
//   durationMs::= unsigned int, foreground time in this session
//
// Responses:
//   204 No Content  on success or silent dedupe
//   400 Bad Request on validation failure (no body)
//   405             non-POST
//   503             when db_config.php is missing or the DB is unreachable
//
// The frontend swallows all of these silently — never log secrets,
// connection strings, or stack traces here.

header('Content-Type: application/json');
header('Cache-Control: no-store');

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
    http_response_code(405);
    exit;
}

// Must stay in sync with the device IDs in core/device_registry.cpp — a
// device missing here has every event rejected with a 400, so it silently
// never appears on the usage leaderboard. That drift is not visible from
// the outside (the client swallows the 400 like any other failure), so
// tests/unit/device-lists-sync.mts asserts this list against the registry
// and against frontend/public/device-names.json on every CI run.
$ALLOWED_DEVICES = [
    '5mx', '5mxpro', 'mc218', 'osaris', 'series5', 'series7', 'revo',
    'conan', 'netbook', 'netpad', 'series3', 'pocketbk', 'mc400',
    'mc400v126', 'conanv001', 'series3a', 'pocketbk2', 'series3c', 'series3mx', 'siena',
    'workabout', 'workaboutmx', 'organiser2',
];
$ALLOWED_EVENTS = [
    'load', 'session', 'cf_attach', 'speaker_on', 'mic_on',
    'printer_capture', 'printer_via_pc',
];
$MAX_DURATION_MS = 86_400_000;  // 24 hours

$raw = file_get_contents('php://input');
if ($raw === false || $raw === '' || strlen($raw) > 4096) {
    http_response_code(400);
    exit;
}
$body = json_decode($raw, true);
if (!is_array($body)) {
    http_response_code(400);
    exit;
}

$clientId  = $body['clientId']  ?? '';
$deviceId  = $body['deviceId']  ?? '';
$appId     = $body['appId']     ?? '';
$event     = $body['event']     ?? '';
$sessionId = $body['sessionId'] ?? null;
$duration  = $body['durationMs'] ?? null;

if (!is_string($clientId) || !preg_match('/^[0-9a-f-]{36}$/i', $clientId)) {
    http_response_code(400);
    exit;
}

// App-library events carry an appId (manifest slug "category/app")
// instead of a deviceId and land in the app_events table. The slug
// isn't validated against a server-side list — the manifest changes
// with every deploy — so it's charset/length-constrained instead, and
// rate-capped like device loads.
if ($event === 'app_try' || $event === 'app_download') {
    if (!is_string($appId) || !preg_match('#^[a-z0-9][a-z0-9/_-]{0,94}$#', $appId)) {
        http_response_code(400);
        exit;
    }
    $configPath = __DIR__ . '/db_config.php';
    if (!file_exists($configPath)) {
        http_response_code(503);
        exit;
    }
    try {
        $config = require $configPath;
        $pdo = new PDO($config['dsn'], $config['user'], $config['pass'], [
            PDO::ATTR_ERRMODE          => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_EMULATE_PREPARES => false,
        ]);
        $record = function (PDO $pdo) use ($clientId, $appId, $event): void {
            $check = $pdo->prepare(
                'SELECT 1 FROM app_events
                  WHERE client_id = :c AND app_id = :a AND event_type = :e
                    AND created_at > (NOW() - INTERVAL 60 SECOND)
                  LIMIT 1'
            );
            $check->execute([':c' => $clientId, ':a' => $appId, ':e' => $event]);
            if ($check->fetchColumn() === false) {
                $ins = $pdo->prepare(
                    'INSERT INTO app_events (client_id, app_id, event_type)
                     VALUES (:c, :a, :e)'
                );
                $ins->execute([':c' => $clientId, ':a' => $appId, ':e' => $event]);
            }
        };
        try {
            $record($pdo);
        } catch (PDOException $e) {
            // Table missing (42S02): bootstrap it and retry once. NOT run
            // up front — MySQL checks the CREATE privilege even for IF
            // NOT EXISTS, so on hosts whose DB user can't CREATE, an
            // unconditional bootstrap 503s every request even after the
            // table exists (observed live).
            if (($e->errorInfo[0] ?? '') !== '42S02') throw $e;
            $pdo->exec(
                'CREATE TABLE IF NOT EXISTS app_events (
                   id          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
                   client_id   CHAR(36)        NOT NULL,
                   app_id      VARCHAR(96)     NOT NULL,
                   event_type  VARCHAR(16)     NOT NULL,
                   created_at  TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP,
                   PRIMARY KEY (id),
                   KEY idx_app_event  (app_id, event_type),
                   KEY idx_app_recent (client_id, app_id, event_type, created_at)
                 ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4'
            );
            $record($pdo);
        }
        http_response_code(204);
    } catch (Throwable $e) {
        http_response_code(503);
    }
    exit;
}

if (!is_string($deviceId) || !in_array($deviceId, $ALLOWED_DEVICES, true)) {
    http_response_code(400);
    exit;
}
if (!is_string($event) || !in_array($event, $ALLOWED_EVENTS, true)) {
    http_response_code(400);
    exit;
}
if ($sessionId !== null) {
    if (!is_string($sessionId) || !preg_match('/^[0-9a-f-]{36}$/i', $sessionId)) {
        http_response_code(400);
        exit;
    }
}
if ($duration !== null) {
    if (!is_numeric($duration)) {
        http_response_code(400);
        exit;
    }
    $duration = (int)$duration;
    if ($duration < 0 || $duration > $MAX_DURATION_MS) {
        http_response_code(400);
        exit;
    }
}

$configPath = __DIR__ . '/db_config.php';
if (!file_exists($configPath)) {
    http_response_code(503);
    exit;
}

try {
    $config = require $configPath;
    $pdo = new PDO($config['dsn'], $config['user'], $config['pass'], [
        PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES   => false,
    ]);

    if ($event === 'load') {
        // Cap one 'load' per (client, device) per 60 s so a refresh-spammer
        // can't trivially inflate counts.
        $check = $pdo->prepare(
            'SELECT 1 FROM device_events
              WHERE client_id = :c AND device_id = :d AND event_type = \'load\'
                AND created_at > (NOW() - INTERVAL 60 SECOND)
              LIMIT 1'
        );
        $check->execute([':c' => $clientId, ':d' => $deviceId]);
        if ($check->fetchColumn() !== false) {
            http_response_code(204);
            exit;
        }
        $ins = $pdo->prepare(
            'INSERT INTO device_events (client_id, device_id, event_type)
             VALUES (:c, :d, \'load\')'
        );
        $ins->execute([':c' => $clientId, ':d' => $deviceId]);
    } elseif ($event === 'session') {
        if ($sessionId === null || $duration === null) {
            http_response_code(400);
            exit;
        }
        // Idempotent upsert keyed on (client_id, session_id) — many
        // heartbeats collapse to a single row carrying the max duration
        // observed. GREATEST guards against out-of-order beacons.
        $stmt = $pdo->prepare(
            'INSERT INTO device_session_totals
               (client_id, session_id, device_id, duration_ms)
             VALUES (:c, :s, :d, :dur)
             ON DUPLICATE KEY UPDATE
               duration_ms = GREATEST(duration_ms, VALUES(duration_ms)),
               device_id   = VALUES(device_id)'
        );
        $stmt->execute([
            ':c'   => $clientId,
            ':s'   => $sessionId,
            ':d'   => $deviceId,
            ':dur' => $duration,
        ]);
    } else {
        // Feature event — append a row.
        $ins = $pdo->prepare(
            'INSERT INTO device_events (client_id, device_id, event_type)
             VALUES (:c, :d, :e)'
        );
        $ins->execute([
            ':c' => $clientId,
            ':d' => $deviceId,
            ':e' => $event,
        ]);
    }

    http_response_code(204);
} catch (Throwable $e) {
    // Swallow — the client treats any non-2xx as failure-and-forget.
    http_response_code(503);
}
