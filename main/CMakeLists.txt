file(GLOB_RECURSE srcs "main.c" "src/*.c")


idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "./include"
                    REQUIRES driver esp_timer esp_wifi esp_netif nvs_flash esp_event lwip esp_http_client mqtt json bt esp-tls)
