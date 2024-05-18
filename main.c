#include <cjson/cJSON.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char time[11];      // HHMMSS.SS
  char latitude[10];  // DDMM.MMMM
  char lat_dir;       // N or S
  char longitude[11]; // DDDMM.MMMM
  char lon_dir;       // E or W
  char date[7];       // DDMMYY
  float speed;        // Speed in knots
  float course;       // Course over ground
} GPSData_t;

typedef struct {
  GPSData_t gpsData;
  float temp;
  float pressure;
  float orientation;
  bool thermal_true;
  float ascending_speed;
} parsedToStruct_t;

typedef union {

  char toPass[sizeof(parsedToStruct_t) + 1];
  parsedToStruct_t parsedToStruct;

} togetherApesStrong_u;

void parseNMEA(char *nmea_sentence, GPSData_t *gps_data) {
  // Example NMEA Sentence:
  // "$GPRMC,hhmmss.sss,A,ddmm.mmmm,N,dddmm.mmmm,E,x.x,x.x,ddmmyy,,"
  if (strncmp(nmea_sentence, "$GPRMC", 6) == 0) {
    char *token;
    int field_count = 0;

    token = strtok(nmea_sentence, ",");

    while (token != NULL) {
      switch (field_count) {
      case 1: // Time
        strncpy(gps_data->time, token, sizeof(gps_data->time));
        break;
      case 3: // Latitude
        strncpy(gps_data->latitude, token, sizeof(gps_data->latitude));
        break;
      case 4: // N/S Indicator
        gps_data->lat_dir = token[0];
        break;
      case 5: // Longitude
        strncpy(gps_data->longitude, token, sizeof(gps_data->longitude));
        break;
      case 6: // E/W Indicator
        gps_data->lon_dir = token[0];
        break;
      case 7: // Speed
        gps_data->speed = atof(token);
        break;
      case 8: // Course
        gps_data->course = atof(token);
        break;
      case 9: // Date
        strncpy(gps_data->date, token, sizeof(gps_data->date));
        break;
      }

      token = strtok(NULL, ",");
      field_count++;
    }
  }
}

int main() {
  int fd;

  // union grabber

  struct termios options;

  // Open serial port
  fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) {
    perror("open");
    return -1;
  }

  togetherApesStrong_u dataCache;

  // Set serial port options
  tcgetattr(fd, &options);
  cfsetispeed(&options, B9600); // Set baud rate to 9600
  cfsetospeed(&options, B9600);
  options.c_cflag |= (CLOCAL | CREAD);
  tcsetattr(fd, TCSANOW, &options);

  // parseNMEA(buffer, &dataCache.);

  // printf("%s", print_json(&dataCache));

  char jsonFile[64096];

  // Read data
  while (1) {
    int bytes_read = read(fd, dataCache.toPass, sizeof(parsedToStruct_t));
    if (bytes_read > 0) {
      dataCache.toPass[sizeof(parsedToStruct_t)] = '\0'; // Null terminate the string
      printf("Received: %s\n", dataCache.toPass);

      if (dataCache.parsedToStruct.gpsData.time[0])
        dataCache.parsedToStruct.gpsData.time[0] = '0';
      if (dataCache.parsedToStruct.gpsData.latitude[0])
        dataCache.parsedToStruct.gpsData.latitude[0] = '0';
      if (dataCache.parsedToStruct.gpsData.lat_dir)
        dataCache.parsedToStruct.gpsData.lat_dir = '0';
      if (dataCache.parsedToStruct.gpsData.longitude[0])
        dataCache.parsedToStruct.gpsData.longitude[0] = '0';
      if (dataCache.parsedToStruct.gpsData.lon_dir)
        dataCache.parsedToStruct.gpsData.lon_dir = '0';
      if (dataCache.parsedToStruct.gpsData.date[0])
        dataCache.parsedToStruct.gpsData.date[0] = '0';

      char jsonFile[64096];

      sprintf(jsonFile,
              "{\n"
              " \"time\": \"%s\",\n"
              " \"latitude\": \"%s\",\n"
              " \"lat_dir\": \"%c\",\n"
              " \"longtitude\": \"%s\",\n"
              " \"lon_dir\": \"%c\",\n"
              " \"date\": \"%s\",\n"
              " \"speed\": %f,\n"
              " \"course\": %f,\n"
              " \"temp\": %f,\n"
              " \"pressure\": %f,\n"
              " \"orientation\": %f,\n"
              " \"thermal_true\": \"%d\",\n"
              " \"ascending_speed\": %f,\n"
              "}",
              dataCache.parsedToStruct.gpsData.time,
              dataCache.parsedToStruct.gpsData.latitude,
              dataCache.parsedToStruct.gpsData.lat_dir,
              dataCache.parsedToStruct.gpsData.longitude,
              dataCache.parsedToStruct.gpsData.lon_dir,
              dataCache.parsedToStruct.gpsData.date,
              dataCache.parsedToStruct.gpsData.speed,
              dataCache.parsedToStruct.gpsData.course,
              dataCache.parsedToStruct.temp, dataCache.parsedToStruct.pressure,
              dataCache.parsedToStruct.orientation,
              (int)(dataCache.parsedToStruct.thermal_true),
              dataCache.parsedToStruct.ascending_speed);

      printf("%s\n", jsonFile);

      // char time[11];      // HHMMSS.SS
      // char latitude[10];  // DDMM.MMMM
      // char lat_dir;       // N or S
      // char longitude[11]; // DDDMM.MMMM
      // char lon_dir;       // E or W
      // char date[7];       // DDMMYY
      // float speed;        // Speed in knots
      // float course;       // Course over ground

      // parseNMEA(buffer, &);
      // printf("Time: %s\n", gps_data.time);
      // printf("Latitude: %s %c\n", gps_data.latitude, gps_data.lat_dir);
      // printf("Longitude: %s %c\n", gps_data.longitude, gps_data.lon_dir);
      // printf("Speed: %.1f knots\n", gps_data.speed);
      // printf("Course: %.1f\n", gps_data.course);
      // printf("Date: %s\n", gps_data.date);
    }
  }

  // Close serial port
  close(fd);

  return 0;
}
