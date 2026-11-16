# 將 RACK_DIR 指向 Rack SDK 解壓後的資料夾，例如：
#   make RACK_DIR=~/Rack-SDK install
RACK_DIR ?= ../Rack-SDK

FLAGS += -Isrc
CFLAGS +=
CXXFLAGS +=
LDFLAGS +=

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += README.md
DISTRIBUTABLES += CHANGELOG.md

include $(RACK_DIR)/plugin.mk
