TARGETS = clagent

$(TARGETS):
	make -C src/

install:
	make -C src/ install

clean:
	make -C src/ clean
