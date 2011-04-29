CXX := mpic++
CC := mpic++

EXE := hemelb

HEMELB_DEBUG_LEVEL := 0
HEMELB_STEERING_LIB := $(or $(HEMELB_STEERING_LIB),basic)

PMETIS_INCLUDE_DIR := $(TOP)/parmetis
PMETIS_LIBRARY_DIR := $(TOP)/parmetis
HEMELB_CFLAGS :=
HEMELB_CXXFLAGS := -g -pedantic -Wall -Wextra -Wno-unused
HEMELB_DEFS := TIXML_USE_TICPP
HEMELB_INCLUDEPATHS = $(PMETIS_INCLUDE_DIR)
HEMELB_LIBPATHS = $(PMETIS_LIBRARY_DIR)
$(EXE)_LIBS = -lparmetis -lmetis
