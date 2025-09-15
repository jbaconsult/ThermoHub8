from __future__ import annotations

from datetime import timedelta
from typing import Any, Dict, List

from homeassistant.core import HomeAssistant
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

from .api import ThermoHub8Client
from .const import DOMAIN, DEFAULT_SCAN_INTERVAL, CONF_SCAN_INTERVAL

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
            logger=hass.helpers.logger.LoggerAdapter(hass.logger, extra={"domain": DOMAIN}),
            name=f"{DOMAIN}_coordinator",
            update_interval=timedelta(seconds=scan_interval or DEFAULT_SCAN_INTERVAL),
        )
        self.client = client

    async def _async_update_data(self) -> Dict[str, Any]:
        try:
            payload = await self.client.async_get_readings()
        except Exception as err:  # noqa: BLE001
            raise UpdateFailed(err) from err
        return payload

