<!--
#
# winlib: A simple Windows development library
#
# Alan Graves <agraves@gemcore.com>	
# 2017-06-14
#
# This code started out a long time ago as an embedded x86 runtime library.
# The basic classes developed formed a common framework that I found myself
# repeatedly using to development various resource-constrained systems. 
# There is a minimalistic interface to the underlying hardware and only the
# essential peripheral components common to all platforms is supported.
#
-->

# Overview 

It's goal is to make it easy to develop applications for any typical 
microcontroller environment. It currently supports only Windows platforms,
however it should be relatively easy to port to other platforms as needed.

The following functionality is supported:

* Active objects:
** Thread (Suspend, Resume, Sleep)
** Mutex (Acquire, Release)
** Lock
** Event (Release, Wait)
** TrafficLight (GreenLight, RedLight, Wait)
* Timers
* Serial I/O
* File I/O (Basic)
* Circular buffer
* Printf
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
* Proto Threads for non-OS use
* Console interface
* Database

# Suggestions

If you are would like to suggest a new feature, please send an email with a description 
of what you would like to see.

# Bugs


# License

See the LICENSE file for more information.

