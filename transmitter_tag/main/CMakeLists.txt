# Add all your .c files to the SRCS list
idf_component_register(SRCS "ble_transmission.c"
			    "main.c"
			    "i2s_sampling.c"
			    "aoa_ble.c"
                       INCLUDE_DIRS "." "$ENV{IDF_PATH}/examples/bluetooth/nimble/ble_cte/common_components/cte_config"
		       REQUIRES bt nvs_flash esp_driver_i2s fatfs)
