#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

#define LCD_SETDDRAMADDR                                                    0x80
#define LCD_ENABLE_BACKLIGHT                                                0x08
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
#define LCD_4BIT_1ROW_5X11_FONT                                             0x24
#define LCD_4BIT_2ROW_5X8_FONT                                              0x28    
#define LCD_4BIT_2ROW_5X11_FONT                                             0x2c
#define LCD_8BIT_1ROW_5X8_FONT                                              0x30
#define LCD_8BIT_1ROW_5X11_FONT                                             0x34
#define LCD_8BIT_2ROW_5X8_FONT                                              0x38
#define LCD_8BIT_2ROW_5X11_FONT                                             0x3C

static struct i2c_client* lcd_client;
static struct timer_list timer;
static struct work_struct workqueue;
static char time_str[16];

static int send_4bit_data(struct i2c_client* client, uint8_t data, uint8_t mode) {
    uint8_t buffer = data | mode | LCD_ENABLE_BACKLIGHT;
    uint8_t send_buffer;
    int ret;

    send_buffer = buffer & (~0x04);
    ret = i2c_master_send(client, &send_buffer, 1);
    if (ret < 0) 
        return ret;
    udelay(500);
    
    send_buffer = buffer | 0x04;
    ret = i2c_master_send(client, &send_buffer, 1);
    if (ret < 0) 
        return ret;
    udelay(500);
    
    send_buffer = buffer & (~0x04);
    ret = i2c_master_send(client, &send_buffer, 1);
    
    return ret;
}

static int send(struct i2c_client* client, uint8_t data, uint8_t mode) {
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
    int ret;

    ret = send(client, data, 0);
    return ret;
}

static int send_char(struct i2c_client* client, uint8_t data) {
    int ret;

    ret = send(client, data, 1);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to send character: %c (0x%02x)\n", data, data);
    }
    return ret;
}

static void lcd_display_clear(struct i2c_client* client) {
    send_command(client, LCD_CLEAR_DISPLAY);
    msleep(2);
}

static void lcd_cursor_init(struct i2c_client* client) {
    send_command(client, LCD_RETURN_HOME);
    msleep(2);
}

void lcd_set_cursor(struct i2c_client *client, uint8_t row, uint8_t column) {
    uint8_t ddram_address = 0;

    if (row == 0) {
        ddram_address = 0x00 + column; 
    } 
    else if (row == 1) {
        ddram_address = 0x40 + column;
    }

    send_command(client, LCD_SETDDRAMADDR | ddram_address);
}


// Initialize LCD
static int lcd_init(struct i2c_client *client) {
    msleep(50);
    send_command(client, 0x03);
    msleep(5);
    send_command(client, 0x03);
    msleep(5);
    send_command(client, 0x03);
    msleep(5);
    send_command(client, 0x02);

    send_command(client, LCD_4BIT_1ROW_5X8_FONT);
    send_command(client, LCD_DISPLAY_ON_CURSOR_OFF_CURSOR_FLASH_OFF);
    send_command(client, LCD_CURSOR_RIGHT_MOVE_DISPLAY_STAY);

    lcd_display_clear(client);
    lcd_cursor_init(client);
  
    dev_info(&client->dev, "LCD Init success\n");    
    return 0;
}

static void lcd_workqueue_display(struct work_struct* work) {
    int i;
    
    lcd_display_clear(lcd_client);
    lcd_set_cursor(lcd_client, 0, 1);
   
    for (i = 0; time_str[i] != '\0'; i++) {
        send_char(lcd_client, time_str[i]);
    }
    
}

static void lcd_timer_callback(struct timer_list* timer) {
    struct timespec64 ts;
    struct tm tm;

    // get the current time
    ktime_get_real_ts64(&ts);
    time64_to_tm(ts.tv_sec, 0, &tm);

    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    schedule_work(&workqueue);
    mod_timer(timer, jiffies + HZ);
}

static int lcd_probe(struct i2c_client* client) {
    int ret;
  
    lcd_client = client;
    ret = lcd_init(client);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to initialize LCD\n");
        return ret;
    }
    INIT_WORK(&workqueue, lcd_workqueue_display);
     
    timer_setup(&timer, lcd_timer_callback, 0);
    mod_timer(&timer, jiffies + HZ);

    dev_info(&client->dev, "LCD driver initialized\n");
    return 0;
}

static void lcd_remove(struct i2c_client* client) {
    del_timer_sync(&timer);
    dev_info(&client->dev, "LCD driver removed\n");
}

static const struct of_device_id lcd_of_match[] = {
    {.compatible = "lcd"},
    {},
};
MODULE_DEVICE_TABLE(of, lcd_of_match);

static struct i2c_driver lcd_driver = {
    .driver = {
        .name = "lcd",
        .of_match_table = lcd_of_match,
    },
    .probe = lcd_probe,
    .remove = lcd_remove,
};

module_i2c_driver(lcd_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LEEJAEYOUNG");
MODULE_DESCRIPTION("LCD Device Driver");
