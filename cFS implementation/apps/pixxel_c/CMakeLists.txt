# Declare the app using the correct cFS macro name
add_cfe_app(pixxel_c fsw/src/pixxel_c.c)

# Add the include directory for headers
target_include_directories(pixxel_c PUBLIC fsw/src)
