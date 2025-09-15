from __future__ import annotations

DOMAIN = "thermohub8"

CONF_BASE_URL = "base_url"
CONF_API_KEY = "api_key"
CONF_VERIFY_SSL = "verify_ssl"
CONF_SCAN_INTERVAL = "scan_interval"

DEFAULT_VERIFY_SSL = True
DEFAULT_SCAN_INTERVAL = 5  # Sekunden – gerne anpassen
MAX_SENSORS = 8

PLATFORMS = ["sensor"]

ATTR_LAST_UPDATE = "last_update"

