/* SD journal. Each record is in a separate file/cluster; no rename required.
   FAT itself is not transactional. Never remove power/card during a save. */
#include "pc_store_internal.h"
#include "fat32.h"
#include <string.h>
static const char *const paths[2]={"/Prince/STATE0.BIN","/Prince/STATE1.BIN"};
static bool ready(void) {
    fat32_init();
    /* Remount at each operation to recover from card swaps and stale metadata.
       No timer callback may mutate the filesystem while this operation runs. */
    fat32_unmount();
    return fat32_is_ready();
}
/* -1: I/O failure, 0: absent/invalid snapshot, 1: valid snapshot. */
static int read_slot(unsigned slot,uint8_t out[PC_RECORD_SIZE]) {
    fat32_file_t file={0};
    fat32_error_t err=fat32_open(&file,paths[slot]);
    if(err==FAT32_ERROR_FILE_NOT_FOUND || err==FAT32_ERROR_DIR_NOT_FOUND)return 0;
    if(err!=FAT32_OK)return -1;
    size_t n=0;
    if(fat32_size(&file)!=PC_RECORD_SIZE || (file.attributes&FAT32_ATTR_DIRECTORY)) {
        fat32_close(&file);return 0;
    }
    err=fat32_read(&file,out,PC_RECORD_SIZE,&n);
    fat32_error_t close_err=fat32_close(&file);
    if(err!=FAT32_OK || close_err!=FAT32_OK)return -1;
    return n==PC_RECORD_SIZE && pc_record_valid(out)?1:0;
}
bool pc_sd_record_read(uint8_t out[PC_RECORD_SIZE]) {
    if(!ready())return false;
    _Alignas(4) uint8_t other[PC_RECORD_SIZE];
    int a=read_slot(0,out),b=read_slot(1,other);
    if(b==1 && (a!=1 || (int32_t)(pc_record_get32(other+4)-pc_record_get32(out+4))>0)) {
        memcpy(out,other,PC_RECORD_SIZE);return true;
    }
    return a==1;
}
bool pc_sd_record_write(const uint8_t record[PC_RECORD_SIZE]) {
    if(!pc_record_valid(record) || !ready())return false;
    fat32_file_t dir={0};
    fat32_error_t err=fat32_open(&dir,"/Prince");
    if(err==FAT32_ERROR_FILE_NOT_FOUND || err==FAT32_ERROR_DIR_NOT_FOUND)
        err=fat32_dir_create(&dir,"/Prince");
    else if(err==FAT32_OK && !(dir.attributes&FAT32_ATTR_DIRECTORY))err=FAT32_ERROR_NOT_A_DIRECTORY;
    fat32_close(&dir);
    if(err!=FAT32_OK)return false;
    _Alignas(4) uint8_t a[PC_RECORD_SIZE],b[PC_RECORD_SIZE];
    int va=read_slot(0,a),vb=read_slot(1,b);
    if(va<0 || vb<0)return false; /* Do not overwrite a snapshot we cannot read. */
    unsigned slot;
    if(va!=1)slot=0;
    else if(vb!=1)slot=1;
    else slot=(int32_t)(pc_record_get32(a+4)-pc_record_get32(b+4))>0?1:0;
    fat32_file_t file={0};
    err=fat32_open(&file,paths[slot]);
    if(err==FAT32_ERROR_FILE_NOT_FOUND)err=fat32_create(&file,paths[slot]);
    if(err!=FAT32_OK)return false;
    /* Reject unexpected files rather than truncate or delete user data. */
    if(file.file_size>PC_RECORD_SIZE || file.start_cluster<2 ||
       (file.attributes&(FAT32_ATTR_DIRECTORY|FAT32_ATTR_READ_ONLY))) {
        fat32_close(&file);return false;
    }
    size_t written=0;
    err=fat32_write(&file,record,PC_RECORD_SIZE,&written);
    fat32_error_t close_err=fat32_close(&file);
    if(err!=FAT32_OK || close_err!=FAT32_OK || written!=PC_RECORD_SIZE)return false;
    return read_slot(slot,a)==1 && !memcmp(a,record,PC_RECORD_SIZE);
}
