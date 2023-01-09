CC = gcc

CFLAGS = -Wall -Werror -m32 -g -fsanitize=address

SRC_DIR = ./src
OBJ_DIR = ./obj
INCLUDE = -Iinclude/

TARGET = test

SRCS = cy_malloc.c cy_list.c cy_bitmap.c test.c
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

#OBJS 안의 object 파일들 이름 앞에 $(OBJ_DIR)/을 붙인다.
OBJECTS = $(patsubst %.o,$(OBJ_DIR)/%.o,$(OBJS))

all: $(TARGET)

$(TARGET) : $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) 

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@ -MD 


.PHONY: clean all
clean:
	rm -f $(OBJECTS) $(TARGET)

-include $(DEPS)
