#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

#define DEVICE_PATH "/dev/pixxel"

int main()
{
    int fd;
    uint32_t status_val = 0;
    uint32_t command_on = 1;
    uint32_t command_off = 0;
    ssize_t bytes_moved;

    printf("--- Pixxel Driver User-Space Test Utility ---\n");

    /* Test Open System Call */
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0)
    {
        perror("[-] Failed to open " DEVICE_PATH ". Did you load the module and run as sudo?");
        return EXIT_FAILURE;
    }
    printf("[+] Successfully opened " DEVICE_PATH "\n");

    /* Test Write System Call: Turn Payload ON */
    printf("[*] Sending ON command (1) to driver...\n");
    bytes_moved = write(fd, &command_on, sizeof(command_on));
    if (bytes_moved < 0)
    {
        perror("[-] Write operation failed");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[+] Successfully wrote %zd bytes to driver\n", bytes_moved);

    /* Test Read System Call: Fetch Hardware Status Register */
    printf("[*] Reading Status Register from driver...\n");
    bytes_moved = read(fd, &status_val, sizeof(status_val));
    if (bytes_moved < 0)
    {
        perror("[-] Read operation failed");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[+] Successfully read %zd bytes. Status Register Value: 0x%X\n", bytes_moved, status_val);

    /* Test Write System Call: Turn Payload OFF */
    printf("[*] Sending OFF command (0) to driver...\n");
    bytes_moved = write(fd, &command_off, sizeof(command_off));
    if (bytes_moved < 0)
    {
        perror("[-] Write operation failed");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("[+] Successfully wrote %zd bytes to driver\n", bytes_moved);

    /* Test Close System Call */
    close(fd);
    printf("[+] Device closed cleanly. Test complete.\n");

    return EXIT_SUCCESS;
}