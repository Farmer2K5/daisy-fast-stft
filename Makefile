# Daisy Patch SM Project
TARGET = SFFT_TestBed

# Sources
CPP_SOURCES = main_mono.cpp
# CPP_SOURCES = main_stereo.cpp

# Path to DaisySP and LibDaisy 
DAISYSP_DIR ?= ./lib/DaisySP
LIBDAISY_DIR ?= ./lib/libDaisy

# Path to CMSIS-DSP and CMSIS-Core
CMSIS_PATH = ./lib/CMSIS-DSP/
CMSIS_CORE_PATH = ./lib/CMSIS_5/CMSIS/Core

# Include the main source files for each function category
CMSIS_SOURCES = $(CMSIS_PATH)/Source/TransformFunctions/TransformFunctions.c \
                $(CMSIS_PATH)/Source/CommonTables/CommonTables.c \
                $(CMSIS_PATH)/Source/BasicMathFunctions/BasicMathFunctions.c \
                $(CMSIS_PATH)/Source/SupportFunctions/SupportFunctions.c \
                $(CMSIS_PATH)/Source/FastMathFunctions/FastMathFunctions.c \
                $(CMSIS_PATH)/Source/FilteringFunctions/FilteringFunctions.c \
                $(CMSIS_PATH)/Source/StatisticsFunctions/StatisticsFunctions.c \
                $(CMSIS_PATH)/Source/ControllerFunctions/ControllerFunctions.c \
                $(CMSIS_PATH)/Source/ComplexMathFunctions/ComplexMathFunctions.c \

C_SOURCES = $(CMSIS_SOURCES)

# Location of Hardware Support File within the SDK
C_INCLUDES += -I$(CMSIS_PATH)/Include \
              -I$(CMSIS_PATH)/PrivateInclude \
              -I$(CMSIS_CORE_PATH)/Include


# (optional) Includes DaisySP-LGPL (like ReverbSc, etc.) source files within project.
#USE_DAISYSP_LGPL=1

# (optional) Includes FatFS source files within project.
#USE_FATFS = 1

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
