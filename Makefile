# Shared platform entrypoint for KCC.
# Host default is cross; native staging selects daimos.
PLATFORM ?= cross
include $(PLATFORM).mk
