idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/ulp/temp_app"
                    REQUIRES soc ulp)

ulp_add_project("ulp_temp_app" "${CMAKE_SOURCE_DIR}/ulp/temp_app/")