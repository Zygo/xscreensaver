# Get this value from android/Makefile
APP_ABI := $(shell echo $$APP_ABI)
APP_STL := c++_static
APP_PLATFORM := android-16
# ^^ APP_PLATFORM is minimum API version supported
# https://android.googlesource.com/platform/ndk/+/master/docs/user/common_problems.md#target-api-set-higher-than-device-api

