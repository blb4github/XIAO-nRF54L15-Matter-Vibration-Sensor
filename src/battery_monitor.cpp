#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <platform/CHIPDeviceLayer.h>
#include <lib/core/CHIPError.h>

LOG_MODULE_REGISTER(mailbox_battery, LOG_LEVEL_INF);

// XIAO nRF54L15 green user LED, active-low on P2.00.
static const struct gpio_dt_spec mailbox_green_led =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);




// Groene LED: 100 ms puls per IMU-trigger.
// Nieuwe triggers tijdens een puls verlengen deze niet.
static atomic_t mailbox_led_active;
static bool mailbox_led_ready = false;

static void mailbox_led_off(struct k_work *work)
{
    ARG_UNUSED(work);

    if (mailbox_led_ready) {
        gpio_pin_set_dt(&mailbox_green_led, 0);
    }

    atomic_set(&mailbox_led_active, 0);
}

K_WORK_DELAYABLE_DEFINE(mailbox_led_timer, mailbox_led_off);

void mailbox_green_led_pulse()
{
    if (!mailbox_led_ready) {
        return;
    }

    // Alleen een nieuwe puls starten wanneer de LED uit is.
    if (!atomic_cas(&mailbox_led_active, 0, 1)) {
        return;
    }

    int rc = gpio_pin_set_dt(&mailbox_green_led, 1);
    if (rc != 0) {
        atomic_set(&mailbox_led_active, 0);
        LOG_WRN("Green LED ON failed: %d", rc);
        return;
    }

    k_work_schedule(&mailbox_led_timer, K_MSEC(100));
}

#define BATTERY_NODE DT_PATH(zephyr_user)

static const struct adc_dt_spec battery_adc =
    ADC_DT_SPEC_GET_BY_IDX(BATTERY_NODE, 0);

static const struct device *battery_regulator =
    DEVICE_DT_GET(DT_NODELABEL(vbat_pwr));


// Voorlopige LiPo-spanning-naar-percentage schatting.
// Alleen indicatief, vooral tijdens laden niet nauwkeurig.
struct BatteryPoint {
    uint16_t mv;
    uint8_t percent;
};

static uint8_t mailbox_battery_percent(uint32_t mv)
{
    static constexpr BatteryPoint curve[] = {
        {3300,   0},
        {3600,  10},
        {3700,  20},
        {3750,  30},
        {3800,  40},
        {3850,  50},
        {3900,  60},
        {3950,  70},
        {4000,  80},
        {4100,  90},
        {4200, 100},
    };

    if (mv <= curve[0].mv) {
        return 0;
    }

    for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
        if (mv <= curve[i].mv) {
            const auto &a = curve[i - 1];
            const auto &b = curve[i];

            return a.percent +
                (mv - a.mv) * (b.percent - a.percent) /
                (b.mv - a.mv);
        }
    }

    return 100;
}

// Deze callback draait op de Matter-thread.
static void mailbox_matter_battery_update(intptr_t context)
{
    const uint32_t mv = static_cast<uint32_t>(context);
    const uint8_t percent = mailbox_battery_percent(mv);

    using namespace chip::app::Clusters::PowerSource::Attributes;
    using Status = chip::Protocols::InteractionModel::Status;

    const auto voltage_status = BatVoltage::Set(1, mv);

    // Matter gebruikt halve procenten: 200 = 100%.
    const auto percent_status =
        BatPercentRemaining::Set(
            1, static_cast<uint8_t>(percent * 2));

    if (voltage_status == Status::Success &&
        percent_status == Status::Success) {
        LOG_INF("MAILBOX Matter battery: %u mV, approx %u%%",
                static_cast<unsigned>(mv),
                static_cast<unsigned>(percent));
    } else {
        LOG_ERR("MAILBOX Matter battery update failed: voltage=%u percent=%u",
                static_cast<unsigned>(voltage_status),
                static_cast<unsigned>(percent_status));
    }
}

static void battery_measure(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(battery_work, battery_measure);

static void battery_measure(struct k_work *work)
{
    ARG_UNUSED(work);

    int rc = regulator_enable(battery_regulator);

    if (rc != 0) {
        LOG_ERR("Battery measurement enable failed: %d", rc);
        goto next_measurement;
    }

    // De meetkring heeft een korte stabilisatietijd nodig.
    k_msleep(10);

    {
        int16_t raw = 0;
        struct adc_sequence sequence = {
            .buffer = &raw,
            .buffer_size = sizeof(raw),
        };

        rc = adc_sequence_init_dt(&battery_adc, &sequence);

        if (rc == 0) {
            rc = adc_read_dt(&battery_adc, &sequence);
        }

        if (rc == 0) {
            int32_t mv = raw;
            rc = adc_raw_to_millivolts_dt(&battery_adc, &mv);

            if (rc == 0) {
                int32_t battery_mv = mv * 2;
                LOG_INF("MAILBOX BATTERY: %d mV (%d.%03d V)",
                        battery_mv,
                        battery_mv / 1000,
                        battery_mv % 1000);

                // De ADC-meting doorgeven aan de Matter-thread.
                if (battery_mv >= 3000 && battery_mv <= 4300) {
                    CHIP_ERROR err =
                        chip::DeviceLayer::PlatformMgr().ScheduleWork(
                            mailbox_matter_battery_update,
                            static_cast<intptr_t>(battery_mv));

                    if (err != CHIP_NO_ERROR) {
                        LOG_ERR("Failed to schedule Matter battery update");
                    }
                } else {
                    LOG_WRN("Battery voltage outside expected range: %d mV",
                            battery_mv);
                }
            }
        }

        if (rc != 0) {
            LOG_ERR("Battery ADC measurement failed: %d", rc);
        }
    }

    rc = regulator_disable(battery_regulator);

    if (rc != 0) {
        LOG_ERR("Battery measurement disable failed: %d", rc);
    }

next_measurement:
    k_work_reschedule(&battery_work, K_HOURS(1));
}

void mailbox_battery_init()
{
    // Keep the green user LED off to save battery power.
    if (gpio_is_ready_dt(&mailbox_green_led)) {
        int led_rc = gpio_pin_configure_dt(
            &mailbox_green_led, GPIO_OUTPUT_INACTIVE);
        if (led_rc != 0) {
            LOG_WRN("Green LED disable failed: %d", led_rc);
        }
        mailbox_led_ready = (led_rc == 0);
    }

    if (!adc_is_ready_dt(&battery_adc) ||
        !device_is_ready(battery_regulator)) {
        LOG_ERR("Battery ADC or regulator not ready");
        return;
    }

    int rc = adc_channel_setup_dt(&battery_adc);

    if (rc != 0) {
        LOG_ERR("Battery ADC channel setup failed: %d", rc);
        return;
    }

    LOG_INF("Battery monitoring initialized (1 hour interval)");
    k_work_schedule(&battery_work, K_SECONDS(3));
}
