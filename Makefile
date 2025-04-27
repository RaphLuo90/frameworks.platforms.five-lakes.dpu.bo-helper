LDFLAGS := -shared -fPIC
TARGET_LIB := libvs_bo_helper.so
CFLAGS :=  -Wall -Wextra -Werror -fPIC
CC=$(CROSS_COMPILE)gcc
AR=$(CROSS_COMPILE)ar

INCS := -I$(DRM_DRIVER_INC_DIR)/  -I$(PWD)/include
vpath %.c src
SRCS := ${notdir ${wildcard src/*.c}}

INSTALL_DIR=$(PWD)/sdk
BUILD_DIR=out

all : $(BUILD_DIR)/$(TARGET_LIB)

clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf $(INSTALL_DIR)

install: all
	@mkdir -p $(INSTALL_DIR)/lib
	@mkdir -p $(INSTALL_DIR)/include
	@cp -f $(PWD)/include/vs_bo_helper.h $(INSTALL_DIR)/include
	@cp $(BUILD_DIR)/$(TARGET_LIB) $(INSTALL_DIR)/lib


OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

$(BUILD_DIR)/$(TARGET_LIB) : $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@


