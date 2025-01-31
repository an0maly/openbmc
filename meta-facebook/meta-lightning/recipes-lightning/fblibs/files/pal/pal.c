/*
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This file contains code to support IPMI2.0 Specificaton available @
 * http://www.intel.com/content/www/us/en/servers/ipmi/ipmi-specifications.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>
#include <facebook/i2c-dev.h>
#include "pal.h"

#define BIT(value, index) ((value >> index) & 1)

#define LIGHTNING_PLATFORM_NAME "Lightning"
#define LAST_KEY "last_key"
#define LIGHTNING_MAX_NUM_SLOTS 0
#define GPIO_VAL "/sys/class/gpio/gpio%d/value"
#define GPIO_DIR "/sys/class/gpio/gpio%d/direction"

#define GPIO_HAND_SW_ID1 138
#define GPIO_HAND_SW_ID2 139
#define GPIO_HAND_SW_ID4 140
#define GPIO_HAND_SW_ID8 141

#define GPIO_DEBUG_RST_BTN 54
#define GPIO_DEBUG_UART_COUNT 125
#define GPIO_BMC_UART_SWITCH 123

#define GPIO_HB_LED 115

#define I2C_DEV_FAN "/dev/i2c-5"
#define I2C_ADDR_FAN 0x2d
#define FAN_REGISTER_H 0x80
#define FAN_REGISTER_L 0x81

#define LARGEST_DEVICE_NAME 120
#define PWM_DIR "/sys/devices/platform/ast_pwm_tacho.0"
#define PWM_UNIT_MAX 96

const char pal_fru_list[] = "all, peb, pdpb, fcb";
size_t pal_pwm_cnt = 1;
size_t pal_tach_cnt = 12;
const char pal_pwm_list[] = "0";
const char pal_tach_list[] = "0...11";

char * key_list[] = {
"test", // TODO: test kv store
/* Add more Keys here */
LAST_KEY /* This is the last key of the list */
};

char * def_val_list[] = {
  "0", /* test */
  /* Add more def values for the correspoding keys*/
  LAST_KEY /* Same as last entry of the key_list */
};

// Helper Functions
static int
read_device(const char *device, int *value) {
  FILE *fp;
  int rc;

  fp = fopen(device, "r");
  if (!fp) {
    int err = errno;
#ifdef DEBUG
    syslog(LOG_INFO, "failed to open device %s", device);
#endif
    return err;
  }

  rc = fscanf(fp, "%d", value);
  fclose(fp);
  if (rc != 1) {
#ifdef DEBUG
    syslog(LOG_INFO, "failed to read device %s", device);
#endif
    return ENOENT;
  } else {
    return 0;
  }
}

static int
write_device(const char *device, const char *value) {
  FILE *fp;
  int rc;

  fp = fopen(device, "w");
  if (!fp) {
    int err = errno;
#ifdef DEBUG
    syslog(LOG_INFO, "failed to open device for write %s", device);
#endif
    return err;
  }

  rc = fputs(value, fp);
  fclose(fp);

  if (rc < 0) {
#ifdef DEBUG
    syslog(LOG_INFO, "failed to write device %s", device);
#endif
    return ENOENT;
  } else {
    return 0;
  }
}

// Platform Abstraction Layer (PAL) Functions
int
pal_get_platform_name(char *name) {
  strcpy(name, LIGHTNING_PLATFORM_NAME);

  return 0;
}

int
pal_get_num_slots(uint8_t *num) {
  *num = LIGHTNING_MAX_NUM_SLOTS;

  return 0;
}

int
pal_is_fru_prsnt(uint8_t fru, uint8_t *status) {

  switch(fru) {
    case FRU_PEB:
    case FRU_PDPB:
    case FRU_FCB:
      *status = 1;
      break;
    default:
      return -1;
  }
  return 0;
}

int
pal_is_debug_card_prsnt(uint8_t *status) {
  return 0;
}

int
pal_get_server_power(uint8_t slot_id, uint8_t *status) {

  return 0;
}

// Power Off, Power On, or Power Reset the server in given slot
int
pal_set_server_power(uint8_t slot_id, uint8_t cmd) {

  return 0;
}

int
pal_sled_cycle(void) {

  return 0;
}

// Read the Front Panel Hand Switch and return the position
int
pal_get_hand_sw(uint8_t *pos) {

  return 0;
}

// Return the Front panel Power Button
int
pal_get_pwr_btn(uint8_t *status) {

  return 0;
}

// Return the DEBUGCARD's UART Channel Button Status
int
pal_get_uart_chan_btn(uint8_t *status) {

  char path[64] = {0};
  int val;

  sprintf(path, GPIO_VAL, GPIO_DEBUG_UART_COUNT);
  if (read_device(path, &val))
    return -1;

  *status = (uint8_t) val;

  return 0;
}

// Return the current uart position
int
pal_get_uart_chan(uint8_t *status) {

  char path[64] = {0};
  int val;

  sprintf(path, GPIO_VAL, GPIO_BMC_UART_SWITCH);
  if (read_device(path, &val))
    return -1;

  *status = (uint8_t) val;

  return 0;
}

// Set the UART Channel
int
pal_set_uart_chan(uint8_t status) {

  char path[64] = {0};
  char *val;

  val = (status == 0) ? "0": "1";

  sprintf(path, GPIO_VAL, GPIO_BMC_UART_SWITCH);
  if (write_device(path, val))
    return -1;

  return 0;
}

// Return the DEBUGCARD's Reset Button status
int
pal_get_rst_btn(uint8_t *status) {

  char path[64] = {0};
  int val;

  sprintf(path, GPIO_VAL, GPIO_DEBUG_RST_BTN);
  if (read_device(path, &val))
    return -1;

  *status = (uint8_t) val;

  return 0;
}

// Update the Reset button input to the server at given slot
int
pal_set_rst_btn(uint8_t slot, uint8_t status) {

  return 0;
}

// Update the LED for the given slot with the status
int
pal_set_led(uint8_t slot, uint8_t status) {

  return 0;
}

// Update Heartbeet LED
int
pal_set_hb_led(uint8_t status) {
  char path[64] = {0};
  char *val;

  if (status) {
    val = "1";
  } else {
    val = "0";
  }

  sprintf(path, GPIO_VAL, GPIO_HB_LED);
  if (write_device(path, val)) {
    return -1;
  }

  return 0;
}

// Update the Identification LED for the given slot with the status
int
pal_set_id_led(uint8_t slot, uint8_t status) {
  return 0;
}

// Update the USB Mux to the server at given slot
int
pal_switch_usb_mux(uint8_t slot) {

  return 0;
}

// Get the last post code of the given slot
int
pal_post_get_last(uint8_t slot, uint8_t *status) {

  return 0;
}

// Handle the received post code, for now display it on debug card
int
pal_post_handle(uint8_t slot, uint8_t status) {

  return 0;
}


static int
read_kv(char *key, char *value) {

  FILE *fp;
  int rc;

  fp = fopen(key, "r");
  if (!fp) {
    int err = errno;
#ifdef DEBUG
    syslog(LOG_WARNING, "read_kv: failed to open %s", key);
#endif
    return err;
  }

  rc = (int) fread(value, 1, MAX_VALUE_LEN, fp);
  fclose(fp);
  if (rc <= 0) {
#ifdef DEBUG
    syslog(LOG_INFO, "read_kv: failed to read %s", key);
#endif
    return ENOENT;
  } else {
    return 0;
  }
}

static int
write_kv(char *key, char *value) {

  FILE *fp;
  int rc;

  fp = fopen(key, "w");
  if (!fp) {
    int err = errno;
#ifdef DEBUG
    syslog(LOG_WARNING, "write_kv: failed to open %s", key);
#endif
    return err;
  }

  rc = fwrite(value, 1, strlen(value), fp);
  fclose(fp);

  if (rc < 0) {
#ifdef DEBUG
    syslog(LOG_WARNING, "write_kv: failed to write to %s", key);
#endif
    return ENOENT;
  } else {
    return 0;
  }
}

int
pal_get_fru_id(char *str, uint8_t *fru) {

  return lightning_common_fru_id(str, fru);
}

int
pal_get_fru_name(uint8_t fru, char *name) {

  return lightning_common_fru_name(fru, name);
}

int
pal_get_fru_sdr_path(uint8_t fru, char *path) {
  return lightning_sensor_sdr_path(fru, path);
}

int
pal_get_fru_sensor_list(uint8_t fru, uint8_t **sensor_list, int *cnt) {

  switch(fru) {
    case FRU_PEB:
      *sensor_list = (uint8_t *) peb_sensor_list;
      *cnt = peb_sensor_cnt;
      break;
    case FRU_PDPB:
      *sensor_list = (uint8_t *) pdpb_sensor_list;
      *cnt = pdpb_sensor_cnt;
      break;
    case FRU_FCB:
      *sensor_list = (uint8_t *) fcb_sensor_list;
      *cnt = fcb_sensor_cnt;
      break;
    default:
#ifdef DEBUG
      syslog(LOG_WARNING, "pal_get_fru_sensor_list: Wrong fru id %u", fru);
#endif
      return -1;
  }
    return 0;
}


int
pal_sensor_sdr_init(uint8_t fru, sensor_info_t *sinfo) {
  return lightning_sensor_sdr_init(fru, sinfo);
}

int
pal_sensor_read(uint8_t fru, uint8_t sensor_num, void *value) {
  int ret;
  return lightning_sensor_read(fru, sensor_num, value);
}

int
pal_get_sensor_threshold(uint8_t fru, uint8_t sensor_num, uint8_t thresh, void *value) {
  return lightning_sensor_threshold(fru, sensor_num, thresh, value);
}

int
pal_get_sensor_name(uint8_t fru, uint8_t sensor_num, char *name) {
  return lightning_sensor_name(fru, sensor_num, name);
}

int
pal_get_sensor_units(uint8_t fru, uint8_t sensor_num, char *units) {
  return lightning_sensor_units(fru, sensor_num, units);
}

int
pal_get_fruid_path(uint8_t fru, char *path) {
  return lightning_get_fruid_path(fru, path);
}

int
pal_get_fruid_eeprom_path(uint8_t fru, char *path) {
  return lightning_get_fruid_eeprom_path(fru, path);
}

int
pal_get_fruid_name(uint8_t fru, char *name) {
  return lightning_get_fruid_name(fru, name);
}

int
pal_fruid_write(uint8_t slot, char *path) {

  return 0;
}

static int
get_key_value(char* key, char *value) {

  char kpath[64] = {0};

  sprintf(kpath, KV_STORE, key);

  if (access(KV_STORE_PATH, F_OK) == -1) {
    mkdir(KV_STORE_PATH, 0777);
  }

  return read_kv(kpath, value);
}

static int
set_key_value(char *key, char *value) {

  char kpath[64] = {0};

  sprintf(kpath, KV_STORE, key);

  if (access(KV_STORE_PATH, F_OK) == -1) {
    mkdir(KV_STORE_PATH, 0777);
  }

  return write_kv(kpath, value);
}

int
pal_get_key_value(char *key, char *value) {

  int ret;
  int i;

  i = 0;
  while(strcmp(key_list[i], LAST_KEY)) {

    if (!strcmp(key, key_list[i])) {
      // Key is valid
      if ((ret = get_key_value(key, value)) < 0 ) {
#ifdef DEBUG
        syslog(LOG_WARNING, "pal_get_key_value: get_key_value failed. %d", ret);
#endif
        return ret;
      }
      return ret;
    }
    i++;
  }

  return -1;
}

int
pal_set_def_key_value() {

  int ret;
  int i;
  char kpath[64] = {0};

  i = 0;
  while(strcmp(key_list[i], LAST_KEY)) {

  sprintf(kpath, KV_STORE, key_list[i]);

  if (access(kpath, F_OK) == -1) {
      if ((ret = set_key_value(key_list[i], def_val_list[i])) < 0) {
#ifdef DEBUG
        syslog(LOG_WARNING, "pal_set_def_key_value: set_key_value failed. %d", ret);
#endif
      }
    }
    i++;
  }

  return 0;
}

int
pal_set_key_value(char *key, char *value) {

  int ret;
  int i;

  i = 0;
  while(strcmp(key_list[i], LAST_KEY)) {

    if (!strcmp(key, key_list[i])) {
      // Key is valid
      if ((ret = set_key_value(key, value)) < 0) {
#ifdef DEBUG
        syslog(LOG_WARNING, "pal_set_key_value: set_key_value failed. %d", ret);
#endif
        return ret;
      }
      return ret;
    }
    i++;
  }

  return -1;
}

int
pal_get_fru_devtty(uint8_t fru, char *devtty) {

  return 0;
}

void
pal_dump_key_value(void) {
  int i;
  int ret;

  char value[MAX_VALUE_LEN] = {0x0};

  while (strcmp(key_list[i], LAST_KEY)) {
    printf("%s:", key_list[i]);
    if (ret = get_key_value(key_list[i], value) < 0) {
      printf("\n");
    } else {
      printf("%s\n",  value);
    }
    i++;
    memset(value, 0, MAX_VALUE_LEN);
  }
}

int
pal_set_last_pwr_state(uint8_t fru, char *state) {

  return 0;
}

int
pal_get_last_pwr_state(uint8_t fru, char *state) {

  return 0;
}

int
pal_get_sys_guid(uint8_t slot, char *guid) {
  int ret;

  return 0;
}

int
pal_set_sysfw_ver(uint8_t slot, uint8_t *ver) {

  return 0;
}

int
pal_get_sysfw_ver(uint8_t slot, uint8_t *ver) {

  return 0;
}

int
pal_is_bmc_por(void) {

  return 0;
}

int
pal_get_fru_discrete_list(uint8_t fru, uint8_t **sensor_list, int *cnt) {

  return 0;
}

int
pal_sensor_discrete_check(uint8_t fru, uint8_t snr_num, char *snr_name,
    uint8_t o_val, uint8_t n_val) {

  return 0;
}

int
pal_sel_handler(uint8_t fru, uint8_t snr_num) {

  return 0;
}

int
pal_get_event_sensor_name(uint8_t fru, uint8_t snr_num, char *name) {

  return 0;
}

int
pal_parse_sel(uint8_t fru, uint8_t snr_num, uint8_t *event_data, char *error_log) {

  return 0;
}

// Helper function for msleep
void
msleep(int msec) {
  struct timespec req;

  req.tv_sec = 0;
  req.tv_nsec = msec * 1000 * 1000;

  while(nanosleep(&req, &req) == -1 && errno == EINTR) {
    continue;
  }
}

int
pal_set_sensor_health(uint8_t fru, uint8_t value) {

  return 0;
}

int
pal_get_fru_health(uint8_t fru, uint8_t *value) {

  return 0;
}

int
pal_get_fru_list(char *list) {

  strcpy(list, pal_fru_list);
  return 0;
}

int
pal_sensor_threshold_flag(uint8_t fru, uint8_t snr_num, uint8_t *flag) {
  switch (fru) {
    case FRU_PEB:

      switch (snr_num) {
        case PEB_SENSOR_PCIE_SW_TEMP:
        case PEB_SENSOR_PCIE_SW_FRONT_TEMP:
        case PEB_SENSOR_PCIE_SW_REAR_TEMP:
        case PEB_SENSOR_LEFT_CONN_TEMP:
        case PEB_SENSOR_RIGHT_CONN_TEMP:
        case PEB_SENSOR_BMC_TEMP:

          *flag = GETMASK(UCR_THRESH | UNC_THRESH | LNC_THRESH | LCR_THRESH);
          break;
      }
      break;

    case FRU_PDPB:
      switch (snr_num) {
        case PDPB_SENSOR_LEFT_REAR_TEMP:
        case PDPB_SENSOR_LEFT_FRONT_TEMP:
        case PDPB_SENSOR_RIGHT_REAR_TEMP:
        case PDPB_SENSOR_RIGHT_FRONT_TEMP:
        case PDPB_SENSOR_FLASH_TEMP_0:
        case PDPB_SENSOR_FLASH_TEMP_1:
        case PDPB_SENSOR_FLASH_TEMP_2:
        case PDPB_SENSOR_FLASH_TEMP_3:
        case PDPB_SENSOR_FLASH_TEMP_4:
        case PDPB_SENSOR_FLASH_TEMP_5:
        case PDPB_SENSOR_FLASH_TEMP_6:
        case PDPB_SENSOR_FLASH_TEMP_7:
        case PDPB_SENSOR_FLASH_TEMP_8:
        case PDPB_SENSOR_FLASH_TEMP_9:
        case PDPB_SENSOR_FLASH_TEMP_10:
        case PDPB_SENSOR_FLASH_TEMP_11:
        case PDPB_SENSOR_FLASH_TEMP_12:
        case PDPB_SENSOR_FLASH_TEMP_13:
        case PDPB_SENSOR_FLASH_TEMP_14:

          *flag = GETMASK(UCR_THRESH | UNC_THRESH | LNC_THRESH | LCR_THRESH);
          break;
      }
      break;

    case FRU_FCB:
      switch (snr_num) {
        case FCB_SENSOR_BJT_TEMP_1:
        case FCB_SENSOR_BJT_TEMP_2:

          *flag = GETMASK(UCR_THRESH | UNC_THRESH | LNC_THRESH | LCR_THRESH);
          break;

      }
      break;
  }

  return 0;
}

int
pal_get_fan_name(uint8_t num, char *name) {

  switch(num) {

    case FAN_1_FRONT:
      sprintf(name, "Fan 1 Front");
      break;

    case FAN_1_REAR:
      sprintf(name, "Fan 1 Rear");
      break;

    case FAN_2_FRONT:
      sprintf(name, "Fan 2 Front");
      break;

    case FAN_2_REAR:
      sprintf(name, "Fan 2 Rear");
      break;

    case FAN_3_FRONT:
      sprintf(name, "Fan 3 Front");
      break;

    case FAN_3_REAR:
      sprintf(name, "Fan 3 Rear");
      break;

    case FAN_4_FRONT:
      sprintf(name, "Fan 4 Front");
      break;

    case FAN_4_REAR:
      sprintf(name, "Fan 4 Rear");
      break;

    case FAN_5_FRONT:
      sprintf(name, "Fan 5 Front");
      break;

    case FAN_5_REAR:
      sprintf(name, "Fan 5 Rear");
      break;

    case FAN_6_FRONT:
      sprintf(name, "Fan 6 Front");
      break;

    case FAN_6_REAR:
      sprintf(name, "Fan 6 Rear");
      break;

    default:
      return -1;
  }

  return 0;
}

static int
write_fan_value(const int fan, const char *device, const int value) {
  char full_name[LARGEST_DEVICE_NAME];
  char device_name[LARGEST_DEVICE_NAME];
  char output_value[LARGEST_DEVICE_NAME];

  snprintf(device_name, LARGEST_DEVICE_NAME, device, fan);
  snprintf(full_name, LARGEST_DEVICE_NAME, "%s/%s", PWM_DIR, device_name);
  snprintf(output_value, LARGEST_DEVICE_NAME, "%d", value);
  return write_device(full_name, output_value);
}

int
pal_set_fan_speed(uint8_t fan, uint8_t pwm) {
  int unit;
  int ret;

  if (fan >= pal_pwm_cnt) {
    syslog(LOG_INFO, "pal_set_fan_speed: fan number is invalid - %d", fan);
    return -1;
  }

  // Convert the percentage to our 1/96th unit.
  unit = pwm * PWM_UNIT_MAX / 100;

  // For 0%, turn off the PWM entirely
  if (unit == 0) {
    write_fan_value(fan, "pwm%d_en", 0);
    if (ret < 0) {
      syslog(LOG_INFO, "set_fan_speed: write_fan_value failed");
      return -1;
    }

    return 0;

  // For 100%, set falling and rising to the same value
  } else if (unit == PWM_UNIT_MAX) {
    unit = 0;
  }

  ret = write_fan_value(fan, "pwm%d_type", 0);
  if (ret < 0) {
    syslog(LOG_INFO, "set_fan_speed: write_fan_value failed");
    return -1;
  }

  ret = write_fan_value(fan, "pwm%d_rising", 0);
  if (ret < 0) {
    syslog(LOG_INFO, "set_fan_speed: write_fan_value failed");
    return -1;
  }

  ret = write_fan_value(fan, "pwm%d_falling", unit);
  if (ret < 0) {
    syslog(LOG_INFO, "set_fan_speed: write_fan_value failed");
    return -1;
  }

  ret = write_fan_value(fan, "pwm%d_en", 1);
  if (ret < 0) {
    syslog(LOG_INFO, "set_fan_speed: write_fan_value failed");
    return -1;
  }

  return 0;
}

int
pal_get_fan_speed(uint8_t fan, int *rpm) {
  int dev;
  int ret;
  int rpm_h;
  int rpm_l;
  int bank;
  int cnt;

  if (fan >= pal_tach_cnt) {
    syslog(LOG_INFO, "pal_set_fan_speed: fan number is invalid - %d", fan);
    return -1;
  }

  dev = open(I2C_DEV_FAN, O_RDWR);
  if (dev < 0) {
    syslog(LOG_ERR, "get_fan_speed: open() failed");
    close(dev);
    return -1;
  }

  /* Assign the i2c device address */
  ret = ioctl(dev, I2C_SLAVE, I2C_ADDR_FAN);
  if (ret < 0) {
    syslog(LOG_ERR, "get_fan_speed: ioctl() assigning i2c addr failed");
    close(dev);
    return -1;
  }

  /* Read the Bank Register and set it to 0 */
  bank = i2c_smbus_read_byte_data(dev, 0xFF);
  if (bank != 0x0) {
    syslog(LOG_INFO, "read_nct7904_value: Bank Register set to %d", bank);
    if (i2c_smbus_write_byte_data(dev, 0xFF, 0) < 0) {
      syslog(LOG_ERR, "read_nct7904_value: i2c_smbus_write_byte_data: "
          "selecting Bank 0 failed");
      return -1;
    }
  }

  rpm_h = i2c_smbus_read_byte_data(dev, FAN_REGISTER_H + fan*2 /* offset */);
  rpm_l = i2c_smbus_read_byte_data(dev, FAN_REGISTER_L + fan*2 /* offset */);

  close(dev);

  /*
   * cnt[12:5] = 8 LSB bits from rpm_h
   *  cnt[4:0] = 5 LSB bits from rpm_l
   */
  cnt = 0;
  cnt = ((rpm_h & 0xFF) << 5) | (rpm_l & 0x1F);
  if (cnt == 0x1fff || cnt == 0)
    *rpm = 0;
  else
    *rpm = 1350000 / cnt;

  return 0;
}

void
pal_inform_bic_mode(uint8_t fru, uint8_t mode) {
}

void
pal_update_ts_sled() {
}

int
pal_handle_dcmi(uint8_t fru, uint8_t *tbuf, uint8_t tlen, uint8_t *rbuf, uint8_t *rlen) {
}
