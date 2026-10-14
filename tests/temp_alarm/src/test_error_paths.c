/*
 * temp_alarm - Error-path unit tests (homework skeleton)
 *
 * Task 2: fill in the stubs according to TEST_SPEC.md. Each stub calls
 * ztest_test_skip(), so the binary builds and runs cleanly until the test is
 * implemented.
 *
 * Run:
 *   west twister -T tests/temp_alarm -p native_sim
 */

#include <errno.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "sensor_fake.h"
#include "temp_alarm.h"

#define DEFAULT_THRESHOLD 30

/* Same before() hook as in test_basic.c: a clean fake and an initialised module. */
static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	sensor_fake_reset();
	sensor_fake_init();

	zassume_ok(temp_alarm_init(&fake_sensor_dev, DEFAULT_THRESHOLD),
		   "precondition: temp_alarm_init must succeed");
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_sensor_errors
 *
 * Failures from the sensor driver, injected through the fakes.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_sensor_errors, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_sensor_errors, test_fetch_error_propagates)
{
	struct temp_alarm_status status;

	// precondition
	fake_sensor_sample_fetch_fake.return_val = -EIO;

	// check
	zassert_equal(temp_alarm_check(), -EIO, "check must return -EIO");

	//Verify #1: 
	// status.last_error == -EIO. 
	// The module stops after the failed fetch: channel_get is never called, so no stale value is read.
	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");
	zassert_equal(status.last_error, -EIO, "last_error must be -EIO, got %d", status.last_error);
	zassert_equal(fake_sensor_channel_get_fake.call_count, 0, "channel_get must not be called after a failed fetch, got %u", fake_sensor_channel_get_fake.call_count);

}

ZTEST(temp_alarm_sensor_errors, test_channel_get_error_propagates)
{
	struct temp_alarm_status status;

	// precondition
	fake_sensor_channel_get_fake.custom_fake = NULL; // need to remove the custom fake to ensure that channel_get return an error
	fake_sensor_channel_get_fake.return_val = -EIO;

	// check
	zassert_equal(temp_alarm_check(), -EIO, "check must return -EIO");

	// Verify #2:
	// status.last_error == -EIO.
	// The sample was still fetched exactly once.
	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");
	zassert_equal(status.last_error, -EIO, "last_error must be -EIO, got %d", status.last_error);
	zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1, "sample_fetch must be called exactly once, got %u", fake_sensor_sample_fetch_fake.call_count);
}

ZTEST(temp_alarm_sensor_errors, test_fail_then_recover)
{
	struct temp_alarm_status status;
	static int fetch_returns[] = { -EIO, 0 }; 	// sample_fetch returns -EIO on the 1st call, then 0 on the 2nd.

	// precondition
	SET_RETURN_SEQ(fake_sensor_sample_fetch, fetch_returns, ARRAY_SIZE(fetch_returns));
	sensor_fake_set_temperature(20);

	// check
	zassert_equal(temp_alarm_check(), -EIO, "first check must return -EIO");
	zassert_ok(temp_alarm_check(), "second check must succeed");

	// Verify #3:
	// The first check() returns -EIO and the second returns 0.
	// After the second check, status.last_temp == 20, status.is_alarming == false
	// and status.last_error == -EIO (the last error is kept until a reset).
	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");
	zassert_equal(status.last_temp, 20, "last_temp must be 20, got %d", status.last_temp);
	zassert_false(status.is_alarming, "20 C is below the threshold %d, so no alarm", DEFAULT_THRESHOLD);
	zassert_equal(status.last_error, -EIO, "last_error must still be -EIO, got %d", status.last_error);
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_invalid_input
 *
 * Arguments the public API must reject.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_invalid_input, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_invalid_input, test_get_status_null)
{
	// Verify #1: temp_alarm_get_status(NULL) returns exactly -EINVAL.
	zassert_equal(temp_alarm_get_status(NULL), -EINVAL, "get_status(NULL) must return -EINVAL");
}

ZTEST(temp_alarm_invalid_input, test_init_without_device)
{

	// Verify #2: temp_alarm_init(NULL, 30) returns exactly -ENODEV.
	zassert_equal(temp_alarm_init(NULL, DEFAULT_THRESHOLD), -ENODEV, "init without a device must return -ENODEV");
}

ZTEST(temp_alarm_invalid_input, test_init_threshold_out_of_range)
{
	//Verify #3: init(&fake_sensor_dev, -1) and init(&fake_sensor_dev, 101) both return exactly -EINVAL.
	zassert_equal(temp_alarm_init(&fake_sensor_dev, -1), -EINVAL, "init with threshold -1 must return -EINVAL");
	zassert_equal(temp_alarm_init(&fake_sensor_dev, 101), -EINVAL, "init with threshold 101 must return -EINVAL");
}

ZTEST(temp_alarm_invalid_input, test_set_threshold_out_of_range)
{
	struct temp_alarm_status status;

	// call set threshold
	zassert_equal(temp_alarm_set_threshold(-1), -EINVAL, "set_threshold(-1) must return -EINVAL");
	zassert_equal(temp_alarm_set_threshold(101), -EINVAL, "set_threshold(101) must return -EINVAL");

	//Verify #4:
	// Both calls return exactly -EINVAL.
	// The rejected values did not change the threshold: 30 C is still in force.
	
	// Check first 30°C then 31°C to ensure the threshold still the same 
	sensor_fake_set_temperature(30);
	zassert_ok(temp_alarm_check(), "check at 30 C must succeed");
	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");
	zassert_false(status.is_alarming, "30 C must not alarm: threshold must still be 30");
	sensor_fake_set_temperature(31);
	zassert_ok(temp_alarm_check(), "check at 31 C must succeed");
	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");
	zassert_true(status.is_alarming, "31 C must alarm: threshold must still be 30");
}
