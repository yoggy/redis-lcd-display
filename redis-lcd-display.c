//
// redis-lcd-display.c - Display driver program for AQM0802A
//
// Setup;
//   $ sudo apt-get install redis-server
//   $ sudo apt-get install git-core
//   $ mkdir -p ~/work
//    
//   $ cd ~/work
//   $ git clone git://git.drogon.net/wiringPi
//   $ cd wiringPi
//   $ ./build
//    
//   $ cd ~/work
//   $ git clone https://github.com/redis/hiredis.git
//   $ cd hiredis
//   $ make
//   $ sudo make install
//    
//   $ sudo ldconfig
// 
// Compile & Run:
//   $ cd ~/work
//   $ git clone https://github.com/yoggy/redis-lcd-display.git
//   $ cd redis-lcd-display
//     
//   $ make
//   $ sudo ./redis-lcd-display
// 
// How to use:
//   # normal messages 
//   $ redis-cli set "lcd:0" "RedisLCDDisplay"
//   OK
//   $ redis-cli set "lcd:1" "`date +'%m/%d   %H:%M'`"
//   OK
//   $ redis-cli set "lcd:2" `LANG=C /sbin/ifconfig | grep -v 127.0.0.1 | grep inet | awk '{print $2}' | sed -e 's/addr://'`
//   OK
//   
//   # error message
//   $ redis-cli set "lcd:err" "err: error string..."
//   OK
//   $ redis-cli expire "lcd:err" 10
//   OK
//
// See also :
//   https://www.switch-science.com/catalog/1516/
//   http://akizukidenshi.com/catalog/g/gP-06669/
//
// License:
//   Copyright (c) 2015 yoggy <yoggy0@gmail.com>
//   Released under the MIT license
//   http://opensource.org/licenses/mit-license.php
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#include <hiredis/hiredis.h>

#define AQM0802_ADDR 0x3e

int fd = 0;

void aqm0802_cmd(unsigned char d)
{
	wiringPiI2CWriteReg8(fd, 0x00, d);
}

void aqm0802_char(unsigned char d)
{
	wiringPiI2CWriteReg8(fd, 0x40, d);
}

void aqm0802_clear()
{
	aqm0802_cmd(0x01);
}

void aqm0802_move_cursor(uint8_t x, uint8_t y)
{
	if (y == 0) {
		aqm0802_cmd(0x80 + x);
	}
	else if (y == 1) {
		aqm0802_cmd(0xc0 + x);
	}
}

void aqm0802_print_n(char *str, int len)
{
	int i;

	aqm0802_move_cursor(0, 0);

	if (str == NULL) {
		aqm0802_clear();
		return;
	}

	if (len == 0) {
		aqm0802_clear();
		return;
	}

	// for AQM0802 display size
	int char_count = len;
	if (char_count > 16) char_count = 16;

	// print character
	for (i = 0; i < char_count; ++i) {
		aqm0802_char(str[i]);
		if (i  == 7) {
			aqm0802_move_cursor(0, 1);
		}
	}
}

void aqm0802_print(char *str)
{
	aqm0802_print_n(str, strlen(str));
}

int aqm0802_init(void)
{
	fd = wiringPiI2CSetup(AQM0802_ADDR) ;
	if (fd < 0) {
		fprintf(stderr, "error : wiringPiI2CSetup() failed...\n");
		return 0;
	}

	aqm0802_cmd(0x38);
	aqm0802_cmd(0x39);
	aqm0802_cmd(0x14);
	aqm0802_cmd(0x70);
	aqm0802_cmd(0x56);
	aqm0802_cmd(0x6c);

	aqm0802_cmd(0x38);
	aqm0802_cmd(0x0c);

	aqm0802_clear();

	return 1;
}

int get_string(redisContext *ctx, char *key, char *buf)
{
	int i, len;
	redisReply *reply = NULL;

	// clear buffer
	memset(buf, 0x20, 16);
	buf[16] = 0;

	// get string from Redis...(lcd:0-3)
	reply = redisCommand(ctx, "GET %s", key);
	if (reply == NULL) {
		return 0;
	}
	if (reply->type == REDIS_REPLY_ERROR) {
		freeReplyObject(reply);
		return 0;
	}

	if (reply->str == NULL) {
		freeReplyObject(reply);
		return 0;
	}

	printf("%s -> %s\n", key, reply->str);

	// copy string to buffer
	len = strlen(reply->str);
	if (len > 16) len = 16;
	for (i = 0; i < len; ++i) {
		buf[i] = reply->str[i];
	}

	freeReplyObject(reply);

	return 1;
}

int display(redisContext *ctx, int idx)
{
	int rv;
	char key[64];
	char buf[17];

	// for error message
	if (get_string(ctx, "lcd:err", buf)) {
		// blink message
		if ((millis()/400)%4 == 0) {
			aqm0802_clear();
		}
		else {
			aqm0802_print(buf);
		}
		return 1;
	}

	// for normal message
	snprintf(key, 63, "lcd:%d", idx);

	if (!get_string(ctx, key, buf)) return 0;
	aqm0802_print(buf);

	return rv;
}

int main(int argc, char *argv[])
{
	int i, j;
	redisContext *ctx = NULL;

	setvbuf(stdout, 0, _IONBF, 0);

	wiringPiSetup();
	if (!aqm0802_init()) {
		fprintf(stderr, "error : aqm0802_init() failed...\n");
		return -1;
	}

	// banner
    aqm0802_print("RedisLCDDisplay");

	delay(3000);

	// clear
	aqm0802_clear();

	// connect to redis
	ctx = redisConnect("127.0.0.1", 6379);
	if (ctx->err) {
		fprintf(stderr, "error : cannot connect to Redis...\n");
		fprintf(stderr, "error : %s\n", ctx->errstr);
		return -1;
	}

	// main loop
	while(1) {
		for (i = 0; i < 9; ++i) {
			for (j = 0; j < 5 * 6; ++j) {
				if (!display(ctx, i)) break;
				delay(200);
			}
		}
	}

	return 0;
}
