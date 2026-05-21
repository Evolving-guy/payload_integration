savedcmd_pixxel_platform_driver.mod := printf '%s\n'   pixxel_platform_driver.o | awk '!x[$$0]++ { print("./"$$0) }' > pixxel_platform_driver.mod
