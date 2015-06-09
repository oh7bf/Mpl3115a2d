#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <sqlite3.h>
#include <errno.h>
#include <limits.h>


#ifndef MPL3115A2D_H_INCLUDED
#define MPL3115A2D_H_INCLUDED

#define I2LOCK_MAX 10
#define MPL3115A2D_ADDRESS 0x60
#define MPL3115A2D_STATUS 0x00
#define MPL3115A2D_OUT_P_MSB 0x01
#define MPL3115A2D_OUT_P_CSB 0x02
#define MPL3115A2D_OUT_P_LSB 0x03
#define MPL3115A2D_OUT_T_MSB 0x04
#define MPL3115A2D_OUT_T_LSB 0x05
#define MPL3115A2D_DR_STATUS 0x06

#define SENSORNAME_SIZE 20
#define SQLITEFILENAME_SIZE 200
#define SQLITEQUERY_SIZE 200
#define SQLITE_DOUBLES 10 

extern const char *i2cdev;// i2c device
extern int loglev; // log level
extern int cont; // main loop flag

#endif

