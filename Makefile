# Compiler settings
CXX ?= g++
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g -Wall -Wextra -pedantic
else
    CXXFLAGS += -O2
endif

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Include paths
INCLUDES = -I$(INC_DIR) \
           -I$(INC_DIR)/timer \
           -I$(INC_DIR)/http \
           -I$(INC_DIR)/log \
           -I$(INC_DIR)/cgi_mysql \
           -I$(INC_DIR)/webserver \
           -I$(INC_DIR)/threadpool \
           -I$(INC_DIR)/lock \
           -I$(INC_DIR)/config \
           -I${INC_DIR}/cache

# Library paths and flags
LDFLAGS = -lpthread -lmysqlclient

# Source files
SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/timer/timer.cpp \
       $(SRC_DIR)/http/http_connection.cpp \
       $(SRC_DIR)/http/http_types.cpp \
       $(SRC_DIR)/http/jsonparser.cpp \
       $(SRC_DIR)/log/log.cpp \
       $(SRC_DIR)/cgi_mysql/connection_pool.cpp \
       $(SRC_DIR)/webserver/webserver.cpp \
       $(SRC_DIR)/config/config.cpp \
       $(SRC_DIR)/cache/cache.cpp \
       $(SRC_DIR)/cache/memory_buffer_cache.cpp 

# Output executable
TARGET = $(BUILD_DIR)/server

all: $(TARGET)

$(TARGET): $(SRCS)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
