# Get this value from android/Makefile
APP_ABI := $(shell echo $$APP_ABI)
APP_STL := stlport_static
APP_PLATFORM := android-14
# ^^ APP_PLATFORM is minimum API version supported
# https://android.googlesource.com/platform/ndk/+/master/docs/user/common_problems.md#target-api-set-higher-than-device-api

