/**************************************************************************
 * 
 * Read pressure from MPL3115A2D chip with I2C and write it to a log file. 
 *       
 * Copyright (C) 2014 - 2015 Jaakko Koivuniemi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************************
 *
 * Wed Dec 10 21:17:05 CET 2014
 * Edit: Tue Jun  9 21:20:54 CEST 2015
 *
 * Jaakko Koivuniemi
 **/

#include "mpl3115a2d.h"
#include "I2cWriteRegister.h"
#include "I2cRReadRegBytes.h"
#include "ReadSQLiteTime.h"
#include "InsertSQLite.h"

const int version=20150609; // program version
int presint=300; // pressure measurement interval [s]
int altint=0; // altitude measurement interval [s]

double temperature=0; // temperature [C]
double pressure=0; // pressure [Pa]
double altitude=0; // altitude [m]
unsigned short refpres=0; // reference pressure [0-65535]
const char adatafile[200]="/var/lib/mpl3115a2d/altitude";
const char pdatafile[200]="/var/lib/mpl3115a2d/pressure";
const char tdatafile[200]="/var/lib/mpl3115a2d/temperature";

// local SQLite database file
int dbsqlite=0; // store data to local SQLite database
char dbfile[ SQLITEFILENAME_SIZE ];

const char *i2cdev = "/dev/i2c-1";

const char confile[200]="/etc/mpl3115a2d_config";

const char pidfile[200]="/var/run/mpl3115a2d.pid";

int loglev=5;
const char logfile[200]="/var/log/mpl3115a2d.log";
char message[200]="";

void read_config()
{
  FILE *cfile;
  char *line=NULL;
  char par[20];
  float value;
  size_t len;
  ssize_t read;

  cfile=fopen(confile, "r");
  if(NULL!=cfile)
  {
    syslog(LOG_INFO|LOG_DAEMON, "Read configuration file");
    while((read=getline(&line,&len,cfile))!=-1)
    {
       if(sscanf(line,"%s %f",par,&value)!=EOF) 
       {
          if(strncmp(par,"LOGLEVEL",8)==0)
          {
             loglev=(int)value;
             sprintf(message,"Log level set to %d",(int)value);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
             setlogmask(LOG_UPTO (loglev));
          }
          if(strncmp(par,"DBSQLITE",8)==0)
          {
            if(sscanf(line,"%s %s",par,dbfile)!=EOF)  
            {
              dbsqlite=1;
              sprintf(message, "Store data to database %s", dbfile);
              syslog(LOG_INFO|LOG_DAEMON, "%s", message);
            }
          }
          if(strncmp(par,"ALTINT",7)==0)
          {
             altint=(int)value;
             sprintf(message,"Altitude measurement interval set to %d s",(int)value);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
          }
          if(strncmp(par,"PRESINT",7)==0)
          {
             presint=(int)value;
             sprintf(message,"Pressure measurement interval set to %d s",(int)value);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
          }
          if(strncmp(par,"REFPRES",7)==0)
          {
             refpres=(unsigned short)value;
             sprintf(message,"Reference pressure set to %d",(unsigned short)value);
             syslog(LOG_INFO|LOG_DAEMON, "%s", message);
          }
       }
    }
    fclose(cfile);
  }
  else
  {
    sprintf(message, "Could not open %s", confile);
    syslog(LOG_ERR|LOG_DAEMON, "%s", message);
  }
}

int cont=1; /* main loop flag */

// write temperature value to file
void write_temp(double t)
{
  FILE *tfile;
  tfile=fopen(tdatafile, "w");
  if(NULL==tfile)
  {
    sprintf(message,"could not write to file: %s",tdatafile);
    syslog(LOG_ERR|LOG_DAEMON, "%s", message);
  }
  else
  { 
    fprintf(tfile,"%-+6.3f",t);
    fclose(tfile);
  }
}

// write pressure value to file
void write_pres(double p)
{
  FILE *pfile;
  pfile=fopen(pdatafile, "w");
  if(NULL==pfile)
  {
    sprintf(message,"could not write to file: %s",pdatafile);
    syslog(LOG_ERR|LOG_DAEMON, "%s", message);
  }
  else
  { 
    fprintf(pfile,"%-9.2f",p);
    fclose(pfile);
  }
}

// write altitude value to file
void write_alt(double a)
{
  FILE *afile;
  afile=fopen(adatafile, "w");
  if(NULL==afile)
  {
    sprintf(message,"could not write to file: %s",adatafile);
    syslog(LOG_ERR|LOG_DAEMON, "%s", message);
  }
  else
  { 
    fprintf(afile,"%-9.2f",a);
    fclose(afile);
  }
}


void stop(int sig)
{
  sprintf(message, "signal %d catched, stop", sig);
  syslog(LOG_NOTICE|LOG_DAEMON, "%s", message);
  cont = 0;
}

void terminate(int sig)
{
  sprintf(message, "signal %d catched", sig);
  syslog(LOG_NOTICE|LOG_DAEMON, "%s", message);

  sleep(1);
  syslog(LOG_NOTICE|LOG_DAEMON, "stop");

  cont = 0;
}

void hup(int sig)
{
  sprintf(message, "signal %d catched", sig);
  syslog(LOG_NOTICE|LOG_DAEMON, "%s", message);
}


int main()
{  
  int ok=0;

  setlogmask(LOG_UPTO (loglev));
  syslog(LOG_NOTICE|LOG_DAEMON, "mpl3115a2d v. %d started", version); 

  signal(SIGINT, &stop); 
  signal(SIGKILL, &stop); 
  signal(SIGTERM, &terminate); 
  signal(SIGQUIT, &stop); 
  signal(SIGHUP, &hup); 

  read_config();

  int unxs = (int)time(NULL); // unix seconds
  int nxtpres = unxs; // next time to read pressure and temperature
  int nxtalt = unxs; // next time to read altitude and temperature

  pid_t pid, sid;
        
  pid=fork();
  if(pid<0) 
  {
    exit(EXIT_FAILURE);
  }

  if(pid>0) 
  {
    exit(EXIT_SUCCESS);
  }

  umask(0);

  /* Create a new SID for the child process */
  sid = setsid();
  if( sid < 0 ) 
  {
    syslog(LOG_ERR|LOG_DAEMON, "failed to create child process"); 
    exit(EXIT_FAILURE);
  }
        
  if((chdir("/")) < 0) 
  {
    syslog(LOG_ERR|LOG_DAEMON, "failed to change to root directory"); 
    exit(EXIT_FAILURE);
  }
 
  /* Close out the standard file descriptors */
  close( STDIN_FILENO );
  close( STDOUT_FILENO );
  close( STDERR_FILENO );

  FILE *pidf;
  pidf = fopen(pidfile, "w");

  if( pidf == NULL )
  {
    sprintf(message,"Could not open PID lock file %s, exiting", pidfile);
    syslog(LOG_ERR|LOG_DAEMON, "%s", message);
    exit(EXIT_FAILURE);
  }

  if( flock(fileno(pidf), LOCK_EX||LOCK_NB) == -1 )
  {
    sprintf(message, "Could not lock PID lock file %s, exiting", pidfile);
    syslog(LOG_ERR|LOG_DAEMON, "%s", message);
    exit(EXIT_FAILURE);
  }

  fprintf(pidf, "%d\n", getpid());
  fclose(pidf);

  unsigned char msb,lsb;
  if( refpres > 0 )
  {
    msb = (unsigned char)(refpres>>8);
    lsb = (unsigned char)(refpres&0x00FF);
    ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x14, msb);
    ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x15, lsb);
  }

  short T = 0;
  int P = 0;
  unsigned int A = 0;
  unsigned char buffer[10];

  const char query[ SQLITEQUERY_SIZE ] = "insert into mpl3115a2d (name,temperature,pressure,altitude) values (?,?,?,?)";  
  double data[ SQLITE_DOUBLES ];

  while( cont == 1 )
  {
    unxs = (int)time(NULL); 

    if((( unxs >= nxtpres )||( (nxtpres-unxs) > presint ))&&( presint > 10 )) 
    {
      nxtpres = presint + unxs;
      ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x26, 0x38);
      ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x13, 0x07);
      ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x26, 0x39);

      if(cont==1)
      {
        usleep(500000);

        if( I2cRReadRegBytes(MPL3115A2D_ADDRESS, MPL3115A2D_STATUS, buffer, 6) == 1 )
        {
          T = ((short)buffer[4])<<8;
          T |= (short)buffer[5];
          temperature = ((double)T)/256.0; 
          P = ((int)buffer[1])<<16;
          P |= ((int)buffer[2])<<8;
          P |= ((int)buffer[3]);
          pressure = ((double)P)/64.0;

          sprintf(message, "T=%-+6.3f P=%-9.2f", temperature, pressure);
          syslog(LOG_INFO|LOG_DAEMON, "%s", message);

          write_temp(temperature);
          write_pres(pressure);

          data[0] = temperature;
          data[1] = pressure;
          data[2] = 0;

          if(dbsqlite==1) InsertSQLite(dbfile, query, "p1", 3, data);

          cont = 1;
        }
        else
        {
          syslog(LOG_ERR|LOG_DAEMON, "Temperature and pressure reading failed");
        }
      }
    }

    if(((unxs>=nxtalt)||((nxtalt-unxs)>altint))&&(altint>10)) 
    {
      nxtalt=altint+unxs;
      ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x26, 0xB8);
      ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x13, 0x07);
      ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x26, 0xB9);

      if( cont == 1 )
      {
        usleep(500000);

        if( I2cRReadRegBytes(MPL3115A2D_ADDRESS, MPL3115A2D_STATUS, buffer, 6) == 1 )
        {
          T = ((short)buffer[4])<<8;
          T |= (short)buffer[5];
          temperature = ((double)T)/256.0; 
          A = ((unsigned int)buffer[1])<<24;
          A |= ((unsigned int)buffer[2])<<16;
          A |= ((unsigned int)buffer[3])<<8;
          altitude = ((double)A)/65536.0;

          sprintf(message, "T=%-+6.3f A=%-9.2f", temperature, altitude);
          syslog(LOG_INFO|LOG_DAEMON, "%s", message);

          write_temp(temperature);
          write_alt(altitude);

          data[0] = temperature;
          data[1] = 0;
          data[2] = altitude;

          if(dbsqlite==1) InsertSQLite(dbfile, query, "a1", 3, data);

          cont = 1;
        }
        else
        {
          syslog(LOG_ERR|LOG_DAEMON, "Temperature and altitude reading failed");
        }
      }
    }
    sleep(1);
  }

  syslog(LOG_NOTICE|LOG_DAEMON, "remove PID file");
  ok = remove( pidfile );

  return ok;
}
