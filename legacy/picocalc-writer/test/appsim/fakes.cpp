#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hw/keyboard.h"
#include "hw/board.h"
#include "diag.h"
static int cs=1, dc=0, curCmd=-1; static std::vector<uint8_t> args;
uint8_t gram[480][320][3]; static int cx0,cx1,cy0,cy1,px,py,pb; static uint8_t pix[3];
void gpio_put(int pin,int v){ if(pin==LCD_CS_PIN) cs=v; if(pin==LCD_DC_PIN) dc=v; }
int spi_write_blocking(spi_inst_t*, const uint8_t* d, size_t n){
  if(cs) return n;
  for(size_t i=0;i<n;i++){ uint8_t b=d[i];
    if(!dc){ curCmd=b; args.clear(); pb=0; if(b==0x2C){px=cx0;py=cy0;} continue;}
    if(curCmd==0x2A){args.push_back(b); if(args.size()==4){cx0=args[0]<<8|args[1];cx1=args[2]<<8|args[3];}}
    else if(curCmd==0x2B){args.push_back(b); if(args.size()==4){cy0=args[0]<<8|args[1];cy1=args[2]<<8|args[3];}}
    else if(curCmd==0x2C){ pix[pb++]=b; if(pb==3){pb=0; if(py<480&&px<320) memcpy(gram[py][px],pix,3); if(++px>cx1){px=cx0; if(++py>cy1) py=cy0;}}}
  }
  return n;
}
static int frame=0;
void dump(){ char fn[64]; sprintf(fn,"out/f%02d.ppm",frame++); FILE*f=fopen(fn,"wb"); fprintf(f,"P6 320 320 255\n");
  for(int y=0;y<320;y++) fwrite(gram[y],1,320*3,f); fclose(f); }
// scripted keyboard
struct K{uint8_t c; bool ctrl;};
static std::vector<K> script; static size_t si=0; static int idle=0;
bool Keyboard::init(){ const char* s=getenv("KEYS"); // comma separated hex or chars
  for(const char* p=s; p&&*p;){ unsigned v; int n; if(sscanf(p,"%x%n",&v,&n)==1){script.push_back({uint8_t(v),false}); p+=n;} if(*p==',')p++; else break;}
  return true;}
bool Keyboard::poll(KeyEvent& ev){ if(++idle<3) return false; idle=0; dump(); if(si>=script.size()){ exit(0);} ev=KeyEvent(); ev.code=script[si].c; si++; return true;}
KeyEvent Keyboard::wait(){ KeyEvent ev; while(!poll(ev)){} return ev; }
int Keyboard::battery(){ return 73; }
namespace diag{ void begin(){} void displayReady(){} void stage(Stage){} void note(const char*){} void done(){} bool hadIncompleteStart(){return false;} int previousStage(){return -1;}
[[noreturn]] void crashScreen(const char*,uint32_t,uint32_t,const char*){abort();} }
