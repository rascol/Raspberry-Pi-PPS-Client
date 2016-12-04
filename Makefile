VERSION = 1.2.0

all:
	mkdir pkg
	mkdir pkg/client
	mkdir pkg/client/figures
	mkdir tmp
	
	cd ./client && $(MAKE) all
	cp ./client/pps-client ./pkg/pps-client
	
	cd ./driver && $(MAKE) all
	cp ./driver/gps-pps-io.ko ./pkg/gps-pps-io.ko
	
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
		
	cp -r ./utils/interrupt-timer/. ./tmp
	cd ./tmp && $(MAKE) all
	cp ./tmp/interrupt-timer ./pkg/interrupt-timer
	find ./tmp -type f -delete

	cp -r ./utils/interrupt-timer/driver/. ./tmp		
	cd ./tmp && $(MAKE) all
	cp ./tmp/interrupt-timer.ko ./pkg/interrupt-timer.ko
	find ./tmp -type f -delete
	
	cp -r ./utils/pulse-generator/. ./tmp
	cd ./tmp && $(MAKE) all
	cp ./tmp/pulse-generator ./pkg/pulse-generator
	find ./tmp -type f -delete

	cp -r ./utils/pulse-generator/driver/. ./tmp		
	cd ./tmp && $(MAKE) all
	cp ./tmp/pulse-generator.ko ./pkg/pulse-generator.ko
	find ./tmp -type f -delete
	
	cp -r ./utils/NormalDistribParams/. ./tmp
	cd ./tmp && $(MAKE) all
	cp ./tmp/NormalDistribParams ./pkg/NormalDistribParams
	find ./tmp -type f -delete
	
	cp ./README.md ./pkg/README.md
	cp ./figures/RPi_with_GPS.jpg ./pkg/RPi_with_GPS.jpg
	cp ./figures/frequency-vars.png ./pkg/frequency-vars.png
	cp ./figures/offset-distrib.png ./pkg/offset-distrib.png
	cp ./figures/StatusPrintoutAt10Min.png ./pkg/StatusPrintoutAt10Min.png
	cp ./figures/StatusPrintoutOnStart.png ./pkg/StatusPrintoutOnStart.png
	cp ./figures/InterruptTimerDistrib.png ./pkg/InterruptTimerDistrib.png
	cp ./figures/SingleEventTimerDistrib.png ./pkg/SingleEventTimerDistrib.png
	cp ./figures/time.png ./pkg/time.png
	
	cp ./Doxyfile ./pkg/Doxyfile
	cp ./client/pps-client.md ./pkg/client/pps-client.md
	cp ./client/figures/accuracy_verify.jpg ./pkg/client/figures/accuracy_verify.jpg
	cp ./client/figures/interrupt-delay-comparison.png ./pkg/client/figures/interrupt-delay-comparison.png
	cp ./client/figures/InterruptTimerDistrib.png ./pkg/client/figures/InterruptTimerDistrib.png
	cp ./client/figures/jitter-spike.png ./pkg/client/figures/jitter-spike.png
	cp ./client/figures/pps-jitter-distrib.png ./pkg/client/figures/pps-jitter-distrib.png
	cp ./client/figures/pps-offsets-stress.png ./pkg/client/figures/pps-offsets-stress.png
	cp ./client/figures/pps-offsets-to-300.png ./pkg/client/figures/pps-offsets-to-300.png
	cp ./client/figures/pps-offsets-to-720.png ./pkg/client/figures/pps-offsets-to-720.png
	cp ./client/figures/StatusPrintoutAt10Min.png ./pkg/client/figures/StatusPrintoutAt10Min.png
	cp ./client/figures/StatusPrintoutOnStart.png ./pkg/client/figures/StatusPrintoutOnStart.png
	cp ./client/figures/wiring.png ./pkg/client/figures/wiring.png
	cp ./client/figures/interrupt-delay-comparison-RPi3.png ./pkg/client/figures/interrupt-delay-comparison-RPi3.png
	cp ./client/figures/pps-jitter-distrib-RPi3.png ./pkg/client/figures/pps-jitter-distrib-RPi3.png
	
	
	cp ./client/pps-client.conf ./pkg/pps-client.conf
	cp ./client/pps-client.sh ./pkg/pps-client.sh
	tar czf pkg.tar.gz ./pkg
	./installer/pps-client-make-install $(KERNELVERS)
	rm pkg.tar.gz
	
	rm -rf ./tmp

clean:
	rm -rf ./pkg
	rm -rf ./tmp
	
	cd ./client && $(MAKE) clean
	cd ./driver && $(MAKE) clean
	cd ./utils/interrupt-timer && $(MAKE) clean
	cd ./utils/pulse-generator && $(MAKE) clean
	cd ./utils/NormalDistribParams && $(MAKE) clean
		
	rm ./installer/pps-client-install-hd
	rm ./installer/pps-client-make-install
	
		