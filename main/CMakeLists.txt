idf_component_register(
    SRCS "main.c" "mpu6500.c" "ws2812.c"
         "core/boids.c" "core/effect.c" "core/fluid.c" "core/motion.c"
         "core/mpu6500_decode.c" "core/panel.c" "core/tap.c"
    INCLUDE_DIRS "." "core"
    REQUIRES esp_driver_spi esp_driver_i2c esp_driver_gpio esp_timer log freertos
)
# The physics dominates frame time; optimise the core for speed.
set_source_files_properties(
    core/boids.c core/fluid.c core/motion.c
    PROPERTIES COMPILE_OPTIONS "-O2")
