import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

AUTO_LOAD = ["uart"]
DEPENDENCIES = ["network"]

CONF_UART_ID = "uart_id"
CONF_PORT = "port"
CONF_RESPONSE_TIMEOUT_MS = "response_timeout_ms"

modbus_tcp_rtu_gateway_ns = cg.esphome_ns.namespace("modbus_tcp_rtu_gateway")

ModbusTcpRtuGateway = modbus_tcp_rtu_gateway_ns.class_(
    "ModbusTcpRtuGateway", cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ModbusTcpRtuGateway),
        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_PORT, default=502): cv.port,
        cv.Optional(CONF_RESPONSE_TIMEOUT_MS, default=500): cv.int_range(min=50, max=5000),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await uart.register_uart_device(var, config)

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_response_timeout_ms(config[CONF_RESPONSE_TIMEOUT_MS]))