.PHONY: release-xex test benchmark test-rom
release-xex:
	sh build-local.sh

test:
	sh build-local.sh
	python3 tests/transport-test.py
	python3 tests/proceed-test.py
	python3 tests/appkey-bounds-test.py
	python3 tests/run-tests.py
	python3 tests/ui-layout-test.py
	python3 tests/receive-test.py
	python3 tests/session-test.py
	python3 tests/timing-test.py
	python3 tests/keyboard-test.py
	python3 tests/keyboard-race-test.py candidate

test-rom:
	sh build-local.sh
	python3 tests/continuous-receive-test.py "$(XL_OS_LISTING)"

benchmark:
	python3 tests/render-benchmark.py
