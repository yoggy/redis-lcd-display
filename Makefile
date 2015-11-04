redis-lcd-display: redis-lcd-display.c
	gcc redis-lcd-display.c -o redis-lcd-display -lwiringPi -lhiredis

run: redis-lcd-display
	sudo ./redis-lcd-display

clean:
	rm -rf redis-lcd-display
	
