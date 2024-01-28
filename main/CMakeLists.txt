idf_component_register(SRCS "displaymanager.cpp" "webserver.cpp" "sntp.c" "clock.cpp" "display.cpp" "calibrate.cpp" "stepper.cpp" "multistepper.cpp" "flapmdns.c" "main.cpp" "wifi.c"
                    INCLUDE_DIRS ".")

#spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)