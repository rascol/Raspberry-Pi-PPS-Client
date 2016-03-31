
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
	
	mv ./utils/stop ./
	cd ./stop && $(MAKE) all
	mv ./stop ./utils
	cp ./utils/stop/pps-client-stop ./pkg/pps-client-stop
	
	mv ./utils/remove ./
	cd ./remove && $(MAKE) all
	mv ./remove ./utils
	cp ./utils/remove/pps-client-remove ./pkg/pps-client-remove
	
	mv ./utils/make-install ./
	cd ./make-install && $(MAKE) all
	mv ./make-install ./utils
	cp ./utils/make-install/pps-client-make-install ./utils/pps-client-make-install
	
	cp ./README.md ./pkg/README.md
	cp ./README.html ./pkg/README.html
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
	
	mv ./utils/stop ./
	cd ./stop && $(MAKE) clean
	mv ./stop ./utils
	
	mv ./utils/remove ./
	cd ./remove && $(MAKE) clean
	mv ./remove ./utils
	
	mv ./utils/make-install ./
	cd ./make-install && $(MAKE) clean
	mv ./make-install ./utils
	
	rm ./utils/pps-client-install-hd
	rm ./utils/pps-client-make-install
		