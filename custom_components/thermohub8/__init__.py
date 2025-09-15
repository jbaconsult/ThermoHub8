from __future__ import annotations

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .const import DOMAIN, PLATFORMS, CONF_BASE_URL, CONF_API_KEY, CONF_VERIFY_SSL, CONF_SCAN_INTERVAL
from .api import ThermoHub8Client
from .coordinator import ThermoHub8Coordinator

type ThermoHub8ConfigEntry = ConfigEntry

async def async_setup_entry(hass: HomeAssistant, entry: ThermoHub8ConfigEntry) -> bool:
    """Eintrag einrichten."""
    session = async_get_clientsession(hass)
    base_url: str = entry.data[CONF_BASE_URL]
    api_key: str | None = entry.data.get(CONF_API_KEY)
    verify_ssl: bool = entry.data.get(CONF_VERIFY_SSL, True)
    scan_interval: int | None = entry.options.get(CONF_SCAN_INTERVAL) if entry.options else None

    client = ThermoHub8Client(session=session, base_url=base_url, api_key=api_key, verify_ssl=verify_ssl)
    coordinator = ThermoHub8Coordinator(hass, client, scan_interval)
    await coordinator.async_config_entry_first_refresh()

    hass.data.setdefault(DOMAIN, {})
    hass.data[DOMAIN][entry.entry_id] = {
        "client": client,
        "coordinator": coordinator,
        "base_url": base_url,
    }

    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    return True

async def async_unload_entry(hass: HomeAssistant, entry: ThermoHub8ConfigEntry) -> bool:
    """Eintrag entladen."""
    unload_ok = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if unload_ok:
        hass.data.get(DOMAIN, {}).pop(entry.entry_id, None)
    return unload_ok

async def async_reload_entry(hass: HomeAssistant, entry: ThermoHub8ConfigEntry) -> None:
    await async_unload_entry(hass, entry)
    await async_setup_entry(hass, entry)

