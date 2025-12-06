cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi nvs_flash esp_event
    PRIV_REQUIRES anti_theft_mqtt wifi
)