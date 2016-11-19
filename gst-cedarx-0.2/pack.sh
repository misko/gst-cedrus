#!/bin/bash
#/home/vagrant/baidu/a20/lichee/buildroot/dl/gst-cedarx-0.2.tar.gz
make clean
rm -rf /home/vagrant/baidu/a20/lichee/out/linux/common/buildroot/build/gst-cedarx-0.2/
rm -rf /home/vagrant/baidu/a20/lichee/buildroot/dl/gst-cedarx-0.2.tar.gz
pushd /home/vagrant/gst-cedrus/
rm /tmp/gst-cedarx-0.2.tar.gz
tar -zcf /tmp/gst-cedarx-0.2.tar.gz gst-cedarx-0.2
popd
cp /tmp/gst-cedarx-0.2.tar.gz /home/vagrant/baidu/a20/lichee/buildroot/dl/gst-cedarx-0.2.tar.gz
