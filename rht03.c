// filename: rht03.c
// description: RHT03 / DHT22 / AM2303 handler
// use register library
// compile: cc rht03.c -o rht03 -lrt -lregister
// execute: sudo ./rht03

#include <register.h>
#include <time.h>

#define TIMEOUT 1000

// measure humidity and temperature
// port: GPIO number, *hum: pointer to hum var, *tmp: pointer to tmp var
// return: success flag
int rht03(int port, int *hum, int *tmp){
  static struct timespec last;// time of last access
  struct timespec now;// this time
  uint32_t usec;// interval
  uint32_t *regs; // register map
  int i, d, b;// loop counter
  int th;// threshold
  uint8_t data[5];// raw data
  static int h, t;// humidity and temperature measurement

  // check interval
  clock_gettime(CLOCK_MONOTONIC, &now);
  usec = (now.tv_sec - last.tv_sec) * 1000000
    + (now.tv_nsec - last.tv_nsec) / 1000;
  if(usec < 2000000){// at least 2S
    *hum = h;
    *tmp = t;
    return 0;
  }
  last.tv_sec = now.tv_sec;
  last.tv_nsec = now.tv_nsec;

  // get register map
  regs = regs_map(OFF_PRT);
  if(regs == MAP_FAILED){
    fprintf(stderr, "fail to get registers map. %s\n", strerror(errno));
    return -1;
  }

  // check bus free
  fnc_wr(regs, port, FNC_IN);// set port as input
  if(prt_rd(regs, port) == 0){// line is L
    fprintf(stderr, "bus busy.\n");
    munmap((void *)regs, getpagesize());
    return -1;
  }

  // start request
  fnc_wr(regs, port, FNC_OUT);// set port as output
  prt_wr(regs, port, 0);// H->L
  usleep(1000);// keep L at least 500uS
  fnc_wr(regs, port, FNC_IN);// set port as input, L->H by pull up

  // wait for H->L
  for(i = 0; prt_rd(regs, port); i++){
    if(i > TIMEOUT){
      fprintf(stderr, "response timeout\n");
      munmap((void *)regs, getpagesize());
      return -1;
    }
  }

  // count for L->H
  for(i = 0; prt_rd(regs, port) == 0; i++){
    if(i > TIMEOUT){
      fprintf(stderr, "base time too long.\n");
      munmap((void *)regs, getpagesize());
      return -1;
    }
  }

  th = i * 50 / 80;// calculate 50uS from base time

  // wait for H->L
  for(i = 0; prt_rd(regs, port); i++){
    if(i > TIMEOUT){
      fprintf(stderr, "signal not detect.\n");
      munmap((void *)regs, getpagesize());
      return -1;
    }
  }

  // read data
  for(d = 0; d < 5; d++){// data
    for(b = 0; b < 8; b++){// bit
      while(prt_rd(regs, port) == 0);//L->H
      for(i = 0; prt_rd(regs, port); i++){//H->L
        if(i > TIMEOUT){
          fprintf(stderr, "timeout at data:%d, bit:%d\n", d, b);
          munmap((void *)regs, getpagesize());
          return -1;
        }
      }
      data[d] <<= 1;// next bit
      if(i > th) // set 1 or 0(nothing)
        data[d] |= 1;
    }
  }
  munmap((void *)regs, getpagesize());// free register map

  //checksum
  if(data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xff)){
    fprintf(stderr, "checksum unmuch.\n");
    return -1;
  }

  h = (int)data[0] * 256 + data[1];// relative humidity * 10
  t = (int)(data[2] & 0x7f) * 256 + data[3];// relative temperature * 10
  if(data[2] & 0x80)// if minus
    t *= -1;// set sign bit
  *hum = h;
  *tmp = t;
  return 0;
}

// rht03 function handling example
// RHT03 shall be connected to GPIO24
// display relative humidity and temperature, and repeat
int main(){
  int hum, tmp, ret;

  printf("RHT03 handling example.\n");
  printf("type ^C then stop.\n\n");
  sleep(1);

  while(-1){
    ret = rht03(24, &hum, &tmp);// read RHT03
    if(ret)// error
      continue;// do nothing
    printf("humidity:%.1f, temperature:%.1f\n",
      (float)hum / 10,//relative humidity
      (float)tmp / 10);//relative temperature
    sleep(2);// interval
  }
  return 0;
}
