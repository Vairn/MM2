#include <stdio.h>
#include "mm2_anm_codec.h"
#include "mm2_anm_preview.h"
static void writeppm(const char* out, mm2_anm_composite_rgba* c){
  FILE* f=fopen(out,"wb"); if(!f)return;
  fprintf(f,"P6\n%d %d\n255\n",c->width,c->height);
  for(int y=0;y<c->height;y++)for(int x=0;x<c->width;x++){
    uint8_t* p=c->rgba+((size_t)y*c->width+x)*4;
    fputc(p[0],f); fputc(p[1],f); fputc(p[2],f);
  }
  fclose(f);
}
int main(){
  const char* ids[]={"39","47","33","68","36",0};
  for(int i=0;ids[i];i++){
    char path[64], out[64]; snprintf(path,sizeof(path),"../../%s.anm",ids[i]); snprintf(out,sizeof(out),"anm_%s_f0.ppm",ids[i]);
    mm2_anm_file anm={}; if(mm2_anm_load_file(path,&anm)!=MM2_ANM_OK) continue;
    mm2_anm_composite_rgba comp={}; mm2_anm_composite_frame_rgba(&anm,0,&comp);
    writeppm(out,&comp); printf("wrote %s %dx%d frames=%u\n",out,comp.width,comp.height,anm.frame_count);
    mm2_anm_composite_rgba_free(&comp); mm2_anm_free(&anm);
  }
}
