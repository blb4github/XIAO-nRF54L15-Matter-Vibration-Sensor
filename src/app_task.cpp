/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "app_task.h"

#include "app/matter_init.h"
#include "app/task_executor.h"
#include "board/board.h"
#include "lib/core/CHIPError.h"
#include "lib/support/CodeUtils.h"

#include <setup_payload/OnboardingCodesUtil.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <cmath>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <zephyr/logging/log.h>

void mailbox_battery_init();
void mailbox_green_led_pulse();

LOG_MODULE_DECLARE(app, CONFIG_CHIP_APP_LOG_LEVEL);

using namespace ::chip;
using namespace ::chip::app;
using namespace ::chip::DeviceLayer;


// LSM6DS3TR-C wake-up interrupt via P0.02.
// Detectie wordt door de IMU uitgevoerd, niet door CPU-polling.

static const struct device *mailbox_imu =
    DEVICE_DT_GET(DT_ALIAS(imu0));

static const struct i2c_dt_spec mailbox_imu_i2c =
    I2C_DT_SPEC_GET(DT_ALIAS(imu0));

static const struct device *mailbox_gpio =
    DEVICE_DT_GET(DT_NODELABEL(gpio0));

constexpr gpio_pin_t MAILBOX_INT_PIN = 2;

static struct gpio_callback mailbox_gpio_callback;

static void mailbox_imu_irq_handler(struct k_work *work);
K_WORK_DEFINE(mailbox_irq_work, mailbox_imu_irq_handler);

static void mailbox_gpio_isr(const struct device *port,
                             struct gpio_callback *cb,
                             gpio_port_pins_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    k_work_submit(&mailbox_irq_work);
}

// Alarm actief tot 5 seconden na de laatste gedetecteerde schok.
static bool alarm_latched = false;
static void mailbox_alarm_timeout(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(mailbox_alarm_timer, mailbox_alarm_timeout);

// Wordt uitgevoerd op de Matter-thread.
static void mailbox_matter_alarm_update(intptr_t context)
{
    const bool alarm = (context != 0);

    auto status =
        chip::app::Clusters::BooleanState::Attributes::StateValue::Set(
            1, alarm);

    if (status == chip::Protocols::InteractionModel::Status::Success) {
        LOG_INF("MAILBOX Matter endpoint 1 StateValue=%d", alarm ? 1 : 0);
    } else {
        LOG_ERR("MAILBOX Matter attribute update failed: %u",
                static_cast<unsigned>(status));
    }
}

// Wordt uitgevoerd na 5 seconden zonder nieuwe detectie.
static void mailbox_alarm_timeout(struct k_work *work)
{
    ARG_UNUSED(work);

    alarm_latched = false;
    LOG_INF("MAILBOX ALARM OFF: 5 seconds without detection");

    CHIP_ERROR err = PlatformMgr().ScheduleWork(
        mailbox_matter_alarm_update, 0);

    if (err != CHIP_NO_ERROR) {
        LOG_ERR("Failed to schedule Matter alarm reset: %"
                CHIP_ERROR_FORMAT, err.Format());
    }
}

static void mailbox_imu_irq_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    // Lezen van WAKE_UP_SRC bevestigt een latched wake-up event.
    uint8_t source = 0;
    int rc = i2c_reg_read_byte_dt(&mailbox_imu_i2c, 0x1B, &source);

    if (rc != 0) {
        LOG_ERR("IMU WAKE_UP_SRC read failed: %d", rc);
        return;
    }

    // WU_IA = bit 3.
    if ((source & 0x08) == 0) {
        return;
    }

    // Korte LED-puls bij iedere bevestigde IMU-detectie.
    mailbox_green_led_pulse();

    // Iedere detectie verlengt het alarm met vijf seconden.
    k_work_reschedule(&mailbox_alarm_timer, K_SECONDS(5));

    if (!alarm_latched) {
        alarm_latched = true;
        LOG_INF("MAILBOX ALARM ON: IMU hardware wake-up");

        CHIP_ERROR err = PlatformMgr().ScheduleWork(
            mailbox_matter_alarm_update, 1);

        if (err != CHIP_NO_ERROR) {
            LOG_ERR("Failed to schedule Matter alarm: %"
                    CHIP_ERROR_FORMAT, err.Format());
        }
    } else {
        LOG_INF("MAILBOX ALARM retriggered");
    }
}

// Configureer de interne bewegingsdetectie van de LSM6DS3TR-C.
static int mailbox_imu_enable_interrupt()
{
    if (!device_is_ready(mailbox_imu) ||
        !i2c_is_ready_dt(&mailbox_imu_i2c) ||
        !device_is_ready(mailbox_gpio)) {
        return -ENODEV;
    }

    struct sensor_value odr = { .val1 = 52, .val2 = 0 };

    int rc = sensor_attr_set(
        mailbox_imu, SENSOR_CHAN_ACCEL_XYZ,
        SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);

    if (rc != 0) {
        return rc;
    }

    // Disable INT1 wake-up routing during configuration.
    rc = i2c_reg_write_byte_dt(&mailbox_imu_i2c, 0x5E, 0x00);
    if (rc != 0) return rc;

    // TAP_CFG: interrupts enabled, slope filter, latched interrupt.
    rc = i2c_reg_write_byte_dt(&mailbox_imu_i2c, 0x58, 0x81);
    if (rc != 0) return rc;

    // WAKE_UP_DUR: geen extra detectieduur.
    rc = i2c_reg_write_byte_dt(&mailbox_imu_i2c, 0x5C, 0x00);
    if (rc != 0) return rc;

    // WAKE_UP_THS: één drempelstap bij +/-2g.
    rc = i2c_reg_write_byte_dt(&mailbox_imu_i2c, 0x5B, 0x01);
    if (rc != 0) return rc;

    rc = gpio_pin_configure(mailbox_gpio, MAILBOX_INT_PIN,
                            GPIO_INPUT);
    if (rc != 0) return rc;

    gpio_init_callback(&mailbox_gpio_callback, mailbox_gpio_isr,
                       BIT(MAILBOX_INT_PIN));

    rc = gpio_add_callback(mailbox_gpio, &mailbox_gpio_callback);
    if (rc != 0) return rc;

    // Oude wake-up status wissen.
    uint8_t source = 0;
    rc = i2c_reg_read_byte_dt(&mailbox_imu_i2c, 0x1B, &source);
    if (rc != 0) return rc;

    rc = gpio_pin_interrupt_configure(
        mailbox_gpio, MAILBOX_INT_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    if (rc != 0) return rc;

    // MD1_CFG: route wake-up event naar INT1.
    rc = i2c_reg_write_byte_dt(&mailbox_imu_i2c, 0x5E, 0x20);
    if (rc != 0) return rc;

    LOG_INF("Mailbox IMU hardware interrupt enabled on P0.02");
    return 0;
}

CHIP_ERROR AppTask::Init()
{
	/* Initialize Matter stack */
	ReturnErrorOnFailure(Nrf::Matter::PrepareServer());

	if (!Nrf::GetBoard().Init()) {
		LOG_ERR("User interface initialization failed.");
		return CHIP_ERROR_INCORRECT_STATE;
	}

	/* Register Matter event handler that controls the connectivity status LED based on the captured Matter network
	 * state. */
	ReturnErrorOnFailure(Nrf::Matter::RegisterEventHandler(Nrf::Board::DefaultMatterEventHandler, 0));

	CHIP_ERROR err = Nrf::Matter::StartServer();
	if (err != CHIP_NO_ERROR) {
		return err;
	}

	mailbox_battery_init();

    // Synchroniseer de initiële alarmstatus met Matter.
    CHIP_ERROR alarm_err = PlatformMgr().ScheduleWork(
        mailbox_matter_alarm_update, 0);

    if (alarm_err != CHIP_NO_ERROR) {
        LOG_ERR("Failed to initialize Matter alarm: %"
                CHIP_ERROR_FORMAT, alarm_err.Format());
    }

    int imu_rc = mailbox_imu_enable_interrupt();

    if (imu_rc != 0) {
        LOG_ERR("Mailbox IMU interrupt configuration failed: %d",
                imu_rc);
    }


	return CHIP_NO_ERROR;
}

CHIP_ERROR AppTask::StartApp()
{
	ReturnErrorOnFailure(Init());

	while (true) {
		Nrf::DispatchNextTask();
	}

	return CHIP_NO_ERROR;
}
