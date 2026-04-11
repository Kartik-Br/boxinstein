########################################################################
# LIBRARY SOURCES - MUST BE IN THE SAME FOLDER as main.c (DO NOT CHANGE)
########################################################################

# Set mylib folder path.
# Do not change the MYLIB_PATH variable name.
# ONLY use relative file paths that start with $(SOURCELIB_ROOT)../
# DO NOT USE absolute file paths (e.g. /home/users/myuser/mydir)
# e.g. MYLIBPATH=$(SOURCELIB_ROOT)/../mylib
MYLIB_PATH=./lib

# Set folder path with header files to include.
# ONLY use relative file paths that start with $(SOURCELIB_ROOT)../
# DO NOT USE absolute file paths (e.g. /home/users/myuser/mydir)
CFLAGS += -I$(MYLIB_PATH)

# List all c file locations that must be included (use space as separator
# e.g. LIBSRCS += path_to/file1.c path_to/file2.c)
# ONLY use relative file paths that start with $(SOURCELIB_ROOT)../
# DO NOT USE absolute file paths (e.g. /home/users/myuser/mydir)
ifdef USE_ILI9488
    CFLAGS  += -DUSE_ILI9488 -I$(MYLIB_PATH2)
    LIBSRCS += $(MYLIB_PATH)/accel.c $(MYLIB_PATH)/3dEngine.c $(MYLIB_PATH)/render.c
    LIBSRCS += $(MYLIB_PATH2)/stm32_adafruit_lcd.c $(MYLIB_PATH2)/ili9488.c \
               $(MYLIB_PATH2)/lcd_io_spi.c $(MYLIB_PATH2)/font24.c \
               $(MYLIB_PATH2)/font8.c

    # Compile the 9488 render.c separately to avoid the name clash with lib/render.c
    $(shell mkdir -p obj)
    obj/render9488.o: $(MYLIB_PATH2)/render.c
	    arm-none-eabi-gcc $(CFLAGS) -c $< -o $@

    LIBOBJS += obj/render9488.o
else
    LIBSRCS += $(MYLIB_PATH)/accel.c $(MYLIB_PATH)/ili9341.c \
               $(MYLIB_PATH)/render.c $(MYLIB_PATH)/3dEngine.c
endif
