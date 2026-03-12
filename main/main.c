#include <stdio.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <mpu6050.h>
#include <led_strip.h>

#ifdef CONFIG_EXAMPLE_I2C_ADDRESS_LOW
#define ADDR MPU6050_I2C_ADDRESS_LOW
#else
#define ADDR MPU6050_I2C_ADDRESS_HIGH
#endif

#define LED_GPIO 48
#define LED_NUM 1
#define CALIB_SAMPLES 200

static const char *TAG = "mpu6050_system";

mpu6050_dev_t dev;
static led_strip_handle_t led_strip;

float ax_offset = 0;
float ay_offset = 0;
float az_offset = 0;

float gx_offset = 0;
float gy_offset = 0;
float gz_offset = 0;

////////////////////////////////////////////////////////////
//// LED
////////////////////////////////////////////////////////////

void led_init()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_NUM,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

void led_error()
{
    while (1)
    {
        led_set(50,0,0);   // đỏ
        vTaskDelay(pdMS_TO_TICKS(200));
        led_set(0,0,0);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

////////////////////////////////////////////////////////////
//// MPU6050 Calibration
////////////////////////////////////////////////////////////

void calibrate_mpu6050()
{
    ESP_LOGI(TAG, "Calibrating MPU6050... Keep sensor still");

    led_set(50,50,0); // vàng

    mpu6050_acceleration_t accel;
    mpu6050_rotation_t gyro;

    for (int i = 0; i < CALIB_SAMPLES; i++)
    {
        mpu6050_get_motion(&dev, &accel, &gyro);

        ax_offset += accel.x;
        ay_offset += accel.y;
        az_offset += accel.z - 1.0;

        gx_offset += gyro.x;
        gy_offset += gyro.y;
        gz_offset += gyro.z;

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ax_offset /= CALIB_SAMPLES;
    ay_offset /= CALIB_SAMPLES;
    az_offset /= CALIB_SAMPLES;

    gx_offset /= CALIB_SAMPLES;
    gy_offset /= CALIB_SAMPLES;
    gz_offset /= CALIB_SAMPLES;

    ESP_LOGI(TAG,"Accel offset: %.4f %.4f %.4f", ax_offset, ay_offset, az_offset);
    ESP_LOGI(TAG,"Gyro offset : %.4f %.4f %.4f", gx_offset, gy_offset, gz_offset);
}

////////////////////////////////////////////////////////////
//// MPU6050 Task
////////////////////////////////////////////////////////////

void mpu6050_task(void *pvParameters)
{
    calibrate_mpu6050();

    led_set(0,50,0); // xanh → sensor OK

    while (1)
    {
        mpu6050_acceleration_t accel;
        mpu6050_rotation_t gyro;

        mpu6050_get_motion(&dev, &accel, &gyro);

        float ax = accel.x - ax_offset;
        float ay = accel.y - ay_offset;
        float az = accel.z - az_offset;

        float roll  = atan2(ay, az) * 180.0 / M_PI;
        float pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / M_PI;

        ESP_LOGI(TAG,"Angle: Roll=%.2f Pitch=%.2f", roll, pitch);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

////////////////////////////////////////////////////////////
//// Platform Init
////////////////////////////////////////////////////////////

esp_err_t platform_init()
{
    ESP_ERROR_CHECK(i2cdev_init());

    ESP_ERROR_CHECK(mpu6050_init_desc(&dev, ADDR, 0,
                                      CONFIG_EXAMPLE_SDA_GPIO,
                                      CONFIG_EXAMPLE_SCL_GPIO));

    if (i2c_dev_probe(&dev.i2c_dev, I2C_DEV_WRITE) != ESP_OK)
    {
        ESP_LOGE(TAG, "MPU6050 not detected!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MPU6050 detected");

    ESP_ERROR_CHECK(mpu6050_init(&dev));

    return ESP_OK;
}

////////////////////////////////////////////////////////////
//// System Launch
////////////////////////////////////////////////////////////

void system_launch()
{
    xTaskCreate(mpu6050_task,
                "mpu6050_task",
                4096,
                NULL,
                5,
                NULL);
}

////////////////////////////////////////////////////////////
//// MAIN
////////////////////////////////////////////////////////////

void app_main()
{
    led_init();

    if (platform_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Platform init failed!");
        led_error();
    }

    system_launch();
}