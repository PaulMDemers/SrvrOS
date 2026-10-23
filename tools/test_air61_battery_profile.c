#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_BATTERY_AIR61
#include "../ports/srvros/uacpi/runtime/battery_native_platform.c"
static struct acpi_table_info captured[8];
static uint8_t *blobs[8];
static unsigned count;
uint64_t acpi_table_count(void){return count;}
const struct acpi_table_info *acpi_table_at(uint64_t i){assert(i<count);return captured+i;}
void *pmm_phys_to_virt(uint64_t p){return (void *)(uintptr_t)p;}
void console_printf(const char *f,...){(void)f;}
static uint8_t *load(const char *dir,const char *name,size_t *n){
 char path[512];snprintf(path,sizeof(path),"%s/%s.dat",dir,name);FILE *f=fopen(path,"rb");assert(f);
 assert(!fseek(f,0,SEEK_END));long z=ftell(f);assert(z>0&&z<=65536);rewind(f);
 uint8_t *p=malloc(z);assert(p&&fread(p,1,z,f)==(size_t)z);fclose(f);*n=z;return p;
}
int main(int argc,char **argv){
 assert(!aml_tables_match());
 struct battery_platform p={0};platform=&p;
 assert(!battery_platform_io_map(0x800,0x400));uint32_t v=8;
 assert(!battery_platform_io(0xb90,8,true,&v));assert(!battery_platform_io(0xa74,8,false,&v));
 if(argc==2){
  for(unsigned i=0;i<8;i++){
   char name[16];snprintf(name,sizeof(name),"SSDT%u",i+1);size_t n;blobs[i]=load(argv[1],name,&n);
   memcpy(captured[i].signature,"SSDT",4);captured[i].length=n;captured[i].physical_address=(uintptr_t)blobs[i];
  }
  count=8;assert(aml_tables_match());
  for(unsigned i=0;i<8;i++){blobs[i][10]^=1;assert(!aml_tables_match());blobs[i][10]^=1;}
  count=7;assert(!aml_tables_match());count=8;
  uint64_t original=captured[7].physical_address;captured[7]=captured[6];assert(!aml_tables_match());captured[7].physical_address=original;
  for(unsigned i=0;i<8;i++)free(blobs[i]);
 }
 puts("air61-battery-profile: PASS SSDT refusal and no GPIO admission; optional actual capture fingerprint checks");
}
