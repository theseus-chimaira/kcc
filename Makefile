# Shared platform entrypoint for KCC.
# Host default is unix; native staging changes it to daimos.
PLATFORM ?= unix
include $(PLATFORM).mk
