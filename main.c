#include <cjson/cJSON.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

typedef struct {
  uint8_t _time[20];
  uint8_t _valid[10];
  uint8_t _lat1[20];
  uint8_t _lat2[10];
  uint8_t _lon1[20];
  uint8_t _lon2[10];
  uint8_t _speed[10];
  uint8_t _orient[10];
  uint8_t _date[20];
} RMC_t;

typedef struct {
  RMC_t gpsData;
  float temp;
  float pressure;

} parsedToStruct;

int main() {
  int fd;
  char buffer[255];

  // union grabber

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
  while (1) {
    int bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0'; // Null terminate the string
      printf("Received: %s\n", buffer);
    }
  }

  cJSON *root = cJSON_Parse(buffer);

  // Close serial port
  close(fd);

  return 0;
}
