#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    FILE* file = fopen(argv[1], "rb");
    if(!file) return -1;
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    printf("size is: %llu\n", size);
    fseek(file, 0, SEEK_SET);
    void* buff = malloc(1024 * 1024);
    fread(buff, 1, size, file);
    for(unsigned i = 0; i < size / sizeof(unsigned); i++) {
        printf("%u ", ((unsigned*)buff)[i]);
    }
    fclose(file);
    return 0;
}