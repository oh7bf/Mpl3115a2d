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
 * Edit: Wed May 27 22:13:27 CEST 2015
 *
 * Jaakko Koivuniemi
 **/

#include "mpl3115a2d.h"
#include "I2cWriteRegister.h"

const int version=20150527; // program version
int presint=300; // pressure measurement interval [s]
int altint=0; // altitude measurement interval [s]

double temperature=0; // temperature [C]
double pressure=0; // pressure [Pa]
double altitude=0; // altitude [m]
unsigned short refpres=0; // reference pressure [0-65535]
const char adatafile[200]="/var/lib/mpl3115a2d/altitude";
const char pdatafile[200]="/var/lib/mpl3115a2d/pressure";
const char tdatafile[200]="/var/lib/mpl3115a2d/temperature";

const char *i2cdev = "/dev/i2c-1";

const char confile[200]="/etc/mpl3115a2d_config";

const char pidfile[200]="/var/run/mpl3115a2d.pid";

int loglev=3;
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

// from Lauri Pirttiaho
  unsigned char reg_address = 0x00;
  unsigned char buffer[10];
  struct i2c_msg rdwr_msgs[2] = 
  {
    {  // Start address
      .addr = MPL3115A2D_ADDRESS,
      .flags = 0, // write
      .len = 1,
      .buf = &reg_address 
    },
    { // Read buffer
      .addr = MPL3115A2D_ADDRESS,
      .flags = I2C_M_RD, // read
      .len = 6,
      .buf = buffer
    }
  };

  struct i2c_rdwr_ioctl_data rdwr_data = 
  {
    .msgs = rdwr_msgs,
    .nmsgs = 2
  };

  unsigned char msb,lsb;
  if( refpres > 0 )
  {
    msb = (unsigned char)(refpres>>8);
    lsb = (unsigned char)(refpres&0x00FF);
    ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x14, msb);
    ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x15, lsb);
  }

  short T=0;
  int P=0;
  unsigned int A=0;
  int fd,rd;
  int cnt=0, result=0;
  while(cont==1)
  {
    unxs=(int)time(NULL); 

    if(((unxs>=nxtpres)||((nxtpres-unxs)>presint))&&(presint>10)) 
    {
      nxtpres = presint + unxs;
      ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x26, 0x38);
      ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x13, 0x07);
      ok = I2cWriteRegister(MPL3115A2D_ADDRESS, 0x26, 0x39);

      if(cont==1)
      {
        usleep(500000);
        if( (fd = open(i2cdev, O_RDWR) ) < 0 ) 
        {
          syslog(LOG_ERR|LOG_DAEMON, "Failed to open i2c port");
        }
        else
        {
          rd = flock(fd, LOCK_EX|LOCK_NB);

          cnt = I2LOCK_MAX;
          while( (rd==1) && (cnt>0) ) // try again if port locking failed
          {
            sleep(1);
            rd = flock(fd, LOCK_EX|LOCK_NB);
            cnt--;
          }

          if(rd)
          {
            syslog(LOG_ERR|LOG_DAEMON, "Failed to open i2c port");
            close(fd);
          }
          else
          {
            if( ioctl(fd, I2C_SLAVE, MPL3115A2D_ADDRESS ) < 0 ) 
            {
              syslog(LOG_ERR|LOG_DAEMON, "Unable to get bus access to talk to slave");
            }
            else
            {
              cont = 0;
              result = ioctl(fd, I2C_RDWR, &rdwr_data);
              if( result < 0 ) 
              {
                sprintf(message, "rdwr ioctl error: %d", errno);
                syslog(LOG_ERR|LOG_DAEMON, "%s", message);
              } 
              else 
              { 
                sprintf(message, "read %02x%02x%02x%02x%02x%02x", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
                syslog(LOG_DEBUG, "%s", message);

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
                cont = 1;
              }
            }
          }
          close(fd);
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
        if((fd = open(i2cdev, O_RDWR)) < 0 ) 
        {
          syslog(LOG_ERR|LOG_DAEMON, "Failed to open i2c port");
        }
        else
        {
          rd = flock(fd, LOCK_EX|LOCK_NB);
          cnt = I2LOCK_MAX;
          while( (rd == 1) && (cnt > 0) ) // try again if port locking failed
          {
            sleep(1);
            rd = flock(fd, LOCK_EX|LOCK_NB);
            cnt--;
          }

          if(rd)
          {
            syslog(LOG_ERR|LOG_DAEMON, "Failed to lock i2c port");
            close(fd);
          }
          else
          {
            if( ioctl(fd, I2C_SLAVE, MPL3115A2D_ADDRESS) < 0 ) 
            {
              syslog(LOG_ERR|LOG_DAEMON, "Unable to get bus access to talk to slave");
            }
            else
            {
              cont = 0;
              result = ioctl(fd, I2C_RDWR, &rdwr_data);
              if( result < 0 ) 
              {
                sprintf(message, "rdwr ioctl error: %d", errno);
                syslog(LOG_ERR|LOG_DAEMON, "%s", message);
              } 
              else 
              { 
                sprintf(message,"read %02x%02x%02x%02x%02x%02x", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
                syslog(LOG_DEBUG, "%s", message);

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
                cont = 1;
              }
            }
          }
          close(fd);
        }
      }
    }
    sleep(1);
  }


  syslog(LOG_NOTICE|LOG_DAEMON, "remove PID file");
  ok = remove( pidfile );

  return ok;
}