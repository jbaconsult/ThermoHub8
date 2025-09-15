from __future__ import annotations

from datetime import timedelta
from typing import Any, Dict, List

from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

from .api import ThermoHub8Client
from .const import DOMAIN, DEFAULT_SCAN_INTERVAL, CONF_SCAN_INTERVAL

import logging
_LOGGER = logging.getLogger(__name__)


class ThermoHub8Coordinator(DataUpdateCoordinator[Dict[str, Any]]):
    """Koordinator, der periodisch Messwerte lädt."""

    def __init__(
        self,
        hass: HomeAssistant,
        client: ThermoHub8Client,
        scan_interval: int | None,
    ) -> None:
        super().__init__(
            hass,
            logger=_LOGGER,
            name=f"{DOMAIN}_coordinator",
            update_interval=timedelta(seconds=scan_interval or DEFAULT_SCAN_INTERVAL),
        )
        _LOGGER.info("ThermoHub8Coordinator created (interval=%ss)", (scan_interval or DEFAULT_SCAN_INTERVAL))
        self.client = client

    async def _async_update_data(self) -> Dict[str, Any]:
        _LOGGER.debug("Coordinator update triggered")
        try:
            payload = await self.client.async_get_readings()
            sensors = self.client.normalize_payload(payload)
            _LOGGER.info("ThermoHub8 fetched %d sensor(s); ts=%s", len(sensors), payload.get("ts"))
        except Exception as err:
            _LOGGER.error("ThermoHub8 update failed: %s", err)
            raise
        return payload
