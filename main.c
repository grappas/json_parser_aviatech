#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <cjson/cJSON.h>

int main() {
    int fd;
    char buffer[255];
    struct termios options;

    // Open serial port
    fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("open");
        return -1;
    }

    // Set serial port options
    tcgetattr(fd, &options);
    cfsetispeed(&options, B9600); // Set baud rate to 9600
    cfsetospeed(&options, B9600);
    options.c_cflag |= (CLOCAL | CREAD);
    tcsetattr(fd, TCSANOW, &options);

    // Read data
  while(1)
    { int bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; // Null terminate the string
        printf("Received: %s\n", buffer);
    } }

    cJSON *root = cJSON_Parse(buffer);

    // Close serial port
    close(fd);

    return 0;
}
