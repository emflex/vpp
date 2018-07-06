GTP-U User Plane Function (UPF) based on VPP
============================================

For main VPP README, skip to the next chapter.

The GTP-UP plugins implements a GTP-U user plane based on 3GPP TS 23.214 and
3GPP TS 29.244 version 14.1.

Note: 3GPP Version 14.2+ changed the binary format of the PFCP protocol. The
      plugin has not yet been updated to that format.

Working features:

* PFCP decoding of most (but not all IEs)
* PFCP heartbeat
* PFCP session related messages
* Uplink and Downlink Packet Detection Rules (PDR) and
  Forward Action Rules (FAR) -- (some parts)
* IPv4 -- inner and outer

Untested

* IPv6 -- inner and outer

No yet working:

* PFCP node related messages (except heartbeat)
* Usage Reporting Rules (URR)
* Buffer Action Rules (BAR)
* QoS Enforcement Rule (QER)
* Sx Session Reports

Limitations of PDR's and FAR's:

* multiple network instance are not supported
* only the default VRF is supported
* FAR action with destination CP or LI are not implemented
* Uplink SDF's are not checked/applied

General limitations and known deficits:

* Error handling in Sx procedures is weak
* processing of Session Releated Procedures leaks memory from the messages
  and might leak memory from applying the rules to the session
* Session Deletion might leak memory

Vector Packet Processing
========================

## Introduction

The VPP platform is an extensible framework that provides out-of-the-box
production quality switch/router functionality. It is the open source version
of Cisco's Vector Packet Processing (VPP) technology: a high performance,
packet-processing stack that can run on commodity CPUs.

The benefits of this implementation of VPP are its high performance, proven
technology, its modularity and flexibility, and rich feature set.

For more information on VPP and its features please visit the
[FD.io website](http://fd.io/) and
[What is VPP?](https://wiki.fd.io/view/VPP/What_is_VPP%3F) pages.


## Changes

Details of the changes leading up to this version of VPP can be found under
@ref release_notes.


## Directory layout

Directory name         | Description
---------------------- | -------------------------------------------
     build-data        | Build metadata
     build-root        | Build output directory
     doxygen           | Documentation generator configuration
     dpdk              | DPDK patches and build infrastructure
@ref extras/libmemif   | Client library for memif
@ref src/examples      | VPP example code
@ref src/plugins       | VPP bundled plugins directory
@ref src/svm           | Shared virtual memory allocation library
     src/tests         | Standalone tests (not part of test harness)
     src/vat           | VPP API test program
@ref src/vlib          | VPP application library
@ref src/vlibapi       | VPP API library
@ref src/vlibmemory    | VPP Memory management
@ref src/vlibsocket    | VPP Socket I/O
@ref src/vnet          | VPP networking
@ref src/vpp           | VPP application
@ref src/vpp-api       | VPP application API bindings
@ref src/vppinfra      | VPP core library
@ref src/vpp/api       | Not-yet-relocated API bindings
     test              | Unit tests and Python test harness

## Getting started

In general anyone interested in building, developing or running VPP should
consult the [VPP wiki](https://wiki.fd.io/view/VPP) for more complete
documentation.

In particular, readers are recommended to take a look at [Pulling, Building,
Running, Hacking, Pushing](https://wiki.fd.io/view/VPP/Pulling,_Building,_Run
ning,_Hacking_and_Pushing_VPP_Code) which provides extensive step-by-step
coverage of the topic.

For the impatient, some salient information is distilled below.


### Quick-start: On an existing Linux host

To install system dependencies, build VPP and then install it, simply run the
build script. This should be performed a non-privileged user with `sudo`
access from the project base directory:

    ./extras/vagrant/build.sh

If you want a more fine-grained approach because you intend to do some
development work, the `Makefile` in the root directory of the source tree
provides several convenience shortcuts as `make` targets that may be of
interest. To see the available targets run:

    make


### Quick-start: Vagrant

The directory `extras/vagrant` contains a `VagrantFile` and supporting
scripts to bootstrap a working VPP inside a Vagrant-managed Virtual Machine.
This VM can then be used to test concepts with VPP or as a development
platform to extend VPP. Some obvious caveats apply when using a VM for VPP
since its performance will never match that of bare metal; if your work is
timing or performance sensitive, consider using bare metal in addition or
instead of the VM.

For this to work you will need a working installation of Vagrant. Instructions
for this can be found [on the Setting up Vagrant wiki page]
(https://wiki.fd.io/view/DEV/Setting_Up_Vagrant).


## More information

Several modules provide documentation, see @subpage user_doc for more
end-user-oriented information. Also see @subpage dev_doc for developer notes.

Visit the [VPP wiki](https://wiki.fd.io/view/VPP) for details on more
advanced building strategies and other development notes.


## Test Framework

There is PyDoc generated documentation available for the VPP test framework.
See @ref test_framework_doc for details.



### Build and Upload Docker VPP Image 

Build and Upload VPP Docker Image comprise of three parts: 

1. GitHub :  Used for version control of VPP code.
2. GitLab :  Used for build and upload VPP Docker image.
3. Docker Hub Registry: Used to store VPP Docker image.  


Build and Upload Pipeline. 

1. Commit on https://github.com/travelping/vpp repository will be mirrored to the https://gitlab.com/travelping/vpp-github and triger CI/CD PipeLine on GitLab. Mirroring (Sync) between GitHub and GitLab occurs automatically every 30 minutes or sync process can be triggered manually via the GitLab. 
2. GitLab CI/CD will trigger build and upload of VPP Docker image to the GitHub Registry.
3. After Successful build image will be uploaded here:  https://hub.docker.com/r/ergw/vpp/


Build and Upload steps are described in .gitlab-ci.yml, this file is stored in root directory of this repository.


