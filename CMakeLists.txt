idf_component_register(
    SRC_DIRS src "src" "src/internal" 
    INCLUDE_DIRS include "include" "include/internal"
    REQUIRES nvs_flash esp_websocket_client esp_http_client json esp_event esp_http_client
)