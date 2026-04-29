# `scd41_lib` - Hướng dẫn sử dụng (cập nhật)

README này đã được cập nhật theo code hiện tại trong thư mục `scd41_lib/`.

## 1) Mục tiêu thiết kế

Thư viện được gom lại theo kiểu runtime context (`scd4x_runtime_t`) để trong `main.c` chỉ còn:

1. tạo runtime struct
2. khởi tạo với `I2C handle + mutex`
3. start periodic measurement
4. poll trong vòng lặp và in dữ liệu

Ngoài ra, thư viện đã có sẵn:

- bảo vệ mutex cho giao tiếp I2C
- callback event cho fault/recovered/restart-attempt
- default ITM logger (short log mặc định)
- auto recover khi mất kết nối sensor
- auto recover I2C peripheral khi SDA/SCL bị rút/cắm lại

## 2) Các file cần include

Trong `Core/Src/main.c`:

```c
#include "main.h"
#include "cmsis_os.h"
#include "itm.h"
#include "scd4x_i2c.h"
```

## 3) Cách dùng khuyến nghị (giống code hiện tại)

```c
void StartTaskInput(void *argument)
{
  itm_print("TaskInput started\r\n");

  scd4x_runtime_t scd41;

  scd4x_runtime_init(&scd41, &hi2c2, MutexI2C2Handle);
  scd4x_runtime_set_event_callback(
      &scd41,
      scd4x_runtime_default_itm_event_handler,
      NULL);
  (void)scd4x_runtime_start_periodic_measurement(&scd41);

  for (;;)
  {
    osDelay(1000);

    if (scd4x_runtime_poll(&scd41))
    {
      scd41_print_scd41_measurement(scd41.co2,
                                    scd41.temperature_m_deg_c,
                                    scd41.humidity_m_percent_rh);
    }
  }
}
```

## 4) API runtime quan trọng

- `scd4x_runtime_init(...)`
  - bind `I2C_HandleTypeDef*`
  - bind `osMutexId_t`
  - đặt địa chỉ mặc định `SCD41_I2C_ADDR_62`
  - reset các trường dữ liệu + chuẩn đoán

- `scd4x_runtime_start_periodic_measurement(...)`
  - lock mutex
  - gửi lệnh start periodic measurement
  - unlock mutex

- `scd4x_runtime_poll(...)`
  - đọc dữ liệu nếu ready
  - trả `true` khi có mẫu mới
  - khi lỗi: phát event, phân loại fault, retry/recover

- `scd4x_runtime_set_event_callback(...)`
  - gán callback tùy chỉnh để app xử lý fault/recovered

## 5) Event callback và logger mặc định

Event type:

- `SCD4X_RUNTIME_EVENT_FAULT`
- `SCD4X_RUNTIME_EVENT_RECOVERED`
- `SCD4X_RUNTIME_EVENT_RESTART_ATTEMPT`

Sử dụng nhanh:

```c
scd4x_runtime_set_event_callback(
    &scd41,
    scd4x_runtime_default_itm_event_handler,
    NULL);
```

Mặc định logger đang ở short-log mode (giảm spam). Nếu cần full debug:

```c
scd4x_runtime_default_itm_set_short_log(false);
```

## 6) Runtime struct (các trường mới)

Ngoài dữ liệu đo (`co2`, `temperature_m_deg_c`, `humidity_m_percent_rh`),
`scd4x_runtime_t` còn có thêm:

- `error`, `fault_cause`, `fault_active`
- `i2c_hal_state`, `i2c_hal_error`
- `consecutive_error_count`, `total_error_count`
- `restart_after_failures`
- `on_event`, `on_event_context`

## 7) Cơ chế bảo vệ và phục hồi I2C

Đã có trong thư viện:

- Mutex protection cho I2C access (HAL read/write)
- Tự động re-init peripheral khi lỗi bus (`sensirion_i2c_hal_recover_bus()`)
- Tự động restart periodic measurement sau khi kết nối lại

Mục tiêu là khi:

- mất nguồn sensor rồi cấp lại
- rút/cắm lại SDA hoặc SCL

thì hệ thống tự khôi phục và tiếp tục in dữ liệu khi sensor ổn định lại.

## 8) Lưu ý sử dụng

- Phải tạo mutex trước khi gọi `scd4x_runtime_init(...)`.
- Nếu dùng bus khác, truyền handle khác vào `scd4x_runtime_init(...)`.
- Thư viện đang tối ưu cho runtime API; API low-level vẫn tồn tại nhưng không khuyến khích cho app chính.

## 9) Tóm tắt nhanh

Ba bước cơ bản:

```c
scd4x_runtime_t scd41;
scd4x_runtime_init(&scd41, &hi2c2, MutexI2C2Handle);
scd4x_runtime_set_event_callback(&scd41, scd4x_runtime_default_itm_event_handler, NULL);
```

Sau đó start + poll:

```c
(void)scd4x_runtime_start_periodic_measurement(&scd41);

if (scd4x_runtime_poll(&scd41)) {
  scd41_print_scd41_measurement(scd41.co2,
                                scd41.temperature_m_deg_c,
                                scd41.humidity_m_percent_rh);
}
```
