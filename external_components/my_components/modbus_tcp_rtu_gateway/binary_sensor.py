import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import ModbusTcpRtuGateway

CONF_GATEWAY_ID = "gateway_id"
CONF_HEALTHY = "healthy"
CONF_CLIENT_CONNECTED = "client_connected"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GATEWAY_ID): cv.use_id(ModbusTcpRtuGateway),
        cv.Optional(CONF_HEALTHY): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_CLIENT_CONNECTED): binary_sensor.binary_sensor_schema(),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GATEWAY_ID])

    if CONF_HEALTHY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HEALTHY])
        cg.add(parent.set_healthy_sensor(sens))

    if CONF_CLIENT_CONNECTED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_CLIENT_CONNECTED])
        cg.add(parent.set_client_connected_sensor(sens))