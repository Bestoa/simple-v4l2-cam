default:
	-mkdir -p out
	-mkdir -p image
	gcc src/*.c -o out/cam_test
clean:
	-rm -rf out
	-rm -rf image

