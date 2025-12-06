cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "anti_theft_mqtt_client.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event mqtt
)
