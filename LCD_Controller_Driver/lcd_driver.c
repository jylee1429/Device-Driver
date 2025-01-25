#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/fs.h>

#define LCD_BACK_LIGHT_ON                                                   0x08
#define LCD_BACK_LIGHT_OFF                                                  0x00
#define LCD_SEND_ENABLE_BIT                                                 0x04
#define LCD_DISPLAY_OFF                                                     0x08
// Register Selection
#define LCD_WRITE_INSTRUCTION_REGISTER                                      0x00
#define LCD_WRITE_DATA_REGISTER                                             0x01
#define LCD_READ_BUSY_FLAG_AND_ADDRESS_COUNTER                              0x02
#define LCD_READ_DATA_REGISTER                                              0x03
// LCD Command
#define LCD_CLEAR_DISPLAY                                                   0x01
#define LCD_RETURN_HOME                                                     0x02
// Entry mode set options
// 문자 입력 후 커서의 이동 방향과 디스플레이 자동 이동 여부 설정
#define LCD_CURSOR_LEFT_MOVE_DISPLAY_STAY                                   0x04
#define LCD_CURSOR_LEFT_MOVE_DISPLAY_MOVE                                   0x05
#define LCD_CURSOR_RIGHT_MOVE_DISPLAY_STAY                                  0x06 
#define LCD_CURSOR_RIGHT_MOVE_DISPLAY_MOVE                                  0x07
// Display options
#define LCD_DISPLAY_OFF_CURSOR_OFF_CURSOR_FLASH_OFF                         0x08
#define LCD_DISPLAY_OFF_CURSOR_OFF_CURSOR_FLASH_ON                          0x09
#define LCD_DISPLAY_OFF_CURSOR_ON_CURSOR_FLASH_OFF                          0x0A
#define LCD_DISPLAY_OFF_CURSOR_ON_CURSOR_FLASH_ON                           0x0B
#define LCD_DISPLAY_ON_CURSOR_OFF_CURSOR_FLASH_OFF                          0x0C
#define LCD_DISPLAY_ON_CURSOR_OFF_CURSOR_FLASH_ON                           0x0D
#define LCD_DISPLAY_ON_CURSOR_ON_CURSOR_FLASH_OFF                           0x0E
#define LCD_DISPLAY_ON_CURSOR_ON_CURSOR_FLASH_ON                            0x0F
// Cursor Display shift options
// 커서를 이동하거나 디스플레이 전체를 이동하는 명령
#define LCD_CURSOR_LEFT_MOVE                                                0x10
#define LCD_CURSOR_RIGHT_MOVE                                               0x14
#define LCD_DISPALY_LEFT_MOVE                                               0x18
#define LCD_DISPALY_RIGHT_MOVE                                              0x1C 
// function set options
#define LCD_4BIT_1ROW_5X8_FONT                                              0x20
#define LCD_4BIT_1ROW_5X10_FONT                                             0x24
#define LCD_4BIT_2ROW_5X8_FONT                                              0x28   
#define LCD_8BIT_1ROW_5X8_FONT                                              0x30
#define LCD_8BIT_1ROW_5X10_FONT                                             0x34
#define LCD_8BIT_2ROW_5X8_FONT                                              0x38
// set DDRAM address
#define LCD_SET_DDRAM_ADDRESS                                               0x80    

static struct i2c_client* lcd_client;
static struct timer_list lcd_timer;
static struct work_struct workqueue;
static char date_str[32];
static char time_str[16];

static int i2c_write_bytes(struct i2c_client* client, uint8_t* data, uint8_t len) {
    struct i2c_msg tx_data;
    int ret;
    
    if (!data) {
        dev_err(&client->adapter->dev, "Invalid data buffer\n");
        return -EINVAL;
    }

    tx_data.addr = client->addr;
    tx_data.flags = 0;
    tx_data.len = len;
    tx_data.buf = data;

    ret = i2c_transfer(client->adapter, &tx_data, 1);
    if (ret < 0) {
        dev_err(&client->adapter->dev, "i2c write failed\n");
        return ret;
    }
    return ret;
}

static int send_4bit_data(struct i2c_client* client, uint8_t data, uint8_t mode) {
    uint8_t buffer = data | mode | LCD_BACK_LIGHT_ON;
    uint8_t send_buffer;
    int ret;

    send_buffer = buffer & (~LCD_SEND_ENABLE_BIT);
    ret = i2c_write_bytes(client, &send_buffer, 1);
    if (ret < 0) 
        return ret;
    udelay(500);
    
    send_buffer = buffer | LCD_SEND_ENABLE_BIT;
    ret = i2c_write_bytes(client, &send_buffer, 1);
    if (ret < 0) 
        return ret;
    udelay(500);
    
    send_buffer = buffer & (~LCD_SEND_ENABLE_BIT);
    ret = i2c_write_bytes(client, &send_buffer, 1);

    return ret;
}

static int send_data(struct i2c_client* client, uint8_t data, uint8_t mode) {
    uint8_t upper_bits = data & 0xf0;
    uint8_t low_bits = (data << 4) & 0xf0;
    int ret;

    ret = send_4bit_data(client, upper_bits, mode);
    if (ret < 0) {
        return ret;
    }

    ret = send_4bit_data(client, low_bits, mode);
    if (ret < 0) {
        return ret;
    }
    
    return 0;
}

static int send_command(struct i2c_client* client, uint8_t data) {
    return send_data(client, data, LCD_WRITE_INSTRUCTION_REGISTER);
}

static int send_char(struct i2c_client* client, uint8_t data) {
    return send_data(client, data, LCD_WRITE_DATA_REGISTER);
}

static void lcd_display_clear(struct i2c_client* client) {
    send_command(client, LCD_CLEAR_DISPLAY);
    msleep(2);
}

static void lcd_cursor_return_home(struct i2c_client* client) {
    send_command(client, LCD_RETURN_HOME);
    msleep(2);
}

static void lcd_set_cursor(struct i2c_client* client, uint8_t row, uint8_t column) {
    uint8_t address_counter = 0;

    if (row == 0) {
        address_counter = 0x00 + column;
    }
    else {
        address_counter = 0x40 + column;
    }

    send_command(client, LCD_SET_DDRAM_ADDRESS | address_counter);
}


static int lcd_init(struct i2c_client *client) {
    int ret;

    msleep(50);
    // 8bit function set
    for (int i = 0; i < 3; i++) {
        ret = send_command(client, 0x03);
        if (ret < 0) {
            dev_err(&client->dev, "Failed to send command 0x03 on attempt %d\n", i + 1);
            return ret;
        }
        msleep(5);
    }
    // 4bit functionset 
    ret = send_command(client, 0x02);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to send command set 4bit\n");
        return ret;
    }
    // function set
    ret = send_command(client, LCD_4BIT_2ROW_5X8_FONT);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to configure function set\n");
        return ret;
    }

    // display off
    ret = send_command(client, LCD_DISPLAY_OFF);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to LCD display off\n");
        return ret;
    }
    // display clear
    ret = send_command(client, LCD_CLEAR_DISPLAY);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to LCD clear display\n");
        return ret;
    }
    // set entry mode
    ret = send_command(client, LCD_CURSOR_RIGHT_MOVE_DISPLAY_STAY);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to configure entry mode\n");
        return ret;
    }

    ret = send_command(client, LCD_DISPLAY_ON_CURSOR_OFF_CURSOR_FLASH_OFF);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to configure Display options\n");
        return ret;
    }
    dev_info(&client->dev, "LCD initialization successful\n");

    return 0;
}

static void lcd_workqueue_display(struct work_struct* work) {
    int i;
    
    lcd_display_clear(lcd_client);

    lcd_set_cursor(lcd_client, 0, 0);
    for (i = 0; date_str[i] != '\0'; i++) {
        send_char(lcd_client, date_str[i]);
    }

    lcd_set_cursor(lcd_client, 1, 0);
    for (i = 0; time_str[i] != '\0'; i++) {
        send_char(lcd_client, time_str[i]);
    }
}

static void lcd_timer_callback(struct timer_list* timer) {
    struct timespec64 ts;
    struct tm tm;

    ktime_get_real_ts64(&ts);
    time64_to_tm(ts.tv_sec, 0, &tm);

    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    snprintf(date_str, sizeof(date_str), "%04ld-%02d-%02d %s", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             (tm.tm_wday == 0 ? "Sun" :
              tm.tm_wday == 1 ? "Mon" :
              tm.tm_wday == 2 ? "Tue" :
              tm.tm_wday == 3 ? "Wed" :
              tm.tm_wday == 4 ? "Thu" :
              tm.tm_wday == 5 ? "Fri" : "Sat"));

    schedule_work(&workqueue);
    mod_timer(timer, jiffies + HZ);
}

static int lcd_probe(struct i2c_client* client) {
    int ret;
    struct device* dev = &client->dev;

    lcd_client = client;
    ret = lcd_init(client);
    if (ret < 0) {
        dev_err(dev, "Failed to initialize LCD\n");
        return ret;
    }

    INIT_WORK(&workqueue, lcd_workqueue_display);
    timer_setup(&lcd_timer, lcd_timer_callback, 0);
    mod_timer(&lcd_timer, jiffies + HZ);

    dev_info(dev, "LCD driver initialized\n");
    
    return 0;
}

static void lcd_remove(struct i2c_client* client) {
    struct device* dev = &client->dev;
    
    del_timer_sync(&lcd_timer);
    dev_info(dev, "LCD driver removed\n");
}

static const struct of_device_id lcd_of_match[] = {
    { .compatible = "custom, lcd-device"},
    {},
};
MODULE_DEVICE_TABLE(of, lcd_of_match);

static struct i2c_driver lcd_driver = {
    .driver = {
        .name = "lcd",
        .of_match_table = of_match_ptr(lcd_of_match),
    },
    .probe = lcd_probe,
    .remove = lcd_remove,
};

module_i2c_driver(lcd_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LEEJAEYOUNG");
MODULE_DESCRIPTION("LCD Device Driver");
