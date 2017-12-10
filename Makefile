

help:
	echo "This is the 8bkc SDK. Please use this in one of your projects as indicated in the documentation."


test:
	make -C ${IDF_PATH}/tools/unit-test-app EXTRA_COMPONENT_DIRS=/path/to/my_proj/components TEST_COMPONENTS="8bkc-hal appfs gui-util ugui"
