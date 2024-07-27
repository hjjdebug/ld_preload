all: test libpreload_udpsend.so

libpreload_udpsend.so : ld_preload_udpsend.c
	gcc -fPIC -shared -g -o libpreload_udpsend.so ld_preload_udpsend.c -ldl
# -no-pie 保证生成固定加载地址, 方便调试, 可以用nm 查看地址确认
test : test.c 
	gcc -g -no-pie -o test  test.c


clean:
	rm libpreload_udpsend.so test
