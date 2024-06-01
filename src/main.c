#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gpiod.h"
#include "../extern/nmea_parser/nmea_parser.h"

double height_cache = 0;

time_t start, end;

void GPSDataToJson(const navData_t *data, char *json_output,
                   size_t json_output_size) {
  int state = 0;
  float speed_vertical = 0;
  snprintf(json_output, json_output_size,
           "{ \"time\": \"%d\", \"latitude\": \"%f\", \"lat_dir\": \"%c\", "
           "\"longitude\": \"%f\", \"lon_dir\": \"%c\", \"date\": \"%u\", "
           "\"speed\": %.1f, \"speed_vertical\": %.1f, \"course\": %.1f, "
           "\"height\": %.1f, \"thermal_state\": %d }",
           (unsigned int)data->rmc.time, data->rmc.lat, data->rmc.lat_dir, data->rmc.lon,
           data->rmc.lon_dir, data->rmc.date, data->rmc.speed, speed_vertical,
           data->rmc.course, data->gga.alt, state);
}

void clearString(char *str, size_t size) {
  for (int i = 0; i < size; i++) {
    str[i] = 0;
  }
}

int main() {
  // union grabber

  const char *chipname = "gpiochip0"; // GPIO chip name
  unsigned int line_num = 23;         // GPIO line number
  struct gpiod_chip *chip;
  struct gpiod_line *line;
  int value;

  // Open the GPIO chip
  chip = gpiod_chip_open_by_name(chipname);
  if (!chip) {
    perror("Open chip failed");
    exit(EXIT_FAILURE);
  }

  // Get the GPIO line
  line = gpiod_chip_get_line(chip, line_num);
  if (!line) {
    perror("Get line failed");
    gpiod_chip_close(chip);
    exit(EXIT_FAILURE);
  }

  struct termios options;
  int fd;
  // Open serial port
  fd = open("/dev/ttyAMA0", O_RDONLY | O_NOCTTY);
  if (fd == -1) {
    perror("Unable to open /dev/ttyAMA0");
    perror("Trying to open /dev/ttyS0");
    fd = open("/dev/ttyS0", O_RDONLY | O_NOCTTY);

    if (fd == -1) {
      perror("Unable to open /dev/ttyS0");
      exit(EXIT_FAILURE);
    }
  }

  // Set serial port options
  tcgetattr(fd, &options);
  cfsetispeed(&options, B9600); // Set baud rate to 9600
  options.c_cflag |= (CLOCAL | CREAD);
  tcsetattr(fd, TCSANOW, &options);

  navData_t data;
  nmeaBuffer_t buffer;
  nmea_init(&data, "GP", "RMC");

  char jsonFile[64096];

  // Read data
  while (1) {
    clearString(buffer.str, sizeof(buffer.str));
    clearString(jsonFile, sizeof(jsonFile));
    int bytes_read = read(fd, buffer.str, sizeof(buffer.str));
    if (bytes_read > 0) {
      if (buffer.str[0] == '\n')
        continue;
      buffer.str[sizeof(buffer.str) - 1] = '\0'; // Null terminate the string
      printf("%s\n", buffer.str);
      nmea_parse(&buffer, &data);
      printf("Cycle: %d\n", data.cycle);

      if (data.cycle == 6) {
        GPSDataToJson(&data, jsonFile, sizeof(jsonFile));
        printf("%s\n", jsonFile);
        // print_gsv(&data.gsv);
        FILE *file = fopen("data.json", "w");
        if (file == NULL) {
          perror("Error opening file");
          return 1;
        }
        fprintf(file, "%s\n", jsonFile);
        fclose(file);

        system("cat data.json > "
               "../aviatech-hackathon-2/src/renderer/src/assets/data.json");
      }
    }
  }

  // Close serial port
  close(fd);

  return 0;
}
