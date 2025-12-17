CC = gcc

CFLAGS = -g -pthread -Ipthread -Wall

# final executable name
TARGET = park

# default
all: $(TARGET)

# link to executable
$(TARGET): park.c
	$(CC) $(CFLAGS) -o $(TARGET) park.c

# clean build
clean:
	rm -f $(TARGET)