# Add all your .c files to the SRCS list
idf_component_register(SRCS "ble_transmission.c"
			    "main.c"
			    "i2s_sampling.c"
                       INCLUDE_DIRS ".")
