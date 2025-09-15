from __future__ import annotations

from typing import Any, Dict

from homeassistant.core import HomeAssistant
from homeassistant.helpers.typing import ConfigEntry
from .const import DOMAIN

async def async_get_config_entry_diagnostics(
    hass: HomeAssistant, entry: ConfigEntry
) -> Dict[str, Any]:
    data = hass.data.get(DOMAIN, {}).get(entry.entry_id, {})
    coordinator = data.get("coordinator")
    payload = coordinator.data if coordinator else {}
    # Persönliche Daten entfernen – hier nur Beispiel
    redacted = {"payload_keys": list(payload.keys()), "sample": payload}
    return redacted

