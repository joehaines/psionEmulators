<?php
// Public read endpoint: aggregated per-device usage stats.
//
// Response: JSON array of
//   { deviceId, loadCount, totalTimeMs, uniqueClients }
// ordered by loadCount DESC. Empty array on no rows / DB error.

header('Content-Type: application/json');
header('Cache-Control: public, max-age=60');

if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'GET') {
    http_response_code(405);
    exit;
}

$configPath = __DIR__ . '/db_config.php';
if (!file_exists($configPath)) {
    http_response_code(503);
    echo '[]';
    exit;
}

// Bootstrap for the app_events table (same shape as schema.sql). Only
// called after a 42S02 "table doesn't exist" — see the apps view below.
function psn_create_app_events(PDO $pdo): void {
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
}

try {
    $config = require $configPath;
    $pdo = new PDO($config['dsn'], $config['user'], $config['pass'], [
        PDO::ATTR_ERRMODE            => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES   => false,
    ]);

    // ?view=apps — app-library popularity: per-app try / download
    // counts and unique clients, ordered by combined activity.
    if (($_GET['view'] ?? '') === 'apps') {
        $sql = '
            SELECT
              app_id                                                       AS appId,
              SUM(CASE WHEN event_type = \'app_try\'      THEN 1 ELSE 0 END) AS tryCount,
              SUM(CASE WHEN event_type = \'app_download\' THEN 1 ELSE 0 END) AS downloadCount,
              COUNT(DISTINCT client_id)                                    AS uniqueClients
            FROM app_events
            GROUP BY app_id
            ORDER BY (SUM(CASE WHEN event_type = \'app_try\'      THEN 1 ELSE 0 END)
                    + SUM(CASE WHEN event_type = \'app_download\' THEN 1 ELSE 0 END)) DESC,
                     appId
            LIMIT 500
        ';
        try {
            $rows = $pdo->query($sql)->fetchAll();
        } catch (PDOException $e) {
            // Table missing (42S02): bootstrap it and retry once. The
            // CREATE is deliberately NOT run up front — MySQL checks the
            // CREATE privilege even for IF NOT EXISTS, so on hosts whose
            // DB user can't CREATE, an unconditional bootstrap 503s every
            // request even after the table exists (observed live).
            if (($e->errorInfo[0] ?? '') !== '42S02') throw $e;
            psn_create_app_events($pdo);
            $rows = $pdo->query($sql)->fetchAll();
        }
        $out = [];
        foreach ($rows as $r) {
            $out[] = [
                'appId'         => (string)$r['appId'],
                'tryCount'      => (int)$r['tryCount'],
                'downloadCount' => (int)$r['downloadCount'],
                'uniqueClients' => (int)$r['uniqueClients'],
            ];
        }
        echo json_encode($out);
        exit;
    }

    // Join load counts + unique clients from device_events against the
    // pre-aggregated per-session totals. LEFT JOIN means devices that
    // exist in events but have no recorded session time still show up
    // with totalTimeMs=0.
    $sql = '
        SELECT
          d.device_id                                            AS deviceId,
          SUM(CASE WHEN d.event_type = \'load\' THEN 1 ELSE 0 END) AS loadCount,
          COALESCE(t.total_ms, 0)                                AS totalTimeMs,
          COUNT(DISTINCT d.client_id)                            AS uniqueClients
        FROM device_events d
        LEFT JOIN (
          SELECT device_id, SUM(duration_ms) AS total_ms
          FROM device_session_totals
          GROUP BY device_id
        ) t ON t.device_id = d.device_id
        GROUP BY d.device_id
        ORDER BY loadCount DESC
    ';
    $rows = $pdo->query($sql)->fetchAll();

    $out = [];
    foreach ($rows as $r) {
        $out[] = [
            'deviceId'      => (string)$r['deviceId'],
            'loadCount'     => (int)$r['loadCount'],
            'totalTimeMs'   => (int)$r['totalTimeMs'],
            'uniqueClients' => (int)$r['uniqueClients'],
        ];
    }
    echo json_encode($out);
} catch (Throwable $e) {
    http_response_code(503);
    echo '[]';
}
