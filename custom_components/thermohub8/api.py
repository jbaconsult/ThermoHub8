from __future__ import annotations

from typing import Any, Dict, List, Optional
import asyncio
import yarl
from aiohttp import ClientSession, ClientResponseError
import logging

_LOGGER = logging.getLogger(__name__)

class ThermoHub8Client:
    """
    Minimaler REST-Client für ThermoHub8.
    Erwartet JSON wie:
    {
      "sensors": [
        {"id": 1, "name": "Sensor 1", "value": 21.3, "unit": "°C"},
        ...
      ],
      "ts": "2025-09-15T12:34:56Z"
    }
    """

    def __init__(self, session: ClientSession, base_url: str, api_key: Optional[str] = None, verify_ssl: bool = True) -> None:
        _LOGGER.debug("Init ThermoHub8Client base_url=%s verify_ssl=%s", self._base_url, verify_ssl)
        self._session = session
        self._base_url = str(yarl.URL(base_url).with_scheme(yarl.URL(base_url).scheme or "http"))
        self._api_key = api_key
        self._ssl = verify_ssl

    async def async_get_readings(self) -> Dict[str, Any]:
        # Standard-Endpunkt – bei Bedarf anpassen
        url = str(yarl.URL(self._base_url) / "api" / "v1" / "readings")
        _LOGGER.debug("Fetching ThermoHub8 readings from %s", url)

        headers = {}
        if self._api_key:
            headers["Authorization"] = f"Bearer {self._api_key}"
        try:
            async with self._session.get(url, headers=headers, ssl=self._ssl, timeout=10) as resp:
                resp.raise_for_status()
                data = await resp.json()
                _LOGGER.debug("Received ThermoHub8 payload keys: %s", list(data.keys()))
                return data
        except ClientResponseError as e:
            _LOGGER.warning("ThermoHub8 API error (%s): %s", e.status, e.message)
            raise ConnectionError(f"ThermoHub8 API error: {e.status} {e.message}") from e
        except asyncio.TimeoutError:
            _LOGGER.error("ThermoHub8 API timeout after 10s")
            raise

    @staticmethod
    def normalize_payload(payload: Dict[str, Any]) -> List[Dict[str, Any]]:
        sensors = payload.get("sensors") or []
        normalized: List[Dict[str, Any]] = []
        for idx, item in enumerate(sensors, start=1):
            # Fallbacks
            sensor_id = item.get("id", idx)
            name = item.get("name") or f"Sensor {sensor_id}"
            value = item.get("value")
            unit = item.get("unit")
            normalized.append(
                {
                    "id": int(sensor_id),
                    "name": str(name),
                    "value": value,
                    "unit": unit,
                }
            )
        return normalized

