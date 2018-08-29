# Copyright (c) 2015 Cisco and/or its affiliates.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Scripts require non-POSIX parts of bash
SHELL := /bin/bash

DPDK_BUILD_DIR        ?= $(CURDIR)/_build
DPDK_INSTALL_DIR      ?= $(CURDIR)/_install
DPDK_PKTMBUF_HEADROOM ?= 128
DPDK_CACHE_LINE_SIZE  ?= 64
DPDK_DOWNLOAD_DIR     ?= $(HOME)/Downloads
DPDK_DEBUG            ?= n
DPDK_MLX4_PMD         ?= n
DPDK_MLX5_PMD         ?= n

B := $(DPDK_BUILD_DIR)
I := $(DPDK_INSTALL_DIR)
DPDK_VERSION ?= 18.02
PKG_SUFFIX ?= vpp1
DPDK_BASE_URL ?= http://fast.dpdk.org/rel
DPDK_TARBALL := dpdk-$(DPDK_VERSION).tar.xz
DPDK_TAR_URL := $(DPDK_BASE_URL)/$(DPDK_TARBALL)
DPDK_17.11_TARBALL_MD5_CKSUM := 53ee9e054a8797c9e67ffa0eb5d0c701
DPDK_18.02_TARBALL_MD5_CKSUM := ca13077a014a2102c6e10153dfa3b920
DPDK_SOURCE := $(B)/dpdk-$(DPDK_VERSION)
MACHINE=$(shell uname -m)

# /proc/cpuinfo does not exist on platforms without a /proc and on some
# platforms, notably inside containers, it has no content. In those cases
# we assume there's 1 processor; we use 2*ncpu for the -j option.
# NB: GNU Make 4.2 will let us use '$(file </proc/cpuinfo)' to both test
# for file presence and content; for now this will have to do.
JOBS := $(if $(shell [ -f /proc/cpuinfo ] && head /proc/cpuinfo),\
	$(shell grep -c ^processor /proc/cpuinfo), 2)


all: build

$(CURDIR)/$(DPDK_TARBALL):
	@if [ -e $(DPDK_DOWNLOAD_DIR)/$(DPDK_TARBALL) ] ; \
		then cp $(DPDK_DOWNLOAD_DIR)/$(DPDK_TARBALL) $(CURDIR) ; \
		else curl -o $(CURDIR)/$(DPDK_TARBALL) -LO $(DPDK_TAR_URL) ; \
	fi
	@rm -f $(B)/.download.ok

DPDK_DOWNLOADS = $(CURDIR)/$(DPDK_TARBALL)

$(B)/.download.ok: $(DPDK_DOWNLOADS)
	@mkdir -p $(B)
	@openssl md5 $< | cut -f 2 -d " " - > $(B)/$(DPDK_TARBALL).md5sum
	@([ "$$(<$(B)/$(DPDK_TARBALL).md5sum)" = "$(DPDK_$(DPDK_VERSION)_TARBALL_MD5_CKSUM)" ] || \
	( echo "Bad Checksum! Please remove $< and retry" && \
		rm $(B)/$(DPDK_TARBALL).md5sum && false ))
	@touch $@

.PHONY: download
download: $(B)/.download.ok

.PHONY: clean
clean:
	@rm -rf $(B) $(I)

