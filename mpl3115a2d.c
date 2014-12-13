/**************************************************************************
 * 
 * Read pressure from MPL3115A2D chip with I2C and write it to a log file. 
 *       
 * Copyright (C) 2014 Jaakko Koivuniemi.
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
 * Edit: Sat Dec 13 20:03:43 CET 2014
 *
 * Jaakko Koivuniemi
 **/

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
#include <errno.h>

const int version=20141213; // program version
int presint=300; // pressure measurement interval [s]
int altint=0; // altitude measurement interval [s]

double temperature=0; // temperature [C]
double pressure=0; // pressure [Pa]
double altitude=0; // altitude [m]
unsigned short refpres=0; // reference pressure [0-65535]
const char adatafile[200]="/var/lib/mpl3115a2d/altitude";
const char pdatafile[200]="/var/lib/mpl3115a2d/pressure";
const char tdatafile[200]="/var/lib/mpl3115a2d/temperature";

const char i2cdev[100]="/dev/i2c-1";
const unsigned char address=0x60;
const int  i2lockmax=10; // maximum number of times to try lock i2c port  

const char confile[200]="/etc/mpl3115a2d_config";

const char pidfile[200]="/var/run/mpl3115a2d.pid";

int loglev=3;
const char logfile[200]="/var/log/mpl3115a2d.log";
char message[200]="";

void logmessage(const char logfile[200], const char message[200], int loglev, int msglev)
{
  time_t now;
  char tstr[25];
  struct tm* tm_info;
  FILE *log;

  time(&now);
  tm_info=localtime(&now);
  strftime(tstr,25,"%Y-%m-%d %H:%M:%S",tm_info);
  if(msglev>=loglev)
  {
    log=fopen(logfile, "a");
    if(NULL==log)
    {
      perror("could not open log file");
    }
    else
    { 
      fprintf(log,"%s ",tstr);
      fprintf(log,"%s\n",message);
      fclose(log);
    }
  }
}

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
    sprintf(message,"Read configuration file");
    logmessage(logfile,message,loglev,4);

    while((read=getline(&line,&len,cfile))!=-1)
    {
       if(sscanf(line,"%s %f",par,&value)!=EOF) 
       {
          if(strncmp(par,"LOGLEVEL",8)==0)
          {
             loglev=(int)value;
             sprintf(message,"Log level set to %d",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"ALTINT",7)==0)
          {
             altint=(int)value;
             sprintf(message,"Altitude measurement interval set to %d s",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"PRESINT",7)==0)
          {
             presint=(int)value;
             sprintf(message,"Pressure measurement interval set to %d s",(int)value);
             logmessage(logfile,message,loglev,4);
          }
          if(strncmp(par,"REFPRES",7)==0)
          {
             refpres=(unsigned short)value;
             sprintf(message,"Reference pressure set to %d",(unsigned short)value);
             logmessage(logfile,message,loglev,4);
          }
       }
    }
    fclose(cfile);
  }
  else
  {
    sprintf(message, "Could not open %s", confile);
    logmessage(logfile, message, loglev,4);
  }
}

int cont=1; /* main loop flag */

int write_register(unsigned char reg, unsigned char value)
{
  int fd,rd;
  int cnt=0;
  unsigned char buf[10];

  if((fd=open(i2cdev, O_RDWR)) < 0) 
  {
    sprintf(message,"Failed to open i2c port");
    logmessage(logfile,message,loglev,4);
    return -1;
  }
  rd=flock(fd, LOCK_EX|LOCK_NB);
  cnt=i2lockmax;
  while((rd==1)&&(cnt>0)) // try again if port locking failed
  {
    sleep(1);
    rd=flock(fd, LOCK_EX|LOCK_NB);
    cnt--;
  }
  if(rd)
  {
    sprintf(message,"Failed to lock i2c port");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -2;
  }

  cont=0;
  buf[0]=reg;
  buf[1]=value;
  if(ioctl(fd, I2C_SLAVE, address) < 0) 
  {
    sprintf(message,"Unable to get bus access to talk to slave");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -3;
  }
  if((write(fd, buf, 2)) != 2) 
  {
    sprintf(message,"Error writing to i2c slave");
    logmessage(logfile,message,loglev,4);
    close(fd);
    return -4;
  }
  cont=1;
  close(fd);

  return 1;
}

// write temperature value to file
void write_temp(double t)
{
  FILE *tfile;
  tfile=fopen(tdatafile, "w");
  if(NULL==tfile)
  {
    sprintf(message,"could not write to file: %s",tdatafile);
    logmessage(logfile,message,loglev,4);
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
    logmessage(logfile,message,loglev,4);
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
    logmessage(logfile,message,loglev,4);
  }
  else
  { 
    fprintf(afile,"%-9.2f",a);
    fclose(afile);
  }
}


void stop(int sig)
{
  sprintf(message,"signal %d catched, stop",sig);
  logmessage(logfile,message,loglev,4);
  cont=0;
}

void terminate(int sig)
{
  sprintf(message,"signal %d catched",sig);
  logmessage(logfile,message,loglev,4);

  sleep(1);
  strcpy(message,"stop");
  logmessage(logfile,message,loglev,4);

  cont=0;
}

void hup(int sig)
{
  sprintf(message,"signal %d catched",sig);
  logmessage(logfile,message,loglev,4);
}


int main()
{  
  int ok=0;

  sprintf(message,"mpl3115a2d v. %d started",version); 
  logmessage(logfile,message,loglev,4);

  signal(SIGINT,&stop); 
  signal(SIGKILL,&stop); 
  signal(SIGTERM,&terminate); 
  signal(SIGQUIT,&stop); 
  signal(SIGHUP,&hup); 

  read_config();

  int unxs=(int)time(NULL); // unix seconds
  int nxtpres=unxs; // next time to read pressure and temperature
  int nxtalt=unxs; // next time to read altitude and temperature

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
  sid=setsid();
  if(sid<0) 
  {
    strcpy(message,"failed to create child process"); 
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }
        
  if((chdir("/")) < 0) 
  {
    strcpy(message,"failed to change to root directory"); 
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }
        
  /* Close out the standard file descriptors */
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  FILE *pidf;
  pidf=fopen(pidfile,"w");

  if(pidf==NULL)
  {
    sprintf(message,"Could not open PID lock file %s, exiting", pidfile);
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }

  if(flock(fileno(pidf),LOCK_EX||LOCK_NB)==-1)
  {
    sprintf(message,"Could not lock PID lock file %s, exiting", pidfile);
    logmessage(logfile,message,loglev,4);
    exit(EXIT_FAILURE);
  }

  fprintf(pidf,"%d\n",getpid());
  fclose(pidf);

// from Lauri Pirttiaho
  unsigned char reg_address=0x00;
  unsigned char buffer[10];
  struct i2c_msg rdwr_msgs[2] = 
  {
    {  // Start address
      .addr = address,
      .flags = 0, // write
      .len = 1,
      .buf = &reg_address 
    },
    { // Read buffer
      .addr = address,
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
  if(refpres>0)
  {
    msb=(unsigned char)(refpres>>8);
    lsb=(unsigned char)(refpres&0x00FF);
    ok=write_register(0x14,msb);
    ok=write_register(0x15,lsb);
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
      nxtpres=presint+unxs;
      ok=write_register(0x26,0x38);
      ok=write_register(0x13,0x07);
      ok=write_register(0x26,0x39);
      if(cont==1)
      {
        usleep(500000);
        if((fd=open(i2cdev, O_RDWR)) < 0) 
        {
          sprintf(message,"Failed to open i2c port");
          logmessage(logfile,message,loglev,4);
        }
        else
        {
          rd=flock(fd, LOCK_EX|LOCK_NB);
          cnt=i2lockmax;
          while((rd==1)&&(cnt>0)) // try again if port locking failed
          {
            sleep(1);
            rd=flock(fd, LOCK_EX|LOCK_NB);
            cnt--;
          }
          if(rd)
          {
            sprintf(message,"Failed to lock i2c port");
            logmessage(logfile,message,loglev,4);
            close(fd);
          }
          else
          {
            if(ioctl(fd,I2C_SLAVE,address)<0) 
            {
              sprintf(message,"Unable to get bus access to talk to slave");
              logmessage(logfile,message,loglev,4);
            }
            else
            {
              cont=0;
              result=ioctl(fd,I2C_RDWR,&rdwr_data);
              if(result<0) 
              {
                sprintf(message,"rdwr ioctl error: %d",errno);
                logmessage(logfile,message,loglev,4);
              } 
              else 
              { 
                sprintf(message,"read %02x%02x%02x%02x%02x%02x", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
                logmessage(logfile,message,loglev,2);
                T=((short)buffer[4])<<8;
                T|=(short)buffer[5];
                temperature=((double)T)/256.0; 
                P=((int)buffer[1])<<16;
                P|=((int)buffer[2])<<8;
                P|=((int)buffer[3]);
                pressure=((double)P)/64.0;
                sprintf(message,"T=%-+6.3f P=%-9.2f",temperature,pressure);
                logmessage(logfile,message,loglev,4);
                write_temp(temperature);
                write_pres(pressure);
                cont=1;
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
      ok=write_register(0x26,0xB8);
      ok=write_register(0x13,0x07);
      ok=write_register(0x26,0xB9);
      if(cont==1)
      {
        usleep(500000);
        if((fd=open(i2cdev, O_RDWR)) < 0) 
        {
          sprintf(message,"Failed to open i2c port");
          logmessage(logfile,message,loglev,4);
        }
        else
        {
          rd=flock(fd, LOCK_EX|LOCK_NB);
          cnt=i2lockmax;
          while((rd==1)&&(cnt>0)) // try again if port locking failed
          {
            sleep(1);
            rd=flock(fd, LOCK_EX|LOCK_NB);
            cnt--;
          }
          if(rd)
          {
            sprintf(message,"Failed to lock i2c port");
            logmessage(logfile,message,loglev,4);
            close(fd);
          }
          else
          {
            if(ioctl(fd,I2C_SLAVE,address)<0) 
            {
              sprintf(message,"Unable to get bus access to talk to slave");
              logmessage(logfile,message,loglev,4);
            }
            else
            {
              cont=0;
              result=ioctl(fd,I2C_RDWR,&rdwr_data);
              if(result<0) 
              {
                sprintf(message,"rdwr ioctl error: %d",errno);
                logmessage(logfile,message,loglev,4);
              } 
              else 
              { 
                sprintf(message,"read %02x%02x%02x%02x%02x%02x", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
                logmessage(logfile,message,loglev,2);
                T=((short)buffer[4])<<8;
                T|=(short)buffer[5];
                temperature=((double)T)/256.0; 
                A=((unsigned int)buffer[1])<<24;
                A|=((unsigned int)buffer[2])<<16;
                A|=((unsigned int)buffer[3])<<8;
                altitude=((double)A)/65536.0;
                sprintf(message,"T=%-+6.3f A=%-9.2f",temperature,altitude);
                logmessage(logfile,message,loglev,4);
                write_temp(temperature);
                write_alt(altitude);
                cont=1;
              }
            }
          }
          close(fd);
        }
      }
    }
    sleep(1);
  }

  strcpy(message,"remove PID file");
  logmessage(logfile,message,loglev,4);
  ok=remove(pidfile);

  return ok;
}
