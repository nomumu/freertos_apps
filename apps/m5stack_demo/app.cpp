#include <stdio.h>
#include <unistd.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/u_int32.h>
#include <std_msgs/msg/int32.h>

#include <rclc/rclc.h>
#include <rclc/executor.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <M5Stack.h>
#endif

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

rcl_publisher_t publisher_btnA, publisher_btnB, publisher_btnC;
rcl_subscription_t subscriber_fillRgb;
std_msgs__msg__Bool msg_btnA, msg_btnB, msg_btnC;
uint8_t last_btnA, last_btnB, last_btnC;
std_msgs__msg__Int32 rgb;

void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
	RCLC_UNUSED(last_call_time);
	M5.update();
	if (timer != NULL) {
		uint8_t btnA = M5.BtnA.isPressed();
		if( last_btnA != btnA ){
			msg_btnA.data = (btnA>0?true:false);
			RCSOFTCHECK(rcl_publish(&publisher_btnA, &msg_btnA, NULL));
			last_btnA = btnA;
		}
		uint8_t btnB = M5.BtnB.isPressed();
		if( last_btnB != btnB ){
			msg_btnB.data = (btnB>0?true:false);
			RCSOFTCHECK(rcl_publish(&publisher_btnB, &msg_btnB, NULL));
			last_btnB = btnB;
		}
		uint8_t btnC = M5.BtnC.isPressed();
		if( last_btnC != btnC ){
			msg_btnC.data = (btnC>0?true:false);
			RCSOFTCHECK(rcl_publish(&publisher_btnC, &msg_btnC, NULL));
			last_btnC = btnC;
		}
	}
}

void fill_rgb_callback(const void * msgin)
{
	const std_msgs__msg__Int32 * msg = (const std_msgs__msg__Int32 *)msgin;
	M5.Lcd.fillScreen( M5.Lcd.color565( ((msg->data&0xFF0000)>>16), ((msg->data&0xFF00)>>8), (msg->data&0xFF) ));
}

extern "C" void appMain(void * arg)
{
	rcl_allocator_t allocator = rcl_get_default_allocator();
	rclc_support_t support;

	// create init_options
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

	// create node
	rcl_node_t node;
	RCCHECK(rclc_node_init_default(&node, "m5stack_node", "m5stack", &support));

	// create button publisher
	RCCHECK(rclc_publisher_init_default(
		&publisher_btnA,
		&node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
		"btn/a"));
	RCCHECK(rclc_publisher_init_default(
		&publisher_btnB,
		&node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
		"btn/b"));
	RCCHECK(rclc_publisher_init_default(
		&publisher_btnC,
		&node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
		"btn/c"));

	// create fill color subscriber
	RCCHECK(rclc_subscription_init_default(
		&subscriber_fillRgb,
		&node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
		"fill_rgb"));

	// create timer,
	rcl_timer_t timer;
	const unsigned int timer_timeout = 100;
	RCCHECK(rclc_timer_init_default(
		&timer,
		&support,
		RCL_MS_TO_NS(timer_timeout),
		timer_callback));

	// create executor
	rclc_executor_t executor;
	RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));

	unsigned int rcl_wait_timeout = 1000;   // in ms
	RCCHECK(rclc_executor_set_timeout(&executor, RCL_MS_TO_NS(rcl_wait_timeout)));
	RCCHECK(rclc_executor_add_subscription(&executor, &subscriber_fillRgb, &rgb, &fill_rgb_callback, ON_NEW_DATA));
	RCCHECK(rclc_executor_add_timer(&executor, &timer));

	initArduino();
	M5.begin();
	M5.Lcd.setCursor(5,20);
	M5.Lcd.println("Hello micro-ROS!");
	while(1){
		rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
		usleep(10000);
	}

	// free resources
	RCCHECK(rcl_publisher_fini(&publisher_btnA, &node))
	RCCHECK(rcl_publisher_fini(&publisher_btnB, &node))
	RCCHECK(rcl_publisher_fini(&publisher_btnC, &node))
	RCCHECK(rcl_subscription_fini(&subscriber_fillRgb, &node));
	RCCHECK(rcl_node_fini(&node))

  	vTaskDelete(NULL);
}
