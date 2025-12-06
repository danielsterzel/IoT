idf_component_register(
    SRCS "anti_theft_wifi.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event nvs_flash
    PRIV_REQUIRES anti_theft_mqtt
)
