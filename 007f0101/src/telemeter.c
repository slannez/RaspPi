#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

int open_port(void) {
  int fd;
  fd = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) { perror("open_port: Unable to open /edv/ttyAMA0"); }
  else { fcntl(fd, F_SETFL, 0); }
  return (fd);
}

int configure(int fd) {
  struct termios options;
  tcgetattr(fd, &options);
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

  tcflush(fd, TCIFLUSH);
  tcsetattr(fd, TCSANOW, &options);
  return (fd);
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

int main(int iargc, const char** arg){
  int fd;
  char buffer[255];
  char *bufptr;
  int nbytes;
  char ch[3];
  
  fd = open_port();
  configure(fd);

  while (1) {
    tcflush(fd, TCIFLUSH);
    ch[0] = 0;
    do {
      ch[3] = ch[0];
      nbytes = read(fd, ch, 1);
      if (nbytes==0) { close(fd); exit(1); }
    } while( (ch[0] != 0x02) || (ch[3] != 0x03) );

    bufptr = buffer;
    do {
      nbytes = read(fd, ch, 1);
      if (nbytes==0) { close(fd); exit(2); }
      (*bufptr)=ch[0];
      ++bufptr;
    } while(ch[0] != 0x03);
    *bufptr = 0;
    printf("%s\n",buffer);
    printf("-----------\n");
  }
  close(fd);
  return(0);
}

