CC = g++
CFLAGS = -Wall -Wextra
LDFLAGS = -L/usr/lib
LIBS = -lRTIMULib -lsqlite3

air_quality: air_quality.cpp sgp30.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f air_quality