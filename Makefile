#export CROSS_COMPILE=~/Dropbox/RPi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-
#export KERNELDIR=~/Dropbox/RPi/Linux_kernels/linux-4.1.19
#export KERNELVERS=4.1.19-v7+

#export KERNELDIR=~/Dropbox/RPi/Linux_kernels/linux-rt-rpi-3.18.9
#export KERNELVERS=3.18.9-rt5-v7+


all:
	mkdir pkg
	
	cd ./build && $(MAKE) all
	cp ./build/pps-client ./pkg/pps-client
	
	cd ./driver && $(MAKE) all
	cp ./driver/pps-client.ko ./pkg/pps-client.ko
	
	mv ./utils/install-head ./
	cd ./install-head && $(MAKE) all
	mv ./install-head ./utils
	cp ./utils/install-head/pps-client-install-hd ./utils/pps-client-install-hd
	
	mv ./utils/remove ./
	cd ./remove && $(MAKE) all
	mv ./remove ./utils
	cp ./utils/remove/pps-client-remove ./pkg/pps-client-remove
	
	mv ./utils/make-install ./
	cd ./make-install && $(MAKE) all
	mv ./make-install ./utils
	cp ./utils/make-install/pps-client-make-install ./utils/pps-client-make-install
	
	cp ./build/pps-client.conf ./pkg/pps-client.conf
	cp ./build/pps-client.sh ./pkg/pps-client.sh
	tar czf pkg.tar.gz ./pkg
	./utils/pps-client-make-install $(KERNELVERS)
	rm pkg.tar.gz
	rm -rf ./pkg

clean:
	rm -rf ./pkg
	
	cd ./build && $(MAKE) clean
	cd ./driver && $(MAKE) clean
	
	mv ./utils/install-head ./
	cd ./install-head && $(MAKE) clean
	mv ./install-head ./utils
	
	mv ./utils/remove ./
	cd ./remove && $(MAKE) clean
	mv ./remove ./utils
	
	mv ./utils/make-install ./
	cd ./make-install && $(MAKE) clean
	mv ./make-install ./utils
	
	rm ./utils/pps-client-install-hd
	rm ./utils/pps-client-make-install
		