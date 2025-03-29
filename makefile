# Compiler settings
CXX ?= g++
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g -Wall -Wextra -pedantic
else
    CXXFLAGS += -O2
endif

# Include paths
INCLUDES = -I. \
           -I./timer \
           -I./http \
           -I./log \
           -I./cgi_mysql \
           -I./webserver \
           -I./threadpool \
           -I./lock \
           -I./config 

# Library paths and flags
LDFLAGS = -lpthread -lmysqlclient

# Source files
SRCS = main.cpp \
       ./timer/timer.cpp \
       ./http/http_connection.cpp \
       ./http/http_types.cpp \
       ./log/log.cpp \
       ./cgi_mysql/connection_pool.cpp \
       ./webserver/webserver.cpp \
       ./config/config.cpp
# Output executable
TARGET = server

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean