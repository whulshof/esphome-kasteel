"""Flash an ESP32-C6 (or another Espressif chip) from this ESP32 over a plain
UART, with Espressif's esp-serial-flasher. No EN/BOOT wires: the target's own
firmware reboots into the ROM download mode when the download line is held
low (devices/zigbee_coordinator_zboss_uart.yaml), and afterwards the target is
reset with its LP watchdog. The image comes from an http(s) URL.
"""
from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import esp32, text_sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_URL

DEPENDENCIES = ["uart", "network", "esp32"]
AUTO_LOAD = ["text_sensor"]

CONF_DOWNLOAD_PIN = "download_pin"
CONF_STREAM_SERVER_ID = "stream_server_id"
CONF_STATUS = "status"

c6_flasher_ns = cg.esphome_ns.namespace("c6_flasher")
C6Flasher = c6_flasher_ns.class_("C6Flasher", cg.Component, uart.UARTDevice)
UpdateAction = c6_flasher_ns.class_("UpdateAction", automation.Action)
DownloadModeAction = c6_flasher_ns.class_("DownloadModeAction", automation.Action)
StreamServerComponent = cg.global_ns.class_("StreamServerComponent", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.only_on_esp32,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(C6Flasher),
            cv.Required(CONF_URL): cv.url,
            cv.Required(CONF_DOWNLOAD_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_STREAM_SERVER_ID): cv.use_id(StreamServerComponent),
            cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(
                icon="mdi:chip", entity_category="diagnostic"
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_url(config[CONF_URL]))
    cg.add(var.set_download_pin(await cg.gpio_pin_expression(config[CONF_DOWNLOAD_PIN])))
    if CONF_STREAM_SERVER_ID in config:
        cg.add_define("USE_C6_FLASHER_STREAM_SERVER")
        cg.add(var.set_stream_server(await cg.get_variable(config[CONF_STREAM_SERVER_ID])))
    if CONF_STATUS in config:
        cg.add(var.set_status_sensor(await text_sensor.new_text_sensor(config[CONF_STATUS])))
    esp32.add_idf_component(name="espressif/esp-serial-flasher", ref="2.1.0")
    esp32.request_http_client()
    esp32.require_certificate_bundle()


ACTION_SCHEMA = automation.maybe_simple_id({cv.GenerateID(): cv.use_id(C6Flasher)})


@automation.register_action("c6_flasher.update", UpdateAction, ACTION_SCHEMA, synchronous=True)
@automation.register_action(
    "c6_flasher.download_mode", DownloadModeAction, ACTION_SCHEMA, synchronous=True
)
async def c6_flasher_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
