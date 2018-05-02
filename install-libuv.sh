#!/bin/sh -ex
git clone https://github.com/libuv/libuv.git
cd libuv && sh autogen.sh && ./configure --prefix=/usr && make && sudo make install
