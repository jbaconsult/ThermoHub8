from __future__ import annotations

from typing import Any, Dict, List, Optional

from homeassistant.components.sensor import SensorEntity, SensorDeviceClass, SensorStateClass
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from homeassistant.config_entries import ConfigEntry
from .const import DOMAIN, MAX_SENSORS, ATTR_LAST_UPDATE
from .coordinator import ThermoHub8Coordinator
from .api import ThermoHub8Client

import logging
_LOGGER = logging.getLogger(__name__)

async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry, async_add_entities: AddEntitiesCallback) -> None:
    data = hass.data[DOMAIN][entry.entry_id]
    coordinator: ThermoHub8Coordinator = data["coordinator"]

    # Erzeuge Entitäten dynamisch basierend auf der ersten Aktualisierung
    payload = coordinator.data or {}
    normalized = ThermoHub8Client.normalize_payload(payload)

    _LOGGER.info("ThermoHub8 creating up to %d sensor entities", MAX_SENSORS)
    _LOGGER.debug("Initial normalized sensors: %s", [s.get("name") for s in normalized])

    entities: List[ThermoHub8Sensor] = []
    for idx, item in enumerate(normalized[:MAX_SENSORS]):
        entities.append(
            ThermoHub8Sensor(
                coordinator=coordinator,
                entry_id=entry.entry_id,
                sensor_id=item["id"],
                name=item["name"],
                unit=item.get("unit"),
            )
        )
    # Falls noch weniger als 8 geliefert werden, legen wir Platzhalter gemäß Index an
    for i in range(len(entities) + 1, MAX_SENSORS + 1):
        entities.append(
            ThermoHub8Sensor(
                coordinator=coordinator,
                entry_id=entry.entry_id,
                sensor_id=i,
                name=f"Sensor {i}",
                unit=None,
                optional=True,
            )
        )

    async_add_entities(entities)
    _LOGGER.info("ThermoHub8 added %d sensor entities", len(entities))

class ThermoHub8Sensor(CoordinatorEntity[ThermoHub8Coordinator], SensorEntity):
    _attr_has_entity_name = True

    def __init__(
        self,
        coordinator: ThermoHub8Coordinator,
        entry_id: str,
        sensor_id: int,
        name: str,
        unit: Optional[str],
        optional: bool = False,
    ) -> None:
        super().__init__(coordinator)
        self._entry_id = entry_id
        self._sensor_id = sensor_id
        self._attr_name = name
        self._unit = unit
        self._optional = optional

        _LOGGER.debug("ThermoHub8Sensor created: id=%s name=%s unit=%s optional=%s",
                      sensor_id, name, unit, optional)
        
        self._attr_unique_id = f"{DOMAIN}_{entry_id}_sensor_{sensor_id}"
        self._attr_device_info = {
            "identifiers": {(DOMAIN, entry_id)},
            "name": "ThermoHub8",
            "manufacturer": "ThermoHub",
            "model": "ThermoHub8",
        }
        # Optional: eine sinnvolle Klasse, falls Temperatur
        if unit and unit in ("°C", "°F", "K"):
            self._attr_device_class = SensorDeviceClass.TEMPERATURE
            self._attr_state_class = SensorStateClass.MEASUREMENT

    @property
    def native_unit_of_measurement(self) -> str | None:
        return self._unit

    @property
    def available(self) -> bool:
        # verfügbar wenn Coordinator OK und dieser Sensor im Payload auftaucht
        if not super().available:
            return False
        normalized = ThermoHub8Client.normalize_payload(self.coordinator.data or {})
        ok = any(item.get("id") == self._sensor_id for item in normalized) or self._optional
        _LOGGER.debug("Sensor %s available=%s", self._sensor_id, ok)
        return ok
        
    @property
    def extra_state_attributes(self) -> Dict[str, Any] | None:
        ts = (self.coordinator.data or {}).get("ts")
        return {ATTR_LAST_UPDATE: ts} if ts else None

    @property
    def native_value(self) -> Any:
        normalized = ThermoHub8Client.normalize_payload(self.coordinator.data or {})
        for item in normalized:
            if item.get("id") == self._sensor_id:
                value = item.get("value")
                _LOGGER.debug("Sensor %s (%s) value=%s", self._sensor_id, self.name, value)
                return value
        # wenn optionaler Sensor, aber (noch) nicht vorhanden
        return None

