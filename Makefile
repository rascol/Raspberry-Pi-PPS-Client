
all:
	mkdir pkg
	
	cd ./build && $(MAKE) all
	cp ./build/pps-client ./pkg/pps-client
	
	cd ./driver && $(MAKE) all
	cp ./driver/pps-client.ko ./pkg/pps-client.ko
	
	mv ./installer/install-head ./
	cd ./install-head && $(MAKE) all
	mv ./install-head ./installer
	cp ./installer/install-head/pps-client-install-hd ./installer/pps-client-install-hd
	
	mv ./installer/stop ./
	cd ./stop && $(MAKE) all
	mv ./stop ./installer
	cp ./installer/stop/pps-client-stop ./pkg/pps-client-stop
	
	mv ./installer/remove ./
	cd ./remove && $(MAKE) all
	mv ./remove ./installer
	cp ./installer/remove/pps-client-remove ./pkg/pps-client-remove
	
	mv ./installer/make-install ./
	cd ./make-install && $(MAKE) all
	mv ./make-install ./installer
	cp ./installer/make-install/pps-client-make-install ./installer/pps-client-make-install
	
	cp ./README.md ./pkg/README.md
	cp ./README.html ./pkg/README.html
	cp ./figures/RPi_with_GPS.jpg ./pkg/RPi_with_GPS.jpg
	cp ./figures/frequency-vars.png ./pkg/frequency-vars.png
	cp ./figures/offset-distrib.png ./pkg/offset-distrib.png
	cp ./figures/StatusPrintoutAt10Min.png ./pkg/StatusPrintoutAt10Min.png
	cp ./figures/StatusPrintoutOnStart.png ./pkg/StatusPrintoutOnStart.png
	
	cp ./build/pps-client.conf ./pkg/pps-client.conf
	cp ./build/pps-client.sh ./pkg/pps-client.sh
	tar czf pkg.tar.gz ./pkg
	./installer/pps-client-make-install $(KERNELVERS)
	rm pkg.tar.gz
	rm -rf ./pkg

clean:
	rm -rf ./pkg
	
	cd ./build && $(MAKE) clean
	cd ./driver && $(MAKE) clean
	
	mv ./installer/install-head ./
	cd ./install-head && $(MAKE) clean
	mv ./install-head ./installer
	
	mv ./installer/stop ./
	cd ./stop && $(MAKE) clean
	mv ./stop ./installer
	
	mv ./installer/remove ./
	cd ./remove && $(MAKE) clean
	mv ./remove ./installer
	
	mv ./installer/make-install ./
	cd ./make-install && $(MAKE) clean
	mv ./make-install ./installer
	
	rm ./installer/pps-client-install-hd
	rm ./installer/pps-client-make-install
		