#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/time.h>

/**
 * Hypothesis: No more than 255 characters received from
 *             teleinfo per message
 *             Linux >= 2.6.23
 **/
#define EOT 0x03
#define NL 0x0A
#define CR 0x0D

#define FADCO 0
#define FOPTARIF 1
#define FISOUSC 2
#define FHCHC 3
#define FHCHP 4
#define FPTEC 5
#define FIINST 6
#define FIMAX 7
#define FPAPP 8
#define FHHPHC 9
#define FMOTDETAT 10

/***********************************************/
/* Data structure for SagemC1 teleinfo message */
/***********************************************/
struct msg_sagem_c1 {
  long int adco; /* identification compteur */
  char optarif; /* option tarifaire */
  int isousc; /* intensite souscrite */
  long int hchc; /* compteur heure creuse */
  long int hchp; /* compteur heure pleine */
  char ptec; /* ??? */
  long int iinst; /* intensite instantanee */
  long int imax; /* intensite maximum */
  long int papp; /* puissance apparente */
  char hhphc; /* type periode courante */
  char * motdetat; /* ??? */
} ;

int open_port(void) {
  int fin;
  fin = open("/dev/ttyAMA0", O_RDONLY | O_NOCTTY | O_NDELAY | O_CLOEXEC);
  if (fin == -1) { perror("open_port: Unable to open /edv/ttyAMA0"); }
  else { fcntl(fin, F_SETFL, 0); }
  return (fin);
}

int configure(int fin) {
  struct termios options;
  tcgetattr(fin, &options);
  cfsetispeed(&options, B1200);
  cfsetospeed(&options, B1200);
  options.c_cflag |= (CLOCAL | CREAD);
  options.c_cflag |= PARENB;
  options.c_cflag &= ~PARODD;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS7;
  options.c_cflag &= ~(CRTSCTS);
  options.c_iflag |= (INPCK | ISTRIP);
  options.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL);
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_oflag &= ~OPOST;
  options.c_cc[VTIME] = 80;
  options.c_cc[VMIN] = 0;

  tcflush(fin, TCIFLUSH);
  tcsetattr(fin, TCSANOW, &options);
  return (fin);
} 

int istextfield(int fid) {
  if (fid==FOPTARIF || fid==FPTEC || fid==FHHPHC || fid==FMOTDETAT) 
    { return 1; } 
  else 
    { return 0; }
}

int checksum(char *row) {
  char *label, *value, crc;
  unsigned char sum = 32;
  int i;
  sscanf(row, "%s %s %s", label, value, &crc);
  for(i=0;i<strlen(label); i++) sum += label[i];
  for(i=0;i<strlen(value); i++) sum += value[i];
  if (sum==crc) return 1;
  else return 0;
}

int main(int argc, const char** argv){
  int fin;
  FILE* fout;
  char buffer[255];
  char *bufptr;
  int nbytes;
  char ch[3];
  struct timeval tv;
  int irec,i,fn;

  /* Default settings */ 
  fout=stdout;

  for(i=1;i<argc;) {
    if (strncmp(argv[i],"-f",2)==0) {
      ++i;
      printf("Output set to: %s",argv[i]);
      fout=fopen(argv[i],"ae"); }
    else if (strncmp(argv[i],"-d",2)==0) {
      ++i;
      if (daemon(1,1)!=0) { printf("Failed to start daemon"); exit(1); } }
    else { ++i; }
  }

  fin = open_port();
  configure(fin);

  while (1) {
    tcflush(fin, TCIFLUSH);
    ch[0] = 0;
    /* Skip message separation */
    do {
      ch[3] = ch[0];
      nbytes = read(fin, ch, 1);
      if (nbytes==0) { close(fin); exit(1); }
    } while( (ch[0] != 0x02) || (ch[3] != 0x03) );

    bufptr = buffer;
    (*bufptr++) = '{';
    (*bufptr++) = '"';
    fn = 0;
    irec = 0;
    do {
      nbytes = read(fin, ch, 1);
      if (nbytes==0) { close(fin); exit(2); }
      if (irec<=1) {
        /* DATA */
        if (ch[0]==' ') { 
          if (irec==0) { 
            (*bufptr++)='"'; 
            (*bufptr++)=':';
            if (istextfield(fn)) { (*bufptr++)='"'; } } 
            ++irec; }
        else if (ch[0]==NL || ch[0]==CR) { }
        else if (*(bufptr-1)==':' && !istextfield(fn) && ch[0]=='0') { }
        else { (*bufptr++) = ch[0]; }
      } else {
        /* CRC */
        if (ch[0]==CR) { 
          if (istextfield(fn)) { (*bufptr++)='"'; }
          (*bufptr++)=','; 
          (*bufptr++)='"'; 
          irec=0; ++fn; }
        else if (ch[0]==NL) { irec=0; }
        else if (ch[0]==EOT) { irec=0; }
      }
    } while(ch[0] != EOT);
    gettimeofday(&tv,NULL);
    sprintf(bufptr,"TS\":{\"SEC\":%i,\"USEC\":%i}}",tv.tv_sec,tv.tv_usec);
    fprintf(fout,"%s\n",buffer);
  }
  close(fin);
  return(0);
}

