add_cfe_app(pixxel_main fsw/src/pixxel_main.c)

# Add the include directory for headers
target_include_directories(pixxel_main PUBLIC fsw/src)
