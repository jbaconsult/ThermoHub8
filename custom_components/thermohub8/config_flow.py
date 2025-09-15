from __future__ import annotations

from typing import Any, Dict

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.core import HomeAssistant
from homeassistant.data_entry_flow import FlowResult
from homeassistant.helpers.aiohttp_client import async_get_clientsession

import logging
_LOGGER = logging.getLogger(__name__)

from .const import (
    DOMAIN,
    CONF_BASE_URL,
    CONF_API_KEY,
    CONF_VERIFY_SSL,
    CONF_SCAN_INTERVAL,
    DEFAULT_VERIFY_SSL,
    DEFAULT_SCAN_INTERVAL,
)
from .api import ThermoHub8Client

STEP_USER_DATA_SCHEMA = vol.Schema(
    {
        vol.Required(CONF_BASE_URL): str,  # z.B. http://thermohub.local:8080
        vol.Optional(CONF_API_KEY): str,
        vol.Optional(CONF_VERIFY_SSL, default=DEFAULT_VERIFY_SSL): bool,
    }
)

STEP_OPTIONS_DATA_SCHEMA = vol.Schema(
    {
        vol.Required(CONF_SCAN_INTERVAL, default=DEFAULT_SCAN_INTERVAL): vol.All(int, vol.Range(min=1, max=60)),
    }
)

async def _validate_connection(hass: HomeAssistant, data: Dict[str, Any]) -> Dict[str, Any]:
    _LOGGER.info("Validating ThermoHub8 connection to %s", data.get("base_url"))
    session = async_get_clientsession(hass)
    client = ThermoHub8Client(
        session=session,
        base_url=data.get("base_url"),
        api_key=data.get(CONF_API_KEY),
        verify_ssl=data.get(CONF_VERIFY_SSL, True),
    )
    
    _LOGGER.info("Starting Validation")

    # einmalig abrufen, um zu prüfen
    payload = await client.async_get_readings()
    sensors = ThermoHub8Client.normalize_payload(payload)
    _LOGGER.info("Validation OK: %d sensor(s) found", len(sensors))
    return payload

class ConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    VERSION = 1

    async def async_step_user(self, user_input: Dict[str, Any] | None = None) -> FlowResult:
        errors: Dict[str, str] = {}
        if user_input is not None:
            try:
                await _validate_connection(self.hass, user_input)
            except Exception as e:  # noqa: BLE001
                _LOGGER.warning("ThermoHub8 validation failed: %s", e)
                errors["base"] = "cannot_connect"
            else:
                # Eindeutigkeit über Basis-URL
                _LOGGER.info("ThermoHub8 creating config entry for %s", user_input.get("base_url"))
                await self.async_set_unique_id(user_input[CONF_BASE_URL].rstrip("/"))
                self._abort_if_unique_id_configured()
                return self.async_create_entry(title="ThermoHub8", data=user_input)

        return self.async_show_form(step_id="user", data_schema=STEP_USER_DATA_SCHEMA, errors=errors)

    async def async_step_import(self, user_input: Dict[str, Any]) -> FlowResult:
        # Unterstützung für YAML-Import, falls gewünscht
        return await self.async_step_user(user_input)

    async def async_step_reauth(self, user_input: Dict[str, Any] | None = None) -> FlowResult:
        return await self.async_step_user(user_input)

    @staticmethod
    def async_get_options_flow(config_entry):
        return OptionsFlowHandler(config_entry)

class OptionsFlowHandler(config_entries.OptionsFlow):
    def __init__(self, config_entry: config_entries.ConfigEntry) -> None:
        self._entry = config_entry

    async def async_step_init(self, user_input: Dict[str, Any] | None = None) -> FlowResult:
        if user_input is not None:
            return self.async_create_entry(title="", data=user_input)

        current = {
            CONF_SCAN_INTERVAL: self._entry.options.get(CONF_SCAN_INTERVAL, DEFAULT_SCAN_INTERVAL),
        }
        return self.async_show_form(step_id="init", data_schema=vol.Schema(
            {vol.Required(CONF_SCAN_INTERVAL, default=current[CONF_SCAN_INTERVAL]): vol.All(int, vol.Range(min=1, max=60))}
        ))

