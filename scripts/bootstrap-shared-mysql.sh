#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ENV_FILE="${PROJECT_ROOT}/.env"
SCHEMA_FILE="${PROJECT_ROOT}/docker/mysql/init/01-init-chatserver.sql"

if [[ -f "${ENV_FILE}" ]]; then
  set -a
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
  set +a
fi

MYSQL_CONTAINER_NAME="${MYSQL_CONTAINER_NAME:-mysql_db}"
MYSQL_ROOT_USER="${MYSQL_ROOT_USER:-root}"
MYSQL_DATABASE="${MYSQL_DATABASE:-chatserver}"
MYSQL_USER="${MYSQL_USER:-chatserver_app}"
MYSQL_PASSWORD="${MYSQL_PASSWORD:-chatserver_dev_password}"

escape_sql_string() {
  printf "%s" "$1" | sed "s/'/''/g"
}

if [[ -z "${MYSQL_ROOT_PASSWORD:-}" ]]; then
  read -rsp "MySQL root password for ${MYSQL_CONTAINER_NAME}: " MYSQL_ROOT_PASSWORD
  echo
fi

if ! command -v docker >/dev/null 2>&1; then
  echo "docker command not found" >&2
  exit 1
fi

if [[ ! -f "${SCHEMA_FILE}" ]]; then
  echo "schema file not found: ${SCHEMA_FILE}" >&2
  exit 1
fi

if ! docker ps --format '{{.Names}}' | grep -Fxq "${MYSQL_CONTAINER_NAME}"; then
  echo "container not running: ${MYSQL_CONTAINER_NAME}" >&2
  exit 1
fi

if [[ ! "${MYSQL_DATABASE}" =~ ^[A-Za-z0-9_]+$ ]]; then
  echo "MYSQL_DATABASE must match ^[A-Za-z0-9_]+$: ${MYSQL_DATABASE}" >&2
  exit 1
fi

if [[ ! "${MYSQL_USER}" =~ ^[A-Za-z0-9_]+$ ]]; then
  echo "MYSQL_USER must match ^[A-Za-z0-9_]+$: ${MYSQL_USER}" >&2
  exit 1
fi

TMP_SQL="$(mktemp)"
cleanup() {
  rm -f "${TMP_SQL}"
}
trap cleanup EXIT

ESCAPED_PASSWORD="$(escape_sql_string "${MYSQL_PASSWORD}")"

cat > "${TMP_SQL}" <<EOF
CREATE DATABASE IF NOT EXISTS \`${MYSQL_DATABASE}\`
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

CREATE USER IF NOT EXISTS '${MYSQL_USER}'@'%' IDENTIFIED BY '${ESCAPED_PASSWORD}';

GRANT ALL PRIVILEGES ON \`${MYSQL_DATABASE}\`.* TO '${MYSQL_USER}'@'%';

FLUSH PRIVILEGES;
EOF

docker exec -i "${MYSQL_CONTAINER_NAME}" \
  mysql -u"${MYSQL_ROOT_USER}" -p"${MYSQL_ROOT_PASSWORD}" < "${TMP_SQL}"

docker exec -i "${MYSQL_CONTAINER_NAME}" \
  mysql -u"${MYSQL_ROOT_USER}" -p"${MYSQL_ROOT_PASSWORD}" "${MYSQL_DATABASE}" \
  < "${SCHEMA_FILE}"

echo "Bootstrap complete for database '${MYSQL_DATABASE}' with user '${MYSQL_USER}'."
