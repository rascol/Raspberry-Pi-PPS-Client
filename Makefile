
all:
	mkdir pkg
	mkdir tmp
	
	cd ./build && $(MAKE) all
	cp ./build/pps-client ./pkg/pps-client
	
	cd ./driver && $(MAKE) all
	cp ./driver/pps-client.ko ./pkg/pps-client.ko
	
	cp -r ./installer/install-head/. ./tmp
	cd ./tmp && $(MAKE) all
	cp ./tmp/pps-client-install-hd ./installer/pps-client-install-hd
	find ./tmp -type f -delete
	
	cp -r ./installer/stop/. ./tmp
	cd ./tmp && $(MAKE) all
	cp ./tmp/pps-client-stop ./pkg/pps-client-stop
	find ./tmp -type f -delete
	
	cp -r ./installer/remove/. ./tmp
	cd ./tmp && $(MAKE) all
	cp ./tmp/pps-client-remove ./pkg/pps-client-remove
	find ./tmp -type f -delete
	
	cp -r ./installer/make-install/. ./tmp
	cd ./tmp && $(MAKE) all
	cp ./tmp/pps-client-make-install ./installer/pps-client-make-install
	find ./tmp -type f -delete
	
	cd ./utils && $(MAKE) all
	cp ./utils/interrupt-timer ./pkg/interrupt-timer
	
	cp -r ./utils/driver/. ./tmp
	cd ./tmp && $(MAKE) all
	cp ./tmp/interrupt-timer.ko ./pkg/interrupt-timer.ko
	find ./tmp -type f -delete
	
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
	
	rm -rf ./tmp
#	rm -rf ./pkg

clean:
	rm -rf ./pkg
	rm -rf ./tmp
	
	cd ./build && $(MAKE) clean
	cd ./driver && $(MAKE) clean
	cd ./utils && $(MAKE) clean
		
	rm ./installer/pps-client-install-hd
	rm ./installer/pps-client-make-install
	
		