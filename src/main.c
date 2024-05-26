#include <fcntl.h>
#include <gpiod.h>
#include <stdbool.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

double height_cache = 0;

time_t start, end;

typedef struct {
  char time[11];        // HHMMSS.SS
  char latitude[10];    // DDMM.MMMM
  char lat_dir;         // N or S
  char longitude[11];   // DDDMM.MMMM
  char lon_dir;         // E or W
  char timestamp[7];    // DDMMYY
  float speed;          // Speed in knots
  float speed_vertical; // Speed in knots
  float course;         // Course over ground
  double altitude;
} GPSData_t;

void initializeGPSData(GPSData_t *data) {
  strncpy(data->time, "null", sizeof(data->time));
  strncpy(data->latitude, "null", sizeof(data->latitude));
  data->lat_dir = ' ';
  strncpy(data->longitude, "null", sizeof(data->longitude));
  data->lon_dir = ' ';
  strncpy(data->timestamp, "null", sizeof(data->timestamp));
  data->speed = 0.0f;
  data->course = 0.0f;
}

void clearString(char *str, size_t size) {
  for (size_t i = 0; i < size; i++) {
    str[i] = '\0';
  }
}

bool parseSentence(const char *sentence, GPSData_t *gpsData) {
  char type[7];
  char trash[256];
  sscanf(sentence, "%6s", type);
  if (strcmp(type, "$GPRMC") == 0) {
    sscanf(sentence,
           // $GPRMC, 123519, A, 4807$	Every NMEA sentence starts with $
           // character. GPRMC	Global Positioning Recommended Minimum
           // Coordinates 123519	Current time in UTC – 12:35:19 A
           // Status A=active or V=Void. 4807.038,N	Latitude 48 deg 07.038′
           // N 01131.000,E	Longitude 11 deg 31.000′ E 022.4	Speed
           // over the ground in knots 084.4	Track angle in degrees True
           // 220318	Current Date – 22rd of March 2018
           // 003.1,W	Magnetic Variation
           // *6A	The checksum data, always begins with *.038, N,
           // 01131.000, E,022.4, 084.4, 230394, 003.1, W*6A
           "$GPRMC,%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%f,%[^,],%[^,],"
           "%[^*],%[^*]*%*s",
           gpsData->time, trash, gpsData->latitude, &gpsData->lat_dir,
           gpsData->longitude, &gpsData->lon_dir, &gpsData->speed, trash,
           gpsData->timestamp, trash, trash);
  } else if (strcmp(type, "$GPGGA") == 0) {
    sscanf(
        sentence,
        // $GPGGA, 123519, 4807.038, N, 01131.000, E, 1, 08, 0.9, 545.4,
        // M, 46.9, M, , *47 123519	Current time in UTC – 12:35:19 4807.038,
        // N	Latitude 48 deg 07.038′ N
        // 01131.000,
        // E	Longitude 11 deg 31.000′ E
        // 1	GPS fix
        // 08	Number of satellites being tracked
        // 0.9	Horizontal dilution of position
        // 545.4,
        // M	Altitude in Meters (above mean sea level)
        // 46.9,
        // M	Height of geoid (mean sea level)
        // (empty field)	Time in seconds since last DGPS update
        // (empty field)	DGPS station ID number
        // *47	The checksum data, always begins with *
        "$GPGGA,%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%lf*%*s",
        gpsData->time, gpsData->latitude, &gpsData->lat_dir, gpsData->longitude,
        &gpsData->lon_dir, gpsData->timestamp, trash, trash, trash,
        &gpsData->altitude);
  } else {
    return true;
  }
  return false;
}

void GPSDataToJson(const GPSData_t *data, char *json_output,
                   size_t json_output_size) {
  int state = 0;
  snprintf(json_output, json_output_size,
           "{ \"time\": \"%s\", \"latitude\": \"%s\", \"lat_dir\": \"%c\", "
           "\"longitude\": \"%s\", \"lon_dir\": \"%c\", \"date\": \"%s\", "
           "\"speed\": %.1f, \"speed_vertical\": %.1f, \"course\": %.1f, "
           "\"height\": %.1f, \"thermal_state\": %d }",
           data->time, data->latitude, data->lat_dir, data->longitude,
           data->lon_dir, data->timestamp, data->speed, data->speed_vertical,
           data->course, data->altitude, state);
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
  cfsetospeed(&options, B9600);
  options.c_cflag |= (CLOCAL | CREAD);
  tcsetattr(fd, TCSANOW, &options);

  char jsonFile[64096];
  char buffer[256];
  GPSData_t GPSData;

  // Read data
  while (1) {
    clearString(buffer, sizeof(buffer));
    clearString(jsonFile, sizeof(jsonFile));
    int bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
      buffer[sizeof(buffer) - 1] = '\0'; // Null terminate the string
      printf("Received: %s\n", buffer);
      initializeGPSData(&GPSData);

      if (parseSentence(buffer, &GPSData) == true) {
        continue;
      }
      GPSDataToJson(&GPSData, jsonFile, sizeof(jsonFile));

      printf("%s\n", jsonFile);

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

  // Close serial port
  close(fd);

  return 0;
}
