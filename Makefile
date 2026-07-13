# ##############################################################################
#  Makefile for the dmheap module
#
#     This Makefile is used to build the dmheap module library
#
#   DONT EDIT THIS FILE - it is automatically generated. 
#   Edit the scripts/Makefile-lib.in file instead.
#       
# ##############################################################################
ifeq ($(DMOD_DIR),)
    DMOD_DIR = build/_deps/dmod-src
endif

#
#  Name of the module
#
DMOD_LIB_NAME=libdmheap.a
DMOD_SOURCES=src/dmheap.c
DMOD_INC_DIRS = include\
		$(DMOD_DIR)/inc\
		build/_deps/dmod-build
DMOD_LIBS = dmod_inc
DMOD_GEN_HEADERS_IN = 
DMOD_DEFINITIONS = $<$<BOOL:OFF>:DMHEAP_DONT_IMPLEMENT_DMOD_API>;DMHEAP_VERSION="1.0"

# -----------------------------------------------------------------------------
# 	Initialization of paths
# -----------------------------------------------------------------------------
include $(DMOD_DIR)/paths.mk

# -----------------------------------------------------------------------------
# 	Including the template for the library
# -----------------------------------------------------------------------------
DMOD_LIB_OBJS_DIR = $(DMOD_OBJS_DIR)/dmheap
include $(DMOD_SLIB_FILE_PATH)
