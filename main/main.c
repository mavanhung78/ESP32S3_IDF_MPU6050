#include <stdio.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <mpu6050.h>

#ifdef CONFIG_EXAMPLE_I2C_ADDRESS_LOW
#define ADDR MPU6050_I2C_ADDRESS_LOW
#else
#define ADDR MPU6050_I2C_ADDRESS_HIGH
#endif

static const char *TAG = "mpu6050_angle";

#define CALIB_SAMPLES 200

float ax_offset = 0;
float ay_offset = 0;
float az_offset = 0;

float gx_offset = 0;
float gy_offset = 0;
float gz_offset = 0;

void calibrate_mpu6050(mpu6050_dev_t *dev)
{
    ESP_LOGI(TAG, "Calibrating MPU6050... Keep sensor still");

    mpu6050_acceleration_t accel;
    mpu6050_rotation_t gyro;

    for (int i = 0; i < CALIB_SAMPLES; i++)
    {
        mpu6050_get_motion(dev, &accel, &gyro);

        ax_offset += accel.x;
        ay_offset += accel.y;
        az_offset += accel.z - 1.0;   // trừ gravity

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

    ESP_LOGI(TAG,"Offset accel: %.4f %.4f %.4f", ax_offset, ay_offset, az_offset);
    ESP_LOGI(TAG,"Offset gyro : %.4f %.4f %.4f", gx_offset, gy_offset, gz_offset);
}

void mpu6050_test(void *pvParameters)
{
    mpu6050_dev_t dev = {0};

    ESP_ERROR_CHECK(mpu6050_init_desc(&dev, ADDR, 0,
                                      CONFIG_EXAMPLE_SDA_GPIO,
                                      CONFIG_EXAMPLE_SCL_GPIO));

    while (1)
    {
        if (i2c_dev_probe(&dev.i2c_dev, I2C_DEV_WRITE) == ESP_OK)
        {
            ESP_LOGI(TAG, "MPU6050 found");
            break;
        }

        ESP_LOGE(TAG, "MPU6050 not found");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(mpu6050_init(&dev));

    calibrate_mpu6050(&dev);

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

        ESP_LOGI(TAG, "Angle: Roll=%.2f Pitch=%.2f", roll, pitch);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main()
{
    ESP_ERROR_CHECK(i2cdev_init());

    xTaskCreate(mpu6050_test, "mpu6050_test",
                configMINIMAL_STACK_SIZE * 6,
                NULL, 5, NULL);
}