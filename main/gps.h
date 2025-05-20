

#ifndef NEO6M_H_
#define NEO6M_H_

#define GPS_UART_PORT      UART_NUM_0
#define BUF_SIZE          1024
#define GPS_RXD_PIN        20
#define GPS_TXD_PIN        18
#define BUF_SIZE          1024
#define RD_BUF_SIZE       (BUF_SIZE)

void parse_gpgga_sentence(const char *nmea_sentence);
void gps_task(void *arg);

#endif 