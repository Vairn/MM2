#include <stdio.h>
#include "mm2_anm_codec.h"
#include "mm2_anm_preview.h"
int main() {
  const char* ids[] = {"39","47","68","33","36",0};
  for (int i=0; ids[i]; i++) {
    char path[64];
    snprintf(path,sizeof(path),"../../%s.anm",ids[i]);
    mm2_anm_file anm={};
    if (mm2_anm_load_file(path,&anm)!=MM2_ANM_OK) { printf("%s load fail\n",path); continue; }
    int cf = mm2_anm_default_overlay_composed_frame(&anm);
    mm2_anm_composite_rgba comp={};
    mm2_anm_composite_frame_rgba(&anm,cf,&comp);
    printf("%s frames=%u composed=%d size=%dx%d\n", ids[i], anm.frame_count, cf, comp.width, comp.height);
    mm2_anm_composite_rgba_free(&comp);
    mm2_anm_free(&anm);
  }
  return 0;
}
