set(srcs "src/WiFi_utils.c")
set(include "include")
set(priv_requires nvs_flash esp_wifi esp_http_client esp_https_server)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include}
    PRIV_REQUIRES "${priv_requires}"
)