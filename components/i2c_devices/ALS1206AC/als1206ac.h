#ifndef ALS1206AC_H
#define ALS1206AC_H
int asl1206ac_init(void);
int asl1206ac_read_data_status(void);
int asl1206ac_read_als(uint16_t *als);

#endif