<!--
#
# Alan Graves <agraves@gemcore.com>	
# 2017-06-14
#
-->

# Introduction

winlib: A Simple Windows Development Library

This code started out a long time ago as an embedded runtime library that
ran on a 80x86 (AMD MCUs), DOS and later on early Windows (16/32bit). The 
basic classes formed a common framework that I found myself repeatedly 
porting during the development of various resource-constrained systems. 
There has always been a minimalistic interface to the underlying hardware 
because only the essential peripherals common to all platforms was supported.

# Overview 

The goal is to make it easy to develop applications for a typical operating
system environment, processor or microcontroller environment. Currently the 
code is for a Windows 10 platform, however it should be relatively easy to 
port to other platforms such as Linux or ARM Cortex Mx as needed.

The following functionality is supported:

* Pre-emptive active objects:
 - Thread (Suspend, Resume, Sleep)
 - Mutex (Acquire, Release)
 - Lock
 - Event (Release, Wait)
 - TrafficLight (GreenLight, RedLight, Wait)
* Timer (Periodic or Single Flags or Functions)
* Serial communications
* File I/O (Basic)
* Circular buffer
* Console printf 
* Simple SCRIPT language parser/compiler

For more information on the Mynewt OS, please visit our website [here](https://mynewt.apache.org/).
If you'd like to get started, visit the [Quick Start Guide](http://mynewt.apache.org/quick-start/).

# Testing

In addition to the core classes, there is a simple Windows Console application (winlib.cpp) that 
is for demonstrating some of the basic functionality.

# Roadmap

winlib is being actively developed and used on projects. Some of the features being considered 
for the future are as follows:

* Simple SCRIPT interpreter
* Console input/output
* Proto Threads for non-OS use
* Database

# Suggestions

If you are would like to suggest a new feature, please send an email with a description 
of what you would like to see.

# Bugs


# License

See the LICENSE file for more information.

