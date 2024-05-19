#include <cjson/cJSON.h>
#include <fcntl.h>
#include <pigpio.h>
#include <stdbool.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#define GPIO_PIN 23

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
  char timestamp[7];         // DDMMYY
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

// typedef struct {
//   GPSData_t gpsData;
//   float temp;
//   float pressure;
//   float orientation;
//   bool thermal_true;
//   float ascending_speed;
// } parsedToStruct_t;

// typedef union {
//
//   char toPass[sizeof(parsedToStruct_t) + 1];
//   parsedToStruct_t parsedToStruct;
//
// } togetherApesStrong_u;

void parseGPRMC(const char *line, GPSData_t *data) {
  char temp_speed[10];
  char temp_course[10];

  // Initialize temporary storage to empty strings
  memset(temp_speed, 0, sizeof(temp_speed));
  memset(temp_course, 0, sizeof(temp_course));

  // Parse the line
  sscanf(line, "$GPRMC,%10[^,],%*c,%9[^,],%c,%10[^,],%c,%9[^,],%9[^,],%6[^,]",
         data->time, data->latitude, &data->lat_dir, data->longitude,
         &data->lon_dir, temp_speed, temp_course, data->timestamp);

  // Convert speed and course only if valid values were read
  if (strlen(temp_speed) > 0) {
    data->speed = atof(temp_speed);
  } else {
    data->speed = 0.0f;
  }
  if (strlen(temp_course) > 0) {
    data->course = atof(temp_course);
  } else {
    data->course = 0.0f;
  }

  // Handle empty fields by setting them to "null"
  if (strlen(data->time) == 0) {
    strncpy(data->time, "null", sizeof(data->time));
  }
  if (strlen(data->latitude) == 0) {
    strncpy(data->latitude, "null", sizeof(data->latitude));
  }
  if (strlen(data->longitude) == 0) {
    strncpy(data->longitude, "null", sizeof(data->longitude));
  }
  if (strlen(data->timestamp) == 0) {
    strncpy(data->timestamp, "null", sizeof(data->timestamp));
  }
}

void parseLine(const char *line, GPSData_t *data) {
  if (strncmp(line, "$GPRMC", 6) == 0) {
    parseGPRMC(line, data);
  }
  // Add more parsers for other NMEA sentence types if needed
}

void clearString(char *str, size_t size) {
  for (size_t i = 0; i < size; i++) {
    str[i] = '\0';
  }
}

void parseCSVToGPSData(const char *csv_string, GPSData_t *data) {
  char buffer[256];
  strncpy(buffer, csv_string, sizeof(buffer));
  buffer[sizeof(buffer) - 1] = '\0';

  char *token;
  int field_count = 0;

  token = strtok(buffer, ",");
  while (token != NULL) {
    switch (field_count) {
    case 0: // Time
      strncpy(data->time, token, sizeof(data->time));
      break;
    case 1: // Latitude
      strncpy(data->latitude, token, sizeof(data->latitude));
      break;
    case 2: // N/S Indicator
      data->lat_dir = token[0];
      break;
    case 3: // Longitude
      strncpy(data->longitude, token, sizeof(data->longitude));
      break;
    case 4: // E/W Indicator
      data->lon_dir = token[0];
      break;
    case 5: // Date
      strncpy(data->timestamp, token, sizeof(data->timestamp));
      break;
    case 6: // Speed
      data->speed = atof(token);
      break;
    case 7: // Course
      data->course = atof(token);
      break;
    case 8: // Height
      data->altitude = atof(token);
      break;
    }

    token = strtok(NULL, ",");
    field_count++;
  }
}

bool parseSentence(const char *sentence, GPSData_t *gpsData) {
  char type[7];
  char trash[256];
  sscanf(sentence, "%6s", type);
  if (strcmp(type, "$GPRMC") == 0) {
    sscanf(sentence,
           // $GPRMC, 123519, A, 4807$	Every NMEA sentence starts with $ character.
           // GPRMC	Global Positioning Recommended Minimum Coordinates
           // 123519	Current time in UTC – 12:35:19
           // A	Status A=active or V=Void.
           // 4807.038,N	Latitude 48 deg 07.038′ N
           // 01131.000,E	Longitude 11 deg 31.000′ E
           // 022.4	Speed over the ground in knots
           // 084.4	Track angle in degrees True
           // 220318	Current Date – 22rd of March 2018
           // 003.1,W	Magnetic Variation
           // *6A	The checksum data, always begins with *.038, N, 01131.000, E,022.4, 084.4, 230394, 003.1, W*6A
           "$GPRMC,%[^,],%[^,],%[^,],%[^,],%[^,],%[^,],%f,%[^,],%[^,],"
           "%[^*],%[^*]*%*s",
           gpsData->time, trash, gpsData->latitude, &gpsData->lat_dir,
           gpsData->longitude, &gpsData->lon_dir, &gpsData->speed,
           trash, gpsData->timestamp,trash,trash);
  } else if (strcmp(type, "$GPGGA") == 0) {
    sscanf(
        sentence,
      // $GPGGA, 123519, 4807.038, N, 01131.000, E, 1, 08, 0.9, 545.4, M, 46.9, M, , *47
      // 123519	Current time in UTC – 12:35:19
      // 4807.038,
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
        &gpsData->lon_dir, gpsData->timestamp, trash, trash, trash, &gpsData->altitude
        );
  }
  else{
    return true;
  }
  return false;
}

// void GPSDataToJson(const GPSData_t *data, char *json_output,
//                    size_t json_output_size) {
//   gpioInitialise();
//   gpioSetMode(GPIO_PIN, PI_INPUT);
//   int state = gpioRead(GPIO_PIN);
//   snprintf(json_output, json_output_size,
//            "{ \"time\": \"%s\", \"latitude\": \"%s\", \"lat_dir\": \"%c\", "
//            "\"longitude\": \"%s\", \"lon_dir\": \"%c\", \"date\": \"%s\", "
//            "\"speed\": %.1f, \"course\": %.1f, \"thermal_state\": %d }",
//            data->time, data->latitude, data->lat_dir, data->longitude,
//            data->lon_dir, data->date, data->speed, data->course, state);
// }

void GPSDataToJson(const GPSData_t *data, char *json_output,
                   size_t json_output_size) {
  gpioInitialise();
  gpioSetMode(GPIO_PIN, PI_INPUT);
  int state = gpioRead(GPIO_PIN);
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
  int fd;

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

  // parseNMEA(buffer, &dataCache.);

  // printf("%s", print_json(&dataCache));

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

      // if (dataCache.parsedToStruct.gpsData.time[0])
      //   dataCache.parsedToStruct.gpsData.time[0] = '0';
      // if (dataCache.parsedToStruct.gpsData.latitude[0])
      //   dataCache.parsedToStruct.gpsData.latitude[0] = '0';
      // if (dataCache.parsedToStruct.gpsData.lat_dir)
      //   dataCache.parsedToStruct.gpsData.lat_dir = '0';
      // if (dataCache.parsedToStruct.gpsData.longitude[0])
      //   dataCache.parsedToStruct.gpsData.longitude[0] = '0';
      // if (dataCache.parsedToStruct.gpsData.lon_dir)
      //   dataCache.parsedToStruct.gpsData.lon_dir = '0';
      // if (dataCache.parsedToStruct.gpsData.date[0])
      //   dataCache.parsedToStruct.gpsData.date[0] = '0';

      // parseCSVToStruct(buffer, &dataCache.parsedToStruct);
      if(parseSentence(buffer, &GPSData) == true){
        continue;
      }
      // parsedToJson(&dataCache.parsedToStruct, jsonFile, sizeof(jsonFile));
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
