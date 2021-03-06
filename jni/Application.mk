#APP_ABI := arm64-v8a armeabi-v7a armeabi mips64 mips x86 x86_64
APP_ABI := armeabi-v7a
#APP_ABI := all
APP_OPTIM := debug    # Build the target in debug mode.
#APP_ABI := armeabi-v7a # Define the target architecture to be ARM.
APP_STL := stlport_static # We use stlport as the standard C/C++ library.
APP_CPPFLAGS := -frtti -fexceptions    # This is the place you enable exception.
APP_PLATFORM := android-19 # Define the target Android version of the native application.